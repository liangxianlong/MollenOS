/* MollenOS
 *
 * Copyright 2011 - 2016, Philip Meulengracht
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
 * - In this file we implement Ash functions
 */

/* Includes */
#include <Modules/Phoenix.h>
#include <Vfs/VfsWrappers.h>
#include <Threading.h>
#include <stddef.h>

/* Externs */
extern PhxId_t GlbAshIdGenerator;
extern List_t *GlbAshes;

/* This is the finalizor function for starting
 * up a new base Ash, it finishes setting up the environment
 * and memory mappings, must be called on it's own thread */
void PhoenixFinishAsh(MCoreAsh_t *Ash)
{
	/* Cast */
	Addr_t BaseAddress = MEMORY_LOCATION_USER;
	Cpu_t CurrentCpu = ApicGetCpu();
	MCoreThread_t *cThread = ThreadingGetCurrentThread(CurrentCpu);

	/* Update this thread */
	Ash->MainThread = cThread->Id;
	cThread->AshId = Ash->Id;

	/* Save address space */
	Ash->AddressSpace = AddressSpaceGetCurrent();

	/* Load Executable */
	Ash->Executable =
		PeLoadImage(NULL, Ash->Name, Ash->FileBuffer, Ash->FileBufferLength, &BaseAddress);
	Ash->NextLoadingAddress = BaseAddress;

	/* Cleanup file buffer */
	kfree(Ash->FileBuffer);
	Ash->FileBuffer = NULL;

	/* Create the memory allocators */
	Ash->Heap = BitmapCreate(MEMORY_LOCATION_USER_HEAP, MEMORY_LOCATION_USER_HEAP_END, PAGE_SIZE);
	Ash->Shm = BitmapCreate(MEMORY_LOCATION_USER_SHM, MEMORY_LOCATION_USER_SHM_END, PAGE_SIZE);

	/* Create the communication pipe */
	Ash->Pipe = PipeCreate(ASH_PIPE_SIZE);

	/* Map Stack */
	BaseAddress = ((MEMORY_LOCATION_USER_STACK - 0x1) & PAGE_MASK);
	AddressSpaceMap(AddressSpaceGetCurrent(), BaseAddress,
		ASH_STACK_INIT, MEMORY_MASK_DEFAULT, ADDRESS_SPACE_FLAG_USER);
	BaseAddress += (MEMORY_LOCATION_USER_STACK & ~(PAGE_MASK));
	Ash->StackStart = BaseAddress;

	/* Setup signalling */
	Ash->SignalQueue = ListCreate(KeyInteger, LIST_NORMAL);
}

/* This is the standard ash-boot function
 * which simply sets up the ash and jumps to userland */
void PhoenixBootAsh(void *Args)
{
	/* Cast the argument */
	MCoreAsh_t *Ash = (MCoreAsh_t*)Args;

	/* Finish boot */
	PhoenixFinishAsh(Ash);

	/* Go to user-land */
	ThreadingEnterUserMode(Ash);

	/* Catch */
	IThreadYield();

	/* SHOULD NEVER reach this point */
	for (;;);
}

/* This function loads the executable and
 * prepares the ash-environment, at this point
 * it won't be completely running yet, it needs
 * its own thread for that. Returns 0 on success */
int PhoenixInitializeAsh(MCoreAsh_t *Ash, MString_t *Path)
{ 
	/* Variables needed for init */
	MCoreFileInstance_t *File = NULL;
	uint8_t *fBuffer = NULL;
	size_t fSize = 0;
	int Index = 0;

	/* Sanity */
	if (Path == NULL) {
		return -1;
	}

	/* Open File */
	File = VfsWrapperOpen(MStringRaw(Path), Read);

	/* Sanity */
	if (File->Code != VfsOk
		|| File->File == NULL) {
		VfsWrapperClose(File);
		return -2;
	}

	/* Allocate a buffer */
	fSize = (size_t)File->File->Size;
	fBuffer = (uint8_t*)kmalloc(fSize);

	/* Read */
	VfsWrapperRead(File, fBuffer, fSize);

	/* Close */
	VfsWrapperClose(File);

	/* Validate File */
	if (!PeValidate(fBuffer, fSize)) {
		kfree(fBuffer);
		return -3;
	}

	/* Reset the Ash structure to be safe
	 * we don't want random dat athere */
	memset(Ash, 0, sizeof(MCoreAsh_t));

	/* Set initial */
	Ash->Id = GlbAshIdGenerator++;
	Ash->Parent = ThreadingGetCurrentThread(ApicGetCpu())->AshId;
	Ash->Type = AshBase;

	/* Split path, even if a / is not found
	* it won't fail, since -1 + 1 = 0, so we just copy
	* the entire string */
	Index = MStringFindReverse(File->File->Path, '/');
	Ash->Name = MStringSubString(File->File->Path, Index + 1, -1);
	Ash->Path = MStringCreate((void*)MStringRaw(File->File->Path), StrUTF8);

	/* Save file buffer */
	Ash->FileBuffer = fBuffer;
	Ash->FileBufferLength = fSize;

	/* Return success! */
	return 0;
}

/* This is a wrapper for starting up a base Ash
 * and uses <PhoenixInitializeAsh> to setup the env
 * and do validation before starting */
PhxId_t PhoenixStartupAsh(MString_t *Path)
{
	/* Variables needed for this */
	MCoreAsh_t *Ash = NULL;
	DataKey_t Key;

	/* Allocate the structure */
	Ash = (MCoreAsh_t*)kmalloc(sizeof(MCoreAsh_t));

	/* Sanitize the created Ash */
	if (PhoenixInitializeAsh(Ash, Path)) {
		kfree(Ash);
		return PHOENIX_NO_ASH;
	}

	/* Add process to list */
	Key.Value = (int)Ash->Id;
	ListAppend(GlbAshes, ListCreateNode(Key, Key, Ash));

	/* Create the loader thread */
	ThreadingCreateThread((char*)MStringRaw(Ash->Name),
		PhoenixBootAsh, Ash, THREADING_USERMODE);

	/* Done */
	return Ash->Id;
}

/* Queries the given ash for information
 * which kind of information is determined by <Function> */
int PhoenixQueryAsh(MCoreAsh_t *Ash,
	AshQueryFunction_t Function, void *Buffer, size_t Length)
{
	/* Sanitize the parameters */
	if (Ash == NULL
		|| Buffer == NULL
		|| Length == 0)
		return -1;

	/* Now.. What do you want to know? */
	switch (Function)
	{
		/* Query name? */
		case AshQueryName:
		{
			/* Never copy more data than user can contain */
			size_t BytesToCopy = MIN(MStringSize(Ash->Name), Length);

			/* Cooopy */
			memcpy(Buffer, MStringRaw(Ash->Name), BytesToCopy);

		} break;

		/* Query memory? in bytes of course */
		case AshQueryMemory:
		{
			/* For now, we only support querying of
			 * how much memory has been allocated for the heap */
			*((size_t*)Buffer) = (Ash->Heap->BlocksAllocated * PAGE_SIZE);

		} break;

		/* Query immediate parent? */
		case AshQueryParent:
		{
			/* Get a pointer */
			PhxId_t *bPtr = (PhxId_t*)Buffer;

			/* There we go */
			*bPtr = Ash->Parent;

		} break;

		/* Query topmost parent? */
		case AshQueryTopMostParent:
		{
			/* Get a pointer */
			PhxId_t *bPtr = (PhxId_t*)Buffer;

			/* Set initial value */
			*bPtr = PHOENIX_NO_ASH;

			/* While parent has a valid parent */
			MCoreAsh_t *Parent = Ash;
			while (Parent->Parent != PHOENIX_NO_ASH) {
				*bPtr = Parent->Parent;
				Parent = PhoenixGetAsh(Parent->Parent);
			}

		} break;

		default:
			break;
	}

	/* Done! */
	return 0;
}

/* Cleans up a given Ash, freeing all it's allocated resources
 * and unloads it's executables, memory space is not cleaned up
 * must be done by external thread */
void PhoenixCleanupAsh(MCoreAsh_t *Ash)
{
	/* Vars */
	ListNode_t *fNode = NULL;

	/* Cleanup Strings */
	MStringDestroy(Ash->Name);
	MStringDestroy(Ash->Path);

	/* Destroy signal stuff */
	_foreach(fNode, Ash->SignalQueue) {
		/* Free the signal */
		kfree(fNode->Data);
	}

	/* Destroy the signal list */
	ListDestroy(Ash->SignalQueue);

	/* Destroy Pipe */
	PipeDestroy(Ash->Pipe);

	/* Cleanup memory allocators */
	BitmapDestroy(Ash->Shm);
	BitmapDestroy(Ash->Heap);

	/* Cleanup executable data */
	PeUnload(Ash->Executable);

	/* Rest of memory is cleaned during
	* address space cleanup */
	kfree(Ash);
}

/* Lookup Ash
 * This function looks up a ash structure
 * by id, if either PHOENIX_CURRENT or PHOENIX_NO_ASH
 * is passed, it retrieves the current process */
MCoreAsh_t *PhoenixGetAsh(PhxId_t AshId)
{
	/* Variables */
	ListNode_t *pNode = NULL;
	Cpu_t CurrentCpu = 0;

	/* Sanity the list, no need to check in
	 * this case */
	if (ListLength(GlbAshes) == 0) {
		return NULL;
	}
	
	/* Sanitize the process id */
	if (AshId == PHOENIX_CURRENT
		|| AshId == PHOENIX_NO_ASH)
	{
		/* Get current cpu id */
		CurrentCpu = ApicGetCpu();

		/* Sanitize threading is up */
		if (ThreadingGetCurrentThread(CurrentCpu) != NULL) {
			AshId = ThreadingGetCurrentThread(CurrentCpu)->AshId;
		}
		else {
			return NULL;
		}
	}

	/* Iterate the list and try
	 * to locate the ash we have */
	_foreach(pNode, GlbAshes) {
		MCoreAsh_t *Ash = (MCoreAsh_t*)pNode->Data;
		if (Ash->Id == AshId) {
			return Ash;
		}
	}

	/* Found? NO! */
	return NULL;
}
