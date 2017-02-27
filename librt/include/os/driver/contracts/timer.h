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
 * MollenOS MCore - Contract Definitions & Structures (Timer Contract)
 * - This header describes the base contract-structure, prototypes
 *   and functionality, refer to the individual things for descriptions
 */

#ifndef _CONTRACT_TIMER_INTERFACE_H_
#define _CONTRACT_TIMER_INTERFACE_H_

/* Includes 
 * - System */
#include <os/driver/contracts/base.h>
#include <os/osdefs.h>
#include <time.h>

/* Timer device query functions that must be implemented
 * by the timer driver - those can then be used by this interface */
#define __TIMER_QUERY_STAT				IPC_DECL_FUNCTION(0)

/* TimerQuery
 * This queries the timer contract for data
 * and must be implemented by all contracts that
 * implement the timer interface */
SERVICEAPI
OsStatus_t
SERVICEABI
TimerQuery(
	_Out_ clock_t *clock) 
{
	return QueryContract(ContractTimer, __TIMER_QUERY_STAT,
		NULL, 0, NULL, 0, NULL, 0, clock, sizeof(clock_t));
}

/* RegisterSystemTimer
 * Registers the given interrupt source as a system
 * timer source, with the given tick. This way the system
 * can always keep track of timers */
_MOS_API
OsStatus_t
MOSABI
RegisterSystemTimer(
	_In_ UUId_t Interrupt, 
	_In_ size_t NsPerTick);

#endif //!_CONTRACT_TIMER_INTERFACE_H_
