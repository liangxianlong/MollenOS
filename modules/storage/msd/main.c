/**
 * MollenOS
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
 * Mass Storage Device Driver (Generic)
 */
//#define __TRACE

#include <ddk/storage.h>
#include <ddk/utils.h>
#include <os/mollenos.h>
#include "msd.h"
#include <string.h>
#include <stdlib.h>

#include "ctt_driver_protocol_server.h"
#include "ctt_storage_protocol_server.h"

static list_t Devices = LIST_INIT;

MsdDevice_t*
MsdDeviceGet(
    _In_ UUId_t deviceId)
{
    return list_find_value(&Devices, (void*)(uintptr_t)deviceId);
}

OsStatus_t
OnLoad(void)
{
    // Register supported protocols
    gracht_server_register_protocol(&ctt_driver_protocol);
    gracht_server_register_protocol(&ctt_storage_protocol);
    return UsbInitialize();
}

static void
DestroyElement(
    _In_ element_t* Element,
    _In_ void*      Context)
{
    MsdDeviceDestroy(Element->value);
}

OsStatus_t
OnUnload(void)
{
    list_clear(&Devices, DestroyElement, NULL);
    return UsbCleanup();
}

OsStatus_t
OnRegister(
    _In_ MCoreDevice_t *Device)
{
    MsdDevice_t* MsdDevice;
    
    MsdDevice = MsdDeviceCreate((MCoreUsbDevice_t*)Device);
    if (MsdDevice == NULL) {
        return OsError;
    }

    list_append(&Devices, &MsdDevice->Header);
    return OsSuccess;
}

void ctt_driver_register_device_callback(struct gracht_recv_message* message, struct ctt_driver_register_device_args* args)
{
    OsStatus_t status = OnRegister(args->device);
    ctt_driver_register_device_response(message, status);
}

OsStatus_t
OnUnregister(
    _In_ MCoreDevice_t *Device)
{
    MsdDevice_t* MsdDevice = MsdDeviceGet(Device->Id);
    if (MsdDevice == NULL) {
        return OsError;
    }

    list_remove(&Devices, &MsdDevice->Header);
    return MsdDeviceDestroy(MsdDevice);
}

void ctt_storage_stat_callback(struct gracht_recv_message* message, struct ctt_storage_stat_args* args)
{
    StorageDescriptor_t descriptor = { 0 };
    OsStatus_t          status     = OsDoesNotExist;
    MsdDevice_t*        device     = MsdDeviceGet(args->device_id);
    if (device) {
        memcpy(&descriptor, &device->Descriptor, sizeof(StorageDescriptor_t));
        status = OsSuccess;
    }
    
    ctt_storage_stat_response(message, status, &descriptor);
}
