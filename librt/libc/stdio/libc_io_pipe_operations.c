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
 * C Standard Library
 * - Standard IO pipe operation implementations.
 */

#include <errno.h>
#include <internal/_io.h>

OsStatus_t stdio_pipe_op_read(stdio_handle_t* handle, void* buffer, size_t length, size_t* bytes_read)
{
    return OsSuccess;
}

OsStatus_t stdio_pipe_op_write(stdio_handle_t* handle, const void* buffer, size_t length, size_t* bytes_written)
{
    return OsSuccess;
}

OsStatus_t stdio_pipe_op_seek(stdio_handle_t* handle, int origin, off64_t offset, long long* position_out)
{
    return OsNotSupported;
}

OsStatus_t stdio_pipe_op_resize(stdio_handle_t* handle, long long resize_by)
{
    // This could be implemented some day, but for now we do not support
    // the resize operation on pipes.
    return OsNotSupported;
}

OsStatus_t stdio_pipe_op_close(stdio_handle_t* handle, int options)
{
    // Depending on the setup of the pipe. If the pipe is local, then we 
    // can simply free the structure. If the pipe is global/inheritable, we need
    // to free the memory used, and destroy the handle.
    return OsSuccess;
}

OsStatus_t stdio_pipe_op_inherit(stdio_handle_t* handle)
{
    // dma_attach
    // dma_attachment_map
    // 
    return OsSuccess;
}

void stdio_get_pipe_operations(stdio_ops_t* ops)
{
    ops->inherit = stdio_pipe_op_inherit;
    ops->read    = stdio_pipe_op_read;
    ops->write   = stdio_pipe_op_write;
    ops->seek    = stdio_pipe_op_seek;
    ops->resize  = stdio_pipe_op_resize;
    ops->close   = stdio_pipe_op_close;
}
