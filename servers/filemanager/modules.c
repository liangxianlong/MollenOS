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
 * MollenOS - File Manager Service
 * - Handles all file related services and disk services
 */

/* Includes
 * - System */
#include "include/vfs.h"

/* Includes
 * - Library */
#include <stdlib.h>

/* VfsResolveFileSystem
 * Tries to resolve the given filesystem by locating
 * the appropriate driver library for the given type */
FileSystemModule_t *VfsResolveFileSystem(FileSystem_t *FileSystem)
{
	/* Variables */
	FileSystemModule_t *Module = NULL;
	ListNode_t *fNode = NULL;
	DataKey_t Key;

	/* Iterate the module list and
	 * try to locate an already loaded module */
	_foreach(fNode, VfsGetModules()) {
		Module = (FileSystemModule_t*)fNode->Data;
		if (Module->Type == FileSystem->Type) {
			Module->References++;
			return Module;
		}
	}

	/* Not found, allocate a new instance */
	Module = (FileSystemModule_t*)malloc(sizeof(FileSystemModule_t));
	Module->Type = FileSystem->Type;
	Module->References = 1;

	/* Resolve the library path and load it */
	Module->Handle = SharedObjectLoad("");

	/* Were we able to resolve? */
	if (Module->Handle == HANDLE_INVALID) {
		free(Module);
		return NULL;
	}

	/* Resolve all functions, they MUST exist
	 * - FsInitialize
	 * - FsDestroy
	 * - FsOpenFile
	 * - FsCloseFile
	 * - FsOpenHandle
	 * - FsCloseHandle
	 * - FsReadFile
	 * - FsWriteFile
	 * - FsSeekFile
	 * - FsDeleteFile
	 * - FsQueryFile */
	Module->Initialize = (FsInitialize_t)
		SharedObjectGetFunction(Module->Handle, "FsInitialize");
	Module->Destroy = (FsDestroy_t)
		SharedObjectGetFunction(Module->Handle, "FsDestroy");
	Module->OpenFile = (FsOpenFile_t)
		SharedObjectGetFunction(Module->Handle, "FsOpenFile");
	Module->CloseFile = (FsCloseFile_t)
		SharedObjectGetFunction(Module->Handle, "FsCloseFile");
	Module->OpenHandle = (FsOpenHandle_t)
		SharedObjectGetFunction(Module->Handle, "FsOpenHandle");
	Module->CloseHandle = (FsCloseHandle_t)
		SharedObjectGetFunction(Module->Handle, "FsCloseHandle");
	Module->ReadFile = (FsReadFile_t)
		SharedObjectGetFunction(Module->Handle, "FsReadFile");
	Module->WriteFile = (FsWriteFile_t)
		SharedObjectGetFunction(Module->Handle, "FsWriteFile");
	Module->SeekFile = (FsSeekFile_t)
		SharedObjectGetFunction(Module->Handle, "FsSeekFile");
	Module->DeleteFile = (FsDeleteFile_t)
		SharedObjectGetFunction(Module->Handle, "FsDeleteFile");
	Module->QueryFile = (FsQueryFile_t)
		SharedObjectGetFunction(Module->Handle, "FsQueryFile");

	/* Sanitize functions */
	if (Module->Initialize == NULL
		|| Module->Destroy == NULL
		|| Module->OpenFile == NULL
		|| Module->CloseFile == NULL
		|| Module->OpenHandle == NULL
		|| Module->CloseHandle == NULL
		|| Module->ReadFile == NULL
		|| Module->WriteFile == NULL
		|| Module->SeekFile == NULL
		|| Module->DeleteFile == NULL
		|| Module->QueryFile == NULL) {
		SharedObjectUnload(Module->Handle);
		free(Module);
		return NULL;
	}

	/* Last thing is to add it to the list */
	Key.Value = 0;
	ListAppend(VfsGetModules(), ListCreateNode(Key, Key, Module));

	/* Return the newly created module */
	return Module;
}
