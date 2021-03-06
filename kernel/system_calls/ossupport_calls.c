/* MollenOS
 *
 * Copyright 2017, Philip Meulengracht
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
 * MollenOS MCore - System Calls
 */
#define __MODULE "SCIF"
//#define __TRACE

#include <modules/manager.h>
#include <memoryspace.h>
#include <ds/mstring.h>
#include <threading.h>
#include <handle.h>
#include <handle_set.h>
#include <assert.h>
#include <debug.h>
#include <heap.h>

OsStatus_t
ScCreateMemoryHandler(
    _In_  Flags_t    Flags,
    _In_  size_t     Length,
    _Out_ UUId_t*    HandleOut,
    _Out_ uintptr_t* AddressBaseOut)
{
    SystemMemoryMappingHandler_t* Handler;
    SystemMemorySpace_t*          Space = GetCurrentMemorySpace();
    assert(Space->Context != NULL);

    Handler = (SystemMemoryMappingHandler_t*)kmalloc(sizeof(SystemMemoryMappingHandler_t));
    if (!Handler) {
        return OsOutOfMemory;
    }
    
    ELEMENT_INIT(&Handler->Header, 0, Handler);
    Handler->Handle  = CreateHandle(HandleTypeGeneric, NULL, Handler);
    Handler->Address = DynamicMemoryPoolAllocate(&Space->Context->Heap, Length);
    if (!Handler->Address) {
        DestroyHandle(Handler->Handle);
        kfree(Handler);
        return OsOutOfMemory;
    }
    Handler->Length  = Length;
    
    *HandleOut       = Handler->Handle;
    *AddressBaseOut  = Handler->Address;
    list_append(Space->Context->MemoryHandlers, &Handler->Header);
    return OsSuccess;
}

OsStatus_t
ScDestroyMemoryHandler(
    _In_ UUId_t Handle)
{
    SystemMemoryMappingHandler_t* Handler = (SystemMemoryMappingHandler_t*)LookupHandle(Handle);
    SystemMemorySpace_t*          Space   = GetCurrentMemorySpace();
    assert(Space->Context != NULL);

    if (Space->Context->MemoryHandlers != NULL && Handler != NULL) {
        list_remove(Space->Context->MemoryHandlers, &Handler->Header);
        DynamicMemoryPoolFree(&Space->Context->Heap, Handler->Address);
        DestroyHandle(Handle);
        kfree(Handler);
        return OsSuccess;
    }
    return OsDoesNotExist;
}

OsStatus_t
ScInstallSignalHandler(
    _In_ uintptr_t Handler) 
{
    SystemMemorySpace_t* Space = GetCurrentMemorySpace();
    assert(Space->Context != NULL);

    Space->Context->SignalHandler = Handler;
    return OsSuccess;
}

OsStatus_t
ScCreateHandle(
    _Out_ UUId_t* HandleOut)
{
    *HandleOut = CreateHandle(HandleTypeGeneric, NULL, NULL);
    if (*HandleOut != UUID_INVALID) {
        return OsSuccess;
    }
    return OsOutOfMemory;
}

OsStatus_t
ScSetHandleActivity(
    _In_ UUId_t  Handle,
    _In_ Flags_t Flags)
{
    return MarkHandle(Handle, Flags);
}

OsStatus_t
ScRegisterHandlePath(
    _In_ UUId_t      Handle,
    _In_ const char* Path)
{
    return RegisterHandlePath(Handle, Path);
}

OsStatus_t
ScLookupHandle(
    _In_  const char* Path,
    _Out_ UUId_t*     HandleOut)
{
    return LookupHandleByPath(Path, HandleOut);
}

OsStatus_t
ScDestroyHandle(
    _In_ UUId_t Handle)
{
    if (Handle == 0 || Handle == UUID_INVALID) {
        return OsInvalidParameters;
    }
    DestroyHandle(Handle);
    return OsSuccess;
}

OsStatus_t
ScCreateHandleSet(
    _In_  Flags_t Flags,
    _Out_ UUId_t* HandleOut)
{
    if (!HandleOut) {
        return OsInvalidParameters;
    }
    
    *HandleOut = CreateHandleSet(Flags);
    if (*HandleOut != UUID_INVALID) {
        return OsSuccess;
    }
    return OsOutOfMemory;
}

OsStatus_t
ScControlHandleSet(
    _In_ UUId_t  SetHandle,
    _In_ int     Operation,
    _In_ UUId_t  Handle,
    _In_ Flags_t Flags,
    _In_ void*   Context)
{
    return ControlHandleSet(SetHandle, Operation, Handle, Flags, Context);
}

OsStatus_t
ScListenHandleSet(
    _In_  UUId_t          Handle,
    _In_  handle_event_t* Events,
    _In_  int             MaxEvents,
    _In_  size_t          Timeout,
    _Out_ int*            NumberOfEventsOut)
{
    if (!Events || !NumberOfEventsOut) {
        return OsInvalidParameters;
    }
    return WaitForHandleSet(Handle, Events, MaxEvents, Timeout, NumberOfEventsOut);
}
