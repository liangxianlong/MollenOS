/* MollenOS
 *
 * Copyright 2011 - 2017, Philip Meulengracht
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
 * MollenOS X86 PS2 Controller (Controller) Driver
 * http://wiki.osdev.org/PS2
 */

/* Includes 
 * - System */
#include <os/driver/contracts/base.h>
#include <os/mollenos.h>
#include "ps2.h"

/* Includes
 * - Library */
#include <stddef.h>
#include <string.h>
#include <stdlib.h>

/* Since there only exists a single ps2
 * chip on-board we keep some static information
 * in this driver */
static PS2Controller_t *GlbController = NULL;

/* PS2ReadStatus
 * Reads the status byte from the controller */
uint8_t PS2ReadStatus(void)
{
	return (uint8_t)ReadIoSpace(&GlbController->CommandSpace,
		PS2_REGISTER_STATUS, 1);
}

/* PS2Wait
 * Waits for the given flag to clear by doing a
 * number of iterations giving a fake-delay */
OsStatus_t PS2Wait(uint8_t Flags, int Negate)
{
	/* Wait for flag to clear */
	for (int i = 0; i < 1000; i++) {
		if (Negate == 1) {
			if (PS2ReadStatus() & Flags) {
				return OsNoError;
			}
		}
		else {
			if (!(PS2ReadStatus() & Flags)) {
				return OsNoError;
			}
		}	
	}

	/* Damn.. */
	return OsError;
}

/* PS2ReadData
 * Reads a byte from the PS2 controller data port */
uint8_t PS2ReadData(int Dummy)
{
	/* Hold response */
	uint8_t Response = 0;

	/* Only wait for input to be full in case
	 * we don't do dummy reads */
	if (Dummy == 0) {
		if (PS2Wait(PS2_STATUS_OUTPUT_FULL, 1) == OsError) {
			return 0xFF;
		}
	}

	/* Retrieve the data from data io space */
	Response = (uint8_t)ReadIoSpace(&GlbController->DataSpace, 
		PS2_REGISTER_DATA, 1);

	/* Done */
	return Response;
}

/* PS2WriteData
 * Writes a data byte to the PS2 controller data port */
OsStatus_t PS2WriteData(uint8_t Value)
{
	/* Sanitize buffer status */
	if (PS2Wait(PS2_STATUS_INPUT_FULL, 0) != OsNoError) {
		return OsError;
	}

	/* Write */
	WriteIoSpace(&GlbController->DataSpace, PS2_REGISTER_DATA, Value, 1);

	/* Done */
	return OsNoError;
}

/* PS2SendCommand
 * Writes the given command to the ps2-controller */
void PS2SendCommand(uint8_t Command)
{
	/* Wait for flag to clear, then write data */
	PS2Wait(PS2_STATUS_INPUT_FULL, 0);
	WriteIoSpace(&GlbController->CommandSpace, 
		PS2_REGISTER_COMMAND, Command, 1);
}

/* PS2SetScanning
 * Updates the enable/disable status of the port */
OsStatus_t PS2SetScanning(int Index, uint8_t Status)
{
	/* Always select port if neccessary */
	if (Index != 0) {
		PS2SendCommand(PS2_SELECT_PORT2);
	}

	/* Set sample rate to given value */
	if (PS2WriteData(Status) != OsNoError
		|| PS2ReadData(0) != PS2_ACK) {
		return OsError;
	}

	/* Wuhuu! */
	return OsNoError;
}

/* PS2SelfTest
 * Does 5 tries to perform a self-test of the
 * ps2 controller */
OsStatus_t PS2SelfTest(void)
{
	/* Variables */
	uint8_t Temp = 0;
	int i = 0;

	/* Iterate through 5 tries */
	for (; i < 5; i++) {
		PS2SendCommand(PS2_SELFTEST);
		Temp = PS2ReadData(0);
		if (Temp == PS2_SELFTEST_OK)
			break;
	}

	/* Yay ! */
	return (i == 5) ? OsError : OsNoError;
}

/* PS2Initialize 
 * Initializes the controller and initializes
 * the attached ports */
OsStatus_t PS2Initialize(void)
{
	/* Variables for initializing */
	OsStatus_t Status = 0;
	uint8_t Temp = 0;
	int i;

	/* Dummy reads, empty it's buffer */
	PS2ReadData(1);
	PS2ReadData(1);

	/* Disable Devices */
	PS2SendCommand(PS2_DISABLE_PORT1);
	PS2SendCommand(PS2_DISABLE_PORT2);

	/* Make sure it's empty, now devices cant fill it */
	PS2ReadData(1);

	/* Get Controller Configuration */
	PS2SendCommand(PS2_GET_CONFIGURATION);
	Temp = PS2ReadData(0);

	/* Discover port status 
	 * both ports should be disabled */
	GlbController->Ports[0].Enabled = 1;
	if (!(Temp & PS2_CONFIG_PORT2_DISABLED)) {
		GlbController->Ports[1].Enabled = 0;
	}
	else {
		/* This simply means we should test channel 2 */
		GlbController->Ports[1].Enabled = 1;
	}

	/* Clear all irqs and translations */
	Temp &= ~(PS2_CONFIG_PORT1_IRQ | PS2_CONFIG_PORT2_IRQ
		| PS2_CONFIG_TRANSLATION);

	/* Write back the configuration */
	PS2SendCommand(PS2_SET_CONFIGURATION);
	Status = PS2WriteData(Temp);

	/* Perform Self Test */
	if (Status != OsNoError 
		|| PS2SelfTest() != OsNoError) {
		//LogFatal("PS2C", "Ps2 Controller failed to initialize, giving up");
		return OsError;
	}

	/* Initialize the ports */
	for (i = 0; i < PS2_MAXPORTS; i++) {
		GlbController->Ports[i].Index = i;
		if (GlbController->Ports[i].Enabled == 1) {
			Status = PS2PortInitialize(&GlbController->Ports[i]);
			if (Status != OsNoError) {
				//LogFatal("PS2C", "Ps2 Controller failed to initialize port %i", i);
			}
		}
	}

	/* Done! */
	return OsNoError;
}

/* OnInterrupt
 * Is called when one of the registered devices
 * produces an interrupt. On successful handled
 * interrupt return OsNoError, otherwise the interrupt
 * won't be acknowledged */
InterruptStatus_t OnInterrupt(void *InterruptData)
{
	/* No further processing is needed */
	_CRT_UNUSED(InterruptData);
	return InterruptHandled;
}

/* OnLoad
 * The entry-point of a driver, this is called
 * as soon as the driver is loaded in the system */
OsStatus_t OnLoad(void)
{
	/* Allocate a new instance of the pit-data */
	GlbController = (PS2Controller_t*)malloc(sizeof(PS2Controller_t));
	memset(GlbController, 0, sizeof(PS2Controller_t));

	/* Create the io-spaces, again we have to create
	 * the io-space ourselves */
	GlbController->DataSpace.Type = IO_SPACE_IO;
	GlbController->DataSpace.PhysicalBase = PS2_IO_DATA_BASE;
	GlbController->DataSpace.Size = PS2_IO_LENGTH;

	GlbController->CommandSpace.Type = IO_SPACE_IO;
	GlbController->CommandSpace.PhysicalBase = PS2_IO_STATUS_BASE;
	GlbController->CommandSpace.Size = PS2_IO_LENGTH;

	/* Create both the io-spaces in system */
	if (CreateIoSpace(&GlbController->DataSpace) != OsNoError
		|| CreateIoSpace(&GlbController->CommandSpace) != OsNoError) {
		return OsError;
	}

	/* No problem, last thing is to acquire the
	 * io-spaces, and just return that as result */
	if (AcquireIoSpace(&GlbController->DataSpace) != OsNoError
		|| AcquireIoSpace(&GlbController->CommandSpace) != OsNoError) {
		return OsError;
	}

	/* Initialize the ps2-contract */
	InitializeContract(&GlbController->Controller, UUID_INVALID, 1,
		ContractTimer, "PS2 Controller Interface");

	/* Now initialize the controller */
	return PS2Initialize();
}

/* OnUnload
 * This is called when the driver is being unloaded
 * and should free all resources allocated by the system */
OsStatus_t OnUnload(void)
{
	/* Destroy the io-spaces we created */
	if (GlbController->CommandSpace.Id != 0) {
		ReleaseIoSpace(&GlbController->CommandSpace);
		DestroyIoSpace(GlbController->CommandSpace.Id);
	}

	if (GlbController->DataSpace.Id != 0) {
		ReleaseIoSpace(&GlbController->DataSpace);
		DestroyIoSpace(GlbController->DataSpace.Id);
	}

	/* Free up allocated resources */
	free(GlbController);

	/* Wuhuu */
	return OsNoError;
}

/* OnRegister
 * Is called when the device-manager registers a new
 * instance of this driver for the given device */
OsStatus_t OnRegister(MCoreDevice_t *Device)
{
	/* Variables */
	OsStatus_t Result = OsNoError;
	PS2Port_t *Port = NULL;

	/* First register call is the ps2-controller
	 * all sequent calls here is ps2-devices 
	 * So install the contract as soon as it arrives */
	if (GlbController->Controller.DeviceId == UUID_INVALID) {
		GlbController->Controller.DeviceId = Device->Id;
		return RegisterContract(&GlbController->Controller);
	}

	/* Select the port */
	if (GlbController->Ports[0].Contract.DeviceId == Device->Id) {
		Port = &GlbController->Ports[0];
	}
	else if (GlbController->Ports[1].Contract.DeviceId == Device->Id) {
		Port = &GlbController->Ports[1];
	}
	else {
		return OsError;
	}

	/* Ok .. It's a new device 
	 * - What kind of device? */
	if (Port->Signature == 0xAB41
		|| Port->Signature == 0xABC1) {
		/* MF2 Keyboard with translation enabled */
		Result = PS2KeyboardInitialize(Port, 1);
	}
	else if (Port->Signature == 0xAB83) {
		/* MF2 Keyboard */
		Result = PS2KeyboardInitialize(Port, 0);
	}
	else if (Port->Signature != 0xFFFFFFFF) {
		Result = PS2MouseInitialize(Port);
	}
	else {
		Result = OsError;
	}

	/* Done! */
	return Result;
}

/* OnUnregister
 * Is called when the device-manager wants to unload
 * an instance of this driver from the system */
OsStatus_t OnUnregister(MCoreDevice_t *Device)
{
	/* Variables */
	OsStatus_t Result = OsNoError;
	PS2Port_t *Port = NULL;

	/* Select the port */
	if (GlbController->Ports[0].Contract.DeviceId == Device->Id) {
		Port = &GlbController->Ports[0];
	}
	else if (GlbController->Ports[1].Contract.DeviceId == Device->Id) {
		Port = &GlbController->Ports[1];
	}
	else {
		/* Probably the controller itself */
		return OsError;
	}

	/* Ok .. It's an device
	 * - What kind of device? */
	if (Port->Signature == 0xAB41
		|| Port->Signature == 0xABC1) {
		/* MF2 Keyboard with translation enabled */
		Result = PS2KeyboardCleanup(Port);
	}
	else if (Port->Signature == 0xAB83) {
		/* MF2 Keyboard */
		Result = PS2KeyboardCleanup(Port);
	}
	else if (Port->Signature != 0xFFFFFFFF) {
		Result = PS2MouseCleanup(Port);
	}
	else {
		Result = OsError;
	}

	/* Done! */
	return Result;
}

/* OnQuery
 * Occurs when an external process or server quries
 * this driver for data, this will correspond to the query
 * function that is defined in the contract */
OsStatus_t OnQuery(MContractType_t QueryType, UUId_t QueryTarget, int Port)
{
	_CRT_UNUSED(QueryType);
	_CRT_UNUSED(QueryTarget);
	_CRT_UNUSED(Port);
	/* Done! */
	return OsNoError;
}
