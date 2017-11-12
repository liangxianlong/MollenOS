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
 * MollenOS MCore - Server & Process Management
 * - The process/server manager is known as Phoenix
 */
#define __MODULE "PCIF"
#define __TRACE

/* Includes 
 * - System */
#include <process/phoenix.h>
#include <process/process.h>
#include <process/server.h>
#include <garbagecollector.h>
#include <scheduler.h>
#include <threading.h>
#include <debug.h>
#include <heap.h>

/* Includes
 * C-Library */
#include <stddef.h>
#include <ds/list.h>
#include <ds/mstring.h>
#include <string.h>

/* Prototypes 
 * They are defined later down this file */
int
PhoenixEventHandler(
    _In_Opt_ void *UserData,
    _In_ MCoreEvent_t *Event);
OsStatus_t
PhoenixReapAsh(
    _In_Opt_ void *UserData);

/* Globals 
 * State-keeping and data-storage */
static MCoreEventHandler_t *EventHandler = NULL;
static CriticalSection_t AccessLock;
static List_t *Processes = NULL;
static UUId_t *AliasMap = NULL;
static UUId_t GcHandlerId = 0;
static UUId_t ProcessIdGenerator = 0;

/* PhoenixInitialize
 * Initialize the Phoenix environment and 
 * start the event-handler loop, it handles all requests 
 * and nothing happens if it isn't started */
void
PhoenixInitialize(void)
{
	// Variables
	int i;

	// Debug
	TRACE("Initializing environment and event handler");

	// Initialize Globals
	ProcessIdGenerator = 1;
	Processes = ListCreate(KeyInteger);
    GcHandlerId = GcRegister(PhoenixReapAsh);
    CriticalSectionConstruct(&AccessLock, CRITICALSECTION_PLAIN);

	// Initialize the global alias map
	AliasMap = (UUId_t*)kmalloc(sizeof(UUId_t) * PHOENIX_MAX_ASHES);
	for (i = 0; i < PHOENIX_MAX_ASHES; i++) {
		AliasMap[i] = UUID_INVALID;
	}

	// Create event handler
	EventHandler = EventInit("phoenix", PhoenixEventHandler, NULL);
}

/* PhoenixCreateRequest
 * Creates and queues up a new request for the process-manager. */
void
PhoenixCreateRequest(
    _In_ MCorePhoenixRequest_t *Request)
{
	EventCreate(EventHandler, &Request->Base);
}

/* PhoenixWaitRequest
 * Wait for a request to finish. A timeout can be specified. */
void
PhoenixWaitRequest(
    _In_ MCorePhoenixRequest_t *Request,
    _In_ size_t Timeout)
{
	EventWait(&Request->Base, Timeout);
}

/* PhoenixRegisterAlias
 * Allows a server to register an alias for its id
 * which means that id (must be above SERVER_ALIAS_BASE)
 * will always refer the calling process */
OsStatus_t
PhoenixRegisterAlias(
	_In_ MCoreAsh_t *Ash, 
	_In_ UUId_t Alias)
{
	// Sanitize both the server and alias 
	if (Ash == NULL
		|| (Alias < PHOENIX_ALIAS_BASE)
		|| AliasMap[Alias - PHOENIX_ALIAS_BASE] != UUID_INVALID) {
		LogFatal("PHNX", "Failed to register alias 0x%x for ash %u (0x%x - %u)",
			Alias, (Ash == NULL ? UUID_INVALID : Ash->Id),
			AliasMap[Alias - PHOENIX_ALIAS_BASE], Alias - PHOENIX_ALIAS_BASE);
		return OsError;
	}

	// Register
	AliasMap[Alias - PHOENIX_ALIAS_BASE] = Ash->Id;
	return OsSuccess;
}

/* PhoenixUpdateAlias
 * Checks if the given process-id has an registered alias.
 * If it has, the given process-id will be overwritten. */
OsStatus_t
PhoenixUpdateAlias(
    _InOut_ UUId_t *AshId)
{
    if (*AshId >= PHOENIX_ALIAS_BASE
		&& *AshId < (PHOENIX_ALIAS_BASE + PHOENIX_MAX_ASHES)) {
        *AshId = AliasMap[*AshId - PHOENIX_ALIAS_BASE];
        return OsSuccess;
    }
    return OsError;
}

/* PhoenixGetNextId 
 * Retrieves the next process-id. */
UUId_t
PhoenixGetNextId(void)
{
    return ProcessIdGenerator++;
}

/* PhoenixGetProcesses 
 * Retrieves access to the list of processes. */
List_t*
PhoenixGetProcesses(void)
{
    return Processes;
}

/* PhoenixRegisterAsh
 * Registers a new ash by adding it to the process-list */
OsStatus_t
PhoenixRegisterAsh(
    _In_ MCoreAsh_t *Ash)
{
    // Variables
    DataKey_t Key;
    
	// Modifications to process-list are locked
    CriticalSectionEnter(&AccessLock);
	Key.Value = (int)Ash->Id;
	ListAppend(Processes, ListCreateNode(Key, Key, Ash));
    CriticalSectionLeave(&AccessLock);
    return OsSuccess;
}

/* PhoenixTerminateAsh
 * This marks an ash for termination by taking it out
 * of rotation and adding it to the cleanup list */
void
PhoenixTerminateAsh(
    _In_ MCoreAsh_t *Ash)
{
    // Variables
    DataKey_t Key;

    // To modify list is locked operation
    CriticalSectionEnter(&AccessLock);
    Key.Value = (int)Ash->Id;
	ListRemoveByKey(Processes, Key);
    CriticalSectionLeave(&AccessLock);

	// Alert GC
	SchedulerThreadWakeAll((uintptr_t*)Ash);
	GcSignal(GcHandlerId, Ash);
}

/* PhoenixReapAsh
 * This function cleans up processes and
 * ashes and servers that might be queued up for
 * destruction, they can't handle all their cleanup themselves */
OsStatus_t
PhoenixReapAsh(
    _In_Opt_ void *UserData)
{
	// Instantiate the base-pointer
	MCoreAsh_t *Ash = (MCoreAsh_t*)UserData;

	// Clean up
	if (Ash->Type == AshBase) {
		PhoenixCleanupAsh(Ash);
	}
	else if (Ash->Type == AshProcess) {
		PhoenixCleanupProcess((MCoreProcess_t*)Ash);
	}
	else {
		//??
		return OsError;
	}
	return OsSuccess;
}

/* PhoenixEventHandler
 * This routine is invoked every-time there is any
 * request pending for us */
int
PhoenixEventHandler(
    _In_Opt_ void *UserData,
    _In_ MCoreEvent_t *Event)
{
	// Variables
	MCorePhoenixRequest_t *Request = NULL;

	// Unused
	_CRT_UNUSED(UserData);

	// Instantiate pointers
	Request = (MCorePhoenixRequest_t*)Event;
	switch (Request->Base.Type) {
		case AshSpawnProcess:
		case AshSpawnServer: {
			TRACE("Spawning %s", MStringRaw(Request->Path));

			if (Request->Base.Type == AshSpawnServer) {
				Request->AshId = PhoenixCreateServer(Request->Path);
			}
			else {
				Request->AshId = PhoenixCreateProcess(
                    Request->Path, &Request->StartupInformation);
			}

			// Sanitize result
			if (Request->AshId != UUID_INVALID) {
                Request->Base.State = EventOk;      
            }
			else {
				Request->Base.State = EventFailed;
            }
        } break;
        
		case AshKill: {
			MCoreAsh_t *Ash = PhoenixGetAsh(Request->AshId);
			if (Ash != NULL) {
				ThreadingTerminateAshThreads(Ash->Id);
				PhoenixTerminateAsh(Ash);
			}
			else {
				Request->Base.State = EventFailed;
			}
		} break;

		default: {
			ERROR("Unhandled Event %u", (size_t)Request->Base.Type);
		} break;
	}

	// Handle cleanup
	if (Request->Base.Cleanup != 0) {
		if (Request->Path != NULL) {
			MStringDestroy(Request->Path);
        }
	}
	return 0;
}
