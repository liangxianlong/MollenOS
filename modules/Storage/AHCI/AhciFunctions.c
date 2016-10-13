/* MollenOS
*
* Copyright 2011 - 2014, Philip Meulengracht
*
* This program is free software : you can redistribute it and / or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation ? , either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.If not, see <http://www.gnu.org/licenses/>.
*
*
* MollenOS MCore - Advanced Host Controller Interface Driver
*/

/* Includes */
#include "Ahci.h"

/* Additional Includes */
#include <DeviceManager.h>
#include <Scheduler.h>
#include <Heap.h>
#include <Timers.h>

/* CLib */
#include <stddef.h>
#include <string.h>

/* AHCICommandDispatch Flags 
 * Used to setup transfer flags */
#define DISPATCH_MULTIPLIER(Pmp)		(Pmp & 0xF)
#define DISPATCH_WRITE					0x10
#define DISPATCH_PREFETCH				0x20
#define DISPATCH_CLEARBUSY				0x40
#define DISPATCH_ATAPI					0x80

/* AHCICommandDispatch 
 * Dispatches a FIS command on a given port 
 * This function automatically allocates everything neccessary
 * for the transfer */
OsStatus_t AhciCommandDispatch(AhciController_t *Controller, AhciPort_t *Port, uint32_t Flags,
	void *Command, size_t CommandLength, void *AtapiCmd, size_t AtapiCmdLength, 
	void *Buffer, size_t BufferLength)
{
	/* Variables */
	AHCICommandTable_t *CmdTable = NULL;
	uint8_t *BufferPtr = NULL;
	size_t BytesLeft = BufferLength;
	int Slot = 0, PrdtIndex = 0;
	ListNode_t *tNode = NULL;
	DataKey_t Key, SubKey;

	/* Assert that buffer is DWORD aligned */
	if (((Addr_t)Buffer & 0x3) != 0) {
		LogFatal("AHCI", "AhciCommandDispatch::Buffer was not dword aligned", Port->Id);
		return OsError;
	}

	/* Assert that buffer length is an even byte-count requested */
	if ((BufferLength & 0x1) != 0) {
		LogFatal("AHCI", "AhciCommandDispatch::BufferLength is odd, must be even", Port->Id);
		return OsError;
	}

	/* Allocate a slot for this FIS */
	Slot = AhciPortAcquireCommandSlot(Controller, Port);

	/* Sanitize that there is room 
	 * for our command */
	if (Slot < 0) {
		LogFatal("AHCI", "Port %u was out of command slots!!", Port->Id);
		return OsError;
	}

	/* Get a reference to the command slot */
	CmdTable = (AHCICommandTable_t*)((uint8_t*)Port->CommandTable + (AHCI_COMMAND_TABLE_SIZE * Slot));

	/* Zero out the command table */
	memset(CmdTable, 0, AHCI_COMMAND_TABLE_SIZE);

	/* Sanitizie packet lenghts */
	if (CommandLength > 64
		|| AtapiCmdLength > 16) {
		LogFatal("AHCI", "Commands are exceeding the allowed length, FIS (%u), ATAPI (%u)", 
			CommandLength, AtapiCmdLength);
		goto Error;
	}

	/* Copy data over to packet */
	if (Command != NULL)
		memcpy(&CmdTable->FISCommand[0], Command, CommandLength); 
	if (AtapiCmd != NULL)
		memcpy(&CmdTable->FISAtapi[0], AtapiCmd, AtapiCmdLength);

	/* Build PRDT */
	BufferPtr = (uint8_t*)Buffer;
	while (BytesLeft > 0)
	{
		/* Get handler to prdt entry */
		AHCIPrdtEntry_t *Prdt = &CmdTable->PrdtEntry[PrdtIndex];

		/* Calculate how much to transfer */
		size_t TransferLength = MIN(AHCI_PRDT_MAX_LENGTH, BytesLeft);

		/* Get the physical address of buffer */
		Addr_t PhysicalAddr = AddressSpaceGetMap(AddressSpaceGetCurrent(), (VirtAddr_t)BufferPtr);

		/* Set buffer */
		Prdt->DataBaseAddress = LODWORD(PhysicalAddr);
		Prdt->DataBaseAddressUpper = (sizeof(void*) > 4) ? HIDWORD(PhysicalAddr) : 0;

		/* Set transfer length */
		Prdt->Descriptor = TransferLength - 1;

		/* Adjust pointer and length */
		BytesLeft -= TransferLength;
		BufferPtr += TransferLength;
		PrdtIndex++;

		/* Set IOC on last */
		if (BytesLeft == 0) {
			Prdt->Descriptor |= AHCI_PRDT_IOC;
		}
	}

	/* Update command table */
	Port->CommandList->Headers[Slot].TableLength = (uint16_t)PrdtIndex;
	Port->CommandList->Headers[Slot].Flags = (uint16_t)(CommandLength / 4);

	/* Set flags */
	if (Flags & DISPATCH_ATAPI)
		Port->CommandList->Headers[Slot].Flags |= (1 << 5);
	if (Flags & DISPATCH_WRITE)
		Port->CommandList->Headers[Slot].Flags |= (1 << 6);
	if (Flags & DISPATCH_PREFETCH)
		Port->CommandList->Headers[Slot].Flags |= (1 << 7);
	if (Flags & DISPATCH_CLEARBUSY)
		Port->CommandList->Headers[Slot].Flags |= (1 << 10);

	/* Update PMP */
	Port->CommandList->Headers[Slot].Flags |= (DISPATCH_MULTIPLIER(Flags) << 12);

	/* Setup Keys */
	Key.Value = Slot;
	SubKey.Value = (int)DISPATCH_MULTIPLIER(Flags);

	/* Add transaction to queue */
	tNode = ListCreateNode(Key, SubKey, NULL);
	ListAppend(Port->Transactions, tNode);

	/* Start command */
	AhciPortStartCommandSlot(Port, Slot);

	/* Start the sleep */
	SchedulerSleepThread((Addr_t*)tNode, 0);
	IThreadYield();

	/* Cleanup */
	AhciPortReleaseCommandSlot(Port, Slot);

	/* Done */
	return OsNoError;

Error:
	/* Cleanup */
	AhciPortReleaseCommandSlot(Port, Slot);

	/* Return error */
	return OsError;
}

/* AHCICommandRegisterFIS 
 * Builds a new AHCI Transaction based on a register FIS */
OsStatus_t AhciCommandRegisterFIS(AhciController_t *Controller, AhciPort_t *Port, 
	ATACommandType_t Command, uint64_t SectorLBA, size_t SectorCount, int Device, 
	int Write, int AddressingMode, void *Buffer, size_t BufferSize)
{
	/* Variables */
	FISRegisterH2D_t Fis;
	uint32_t Flags;

	/* Reset structure */
	memset((void*)&Fis, 0, sizeof(FISRegisterH2D_t));

	/* Fill in FIS */
	Fis.FISType = (uint8_t)FISRegisterH2D;
	Fis.Flags |= FIS_REGISTER_COMMAND;
	Fis.Command = (uint8_t)Command;
	Fis.Device = 0x40 | ((uint8_t)(Device & 0x1) << 4);

	/* Set CHS fields */
	if (AddressingMode == 0) 
	{
		/* Variables */
		//uint16_t Head = 0, Cylinder = 0, Sector = 0;

		/* Step 1 -> Transform LBA into CHS */

		/* Set CHS params */



		/* Set count */
		Fis.Count = LOBYTE(SectorCount);
	}
	else if (AddressingMode == 1
		|| AddressingMode == 2) {
		/* Set LBA28 params */
		Fis.SectorNo = LOBYTE(SectorLBA);
		Fis.SectorNoExtended = (uint8_t)((SectorLBA >> 8) & 0xFF);
		Fis.CylinderLow = (uint8_t)((SectorLBA >> 16) & 0xFF);
		Fis.CylinderLowExtended = (uint8_t)((SectorLBA >> 24) & 0xFF);

		/* If it's an LBA48, set LBA48 params */
		if (AddressingMode == 2) {
			Fis.CylinderHigh = (uint8_t)((SectorLBA >> 32) & 0xFF);
			Fis.CylinderHighExtended = (uint8_t)((SectorLBA >> 40) & 0xFF);

			/* Set count */
			Fis.Count = LOWORD(SectorCount);
		}
		else {
			/* Set count */
			Fis.Count = LOBYTE(SectorCount);
		}
	}

	/* Build flags */
	Flags = DISPATCH_MULTIPLIER(0);
	
	/* Is this an ATAPI? */
	if (Port->Registers->Signature == SATA_SIGNATURE_ATAPI) {
		Flags |= DISPATCH_ATAPI;
	}

	/* Write operation? */
	if (Write != 0) {
		Flags |= DISPATCH_WRITE;
	}

	/* Execute our command */
	return AhciCommandDispatch(Controller, Port, Flags, &Fis,
		sizeof(FISRegisterH2D_t), NULL, 0, Buffer, BufferSize);
}

/* AHCIDeviceIdentify 
 * Identifies the device and type on a port
 * and sets it up accordingly */
void AhciDeviceIdentify(AhciController_t *Controller, AhciPort_t *Port)
{
	/* Variables */
	ATAIdentify_t DeviceInformation;
	OsStatus_t Status;

	/* First of all, is this a port multiplier? 
	 * because then we should really enumerate it */
	if (Port->Registers->Signature == SATA_SIGNATURE_PM
		|| Port->Registers->Signature == SATA_SIGNATURE_SEMB) {
		LogDebug("AHCI", "Unsupported device type 0x%x on port %i",
			Port->Registers->Signature, Port->Id);
		return;
	}

	/* Ok, so either ATA or ATAPI */
	Status = AhciCommandRegisterFIS(Controller, Port, AtaPIOIdentifyDevice,
		0, 0, 0, 0, -1, (void*)&DeviceInformation, sizeof(ATAIdentify_t));

	/* So, how did it go? */
	if (Status != OsNoError) {
		LogFatal("AHCI", "AHCIDeviceIdentify:: Failed to send Identify");
		return;
	}

	/* Safety */
	DeviceInformation.ModelNo[39] = '\0';

	/* Transform information */
	LogInformation("AHCI", "Drive Model: %s", &DeviceInformation.ModelNo[0]);
}