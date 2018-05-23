/* MollenOS
 *
 * Copyright 2018, Philip Meulengracht
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
 * MollenOS System Component Infrastructure 
 * - The Central Processing Unit Component is one of the primary actors
 *   in a domain. 
 */
#define __MODULE "CCPU"
#define __TRACE

#include <component/domain.h>
#include <component/cpu.h>
#include <system/interrupts.h>
#include <system/utils.h>
#include <assert.h>
#include <string.h>
#include <debug.h>
#include <heap.h>

/* Static per-cpu data
 * We do not need to support more than 256 cpus because of APIC id's on the x86 arch. */
static SystemCpuCore_t* CpuStorageTable[256] = { 0 };

/* InitializeProcessor
 * Initializes the cpu of an domain with the given parameters. */
void
InitializeProcessor(
    _In_ SystemCpu_t*       Cpu,
    _In_ const char*        Vendor,
    _In_ const char*        Brand,
    _In_ int                NumberOfCores,
    _In_ uintptr_t*         Data)
{
    // Debug
    TRACE("InitializeProcessor(%s, %s, %i)", Vendor, Brand, NumberOfCores);

    // Sanitize params
    assert(Cpu != NULL);
    assert(NumberOfCores > 0);
    assert(Vendor != NULL);
    assert(Brand != NULL);

    // Initialize the cpu
    memset((void*)Cpu, 0, sizeof(SystemCpu_t));
    memcpy(&Cpu->Vendor[0], Vendor, strnlen(Vendor, 15));
    memcpy(&Cpu->Brand[0],  Brand,  strnlen(Brand, 63));
    memcpy(&Cpu->Data[0],   Data,   4 * sizeof(uintptr_t));
    Cpu->NumberOfCores      = NumberOfCores;

    // Initialize the primary core
    Cpu->PrimaryCore.Id     = CpuGetCurrentId();
    Cpu->PrimaryCore.State  = CpuStateRunning;
    CpuStorageTable[Cpu->PrimaryCore.Id] = &Cpu->PrimaryCore;
}

/* RegisterApplicationCore
 * Registers a new cpu application core for the given cpu. The core count and the
 * application-core will be initialized on first call to this function. This also allocates a 
 * new instance of the cpu-core. */
void
RegisterApplicationCore(
    _In_ SystemCpu_t*       Cpu,
    _In_ UUId_t             CoreId,
    _In_ SystemCpuState_t   InitialState,
    _In_ int                External)
{
    // Variables
    int i;

    // Sanitize params
    assert(Cpu != NULL);
    assert(Cpu->NumberOfCores > 1);

    // Make sure the array is allocated
    if(Cpu->ApplicationCores == NULL) {
        if (Cpu->NumberOfCores > 1) {
            Cpu->ApplicationCores   = (SystemCpuCore_t*)kmalloc(sizeof(SystemCpuCore_t) * (Cpu->NumberOfCores - 1));
            memset((void*)Cpu->ApplicationCores, 0, sizeof(SystemCpuCore_t) * (Cpu->NumberOfCores - 1));
            for (i = 0; i < (Cpu->NumberOfCores - 1); i++) {
                Cpu->ApplicationCores[i].Id     = UUID_INVALID;
                Cpu->ApplicationCores[i].State  = CpuStateUnavailable;
            }
        }
    }

    // Find it and update parameters for it
    for (i = 0; i < (Cpu->NumberOfCores - 1); i++) {
        if (Cpu->ApplicationCores[i].Id == UUID_INVALID) {
            Cpu->ApplicationCores[i].Id         = CoreId;
            Cpu->ApplicationCores[i].State      = InitialState;
            Cpu->ApplicationCores[i].External   = External;
            CpuStorageTable[CoreId]             = &Cpu->ApplicationCores[i];
            break;
        }
    }
}

/* ActivateApplicationCore 
 * Activates the given core and prepares it for usage. This sets up a new 
 * scheduler and initializes a new idle thread. This function does never return. */
void
ActivateApplicationCore(
    _In_ SystemCpuCore_t*   Core)
{
    // Notify everyone that we are running
    Core->State = CpuStateRunning;

    // Create the idle-thread and scheduler for the core
	SchedulerInitialize();
	ThreadingEnable();
    InterruptEnable();

    // Debug
    TRACE("Core %u is now active", GetCurrentProcessorCore()->Id);

    // Enter idle loop
	while (1) {
		CpuIdle();
    }
}

/* GetProcessorCore
 * Retrieves the cpu core from the given core-id. */
SystemCpuCore_t*
GetProcessorCore(
    _In_ UUId_t             CoreId)
{
    assert(CpuStorageTable[CoreId] != NULL);
    return CpuStorageTable[CoreId];
}

/* GetCurrentProcessorCore
 * Retrieves the cpu core that belongs to calling cpu core. */
SystemCpuCore_t*
GetCurrentProcessorCore(void)
{
    assert(CpuStorageTable[CpuGetCurrentId()] != NULL);
    return CpuStorageTable[CpuGetCurrentId()];
}
