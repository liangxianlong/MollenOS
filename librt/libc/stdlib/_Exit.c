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
 * MollenOS C-Support Exit Implementation
 * - Definitions, prototypes and information needed.
 */

#include <internal/_syscalls.h>
#include <internal/_utils.h>
#include <os/process.h>

/* _Exit
 * Terminate normally, no cleanup. No calls to anything. 
 * And it never returns this function */
void
_Exit(
    _In_ int Status)
{
    if (IsProcessModule()) {
        Syscall_ModuleExit(Status);
    }
    else {
        ProcessTerminate(Status);
    }
    Syscall_ThreadExit(Status);
    for(;;);
}
