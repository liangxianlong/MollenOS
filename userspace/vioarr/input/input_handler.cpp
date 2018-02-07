/* MollenOS
 *
 * Copyright 2011 - 2018, Philip Meulengracht
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
 * MollenOS - Vioarr Window Compositor System (Input Interface)
 *  - The window compositor system and general window manager for
 *    MollenOS.
 */

#include <input_handler.hpp>
#include <os/service.h>
#include <cstdlib>

void MessageHandler(InputHandler *Handler) 
{
    // Variables
    char *ArgumentBuffer    = NULL;
    bool IsRunning          = true;
	MRemoteCall_t Message;

    // Open pipe
    ArgumentBuffer = (char*)::malloc(IPC_MAX_MESSAGELENGTH);
	PipeOpen(PIPE_RPCOUT);

    // Listen for messages
	while (IsRunning) {
		if (RPCListen(&Message, ArgumentBuffer) == OsSuccess) {
			// Handle event
            if (Message.Function == __WINDOWMANAGER_NEWINPUT) {
                
            }
		}
	}

    // Done
    PipeClose(PIPE_RPCOUT);
}

// Constructor
InputHandler::InputHandler() : 
    _MessageThread(MessageHandler, this) {
    
}

// Destructor
~InputHandler::~InputHandler() {
    // Kill thread
}