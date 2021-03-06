/* MollenOS
 *
 * Copyright 2019, Philip Meulengracht
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
 * Gracht Async IO Type Definitions & Structures
 * - This header describes the base aio-structure, prototypes
 *   and functionality, refer to the individual things for descriptions
 */

#ifndef __GRACHT_AIO_H__
#define __GRACHT_AIO_H__

#include "types.h"

#if defined(MOLLENOS)
#include <inet/socket.h>
#include <io_events.h>
#include <io.h>

typedef struct io_event gracht_aio_event_t;
#define GRACHT_AIO_EVENT_IN   IOEVTIN
#define GRACHT_AIO_EVENT_CTRL IOEVTCTL

#define gracht_aio_create()                io_set_create(0)
#define gracht_io_wait(aio, events, count) io_set_wait(aio, events, count, 0)
#define gracht_aio_add(aio, iod)           io_set_ctrl(aio, IO_EVT_DESCRIPTOR_ADD, iod, IOEVTIN | IOEVTCTL);
#define gracht_aio_remove(aio, iod)        io_set_ctrl(aio, IO_EVT_DESCRIPTOR_DEL, iod, 0);
#define gracht_aio_destroy(aio)            close(aio)

#define gracht_aio_event_iod(event)        (event)->iod
#define gracht_aio_event_events(event)     (event)->events

#elif defined(__linux__)
#include <unistd.h>
#include <sys/epoll.h>
#include <sys/socket.h>

typedef struct epoll_event gracht_aio_event_t;
#define GRACHT_AIO_EVENT_IN   EPOLLIN
#define GRACHT_AIO_EVENT_CTRL EPOLLRDHUP

#define gracht_aio_create()                epoll_create1(0)
#define gracht_io_wait(aio, events, count) epoll_wait(aio, events, count, -1);
#define gracht_aio_remove(aio, iod)        epoll_ctl(aio, EPOLL_CTL_DEL, iod, NULL)
#define gracht_aio_destroy(aio)            close(aio)

static int gracht_aio_add(int aio, int iod) {
    struct epoll_event event = {
        .events = EPOLLIN | EPOLLRDHUP,
        .data.fd = iod
    };
    return epoll_ctl(aio, EPOLL_CTL_ADD, iod, &event);
}

#define gracht_aio_event_iod(event)        (event)->data.fd
#define gracht_aio_event_events(event)     (event)->events

#else
#error "Undefined platform for aio"
#endif

#endif // !__GRACHT_AIO_H__
