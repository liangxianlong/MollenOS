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
 * MollenOS C Library - Driver Entry 
 */

#include "../libc/threads/tls.h"
#include <os/mollenos.h>
#include <ddk/driver.h>
#include <stdlib.h>

// Module Interface
__EXTERN OsStatus_t        OnLoad(void);
__EXTERN OsStatus_t        OnUnload(void);
__EXTERN OsStatus_t        OnRegister(MCoreDevice_t*);
__EXTERN OsStatus_t        OnUnregister(MCoreDevice_t*);
__EXTERN InterruptStatus_t OnInterrupt(void*, size_t, size_t, size_t);
__EXTERN OsStatus_t        OnQuery(MContractType_t,  int,  MRemoteCallArgument_t*, MRemoteCallArgument_t*, MRemoteCallArgument_t*, MRemoteCallAddress_t*);

/* CRT Initialization sequence
 * for a shared C/C++ environment call this in all entry points */
extern char**
__CrtInitialize(
    _In_  thread_storage_t* Tls,
    _In_  int               IsModule,
    _Out_ int*              ArgumentCount);

/* __CrtModuleEntry
 * Use this entry point for modules. */
void __CrtModuleEntry(void)
{
    thread_storage_t Tls;
    MRemoteCall_t    Message;
    char*            ArgumentBuffer;
    int              IsRunning = 1;

    // Initialize environment
    __CrtInitialize(&Tls, 1, NULL);

    // Call the driver load function 
    // - This will be run once, before loop
    if (OnLoad() != OsSuccess) {
        goto Cleanup;
    }

    // Initialize the driver event loop
    ArgumentBuffer = (char*)malloc(IPC_MAX_MESSAGELENGTH);
    while (IsRunning) {
        if (RPCListen(&Message, ArgumentBuffer) == OsSuccess) {
            switch (Message.Function) {
                case __DRIVER_REGISTERINSTANCE: {
                    OnRegister((MCoreDevice_t*)Message.Arguments[0].Data.Buffer);
                } break;
                case __DRIVER_UNREGISTERINSTANCE: {
                    OnUnregister((MCoreDevice_t*)Message.Arguments[0].Data.Buffer);
                } break;
                case __DRIVER_INTERRUPT: {
                    OnInterrupt((void*)Message.Arguments[1].Data.Value,
                        Message.Arguments[2].Data.Value,
                        Message.Arguments[3].Data.Value,
                        Message.Arguments[4].Data.Value);
                } break;
                case __DRIVER_QUERY: {
                    OnQuery((MContractType_t)Message.Arguments[0].Data.Value, 
                        (int)Message.Arguments[1].Data.Value, 
                        &Message.Arguments[2], &Message.Arguments[3], 
                        &Message.Arguments[4], &Message.From);
                } break;
                case __DRIVER_UNLOAD: {
                    IsRunning = 0;
                } break;

                default: {
                    break;
                }
            }
        }
    }
Cleanup:
    OnUnload();
    exit(-1);
}
