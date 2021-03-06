/**
 * MollenOS
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
 * Gracht Server Type Definitions & Structures
 * - This header describes the base server-structure, prototypes
 *   and functionality, refer to the individual things for descriptions
 */

#include <assert.h>
#include <errno.h>
#include "include/gracht/aio.h"
#include "include/gracht/debug.h"
#include "include/gracht/list.h"
#include "include/gracht/server.h"
#include "include/gracht/link/link.h"
#include <stdlib.h>
#include <string.h>

extern int server_invoke_action(struct gracht_list*, struct gracht_recv_message*);

struct gracht_server_client {
    struct gracht_object_header header;
    int                         iod;
    struct link_ops*            ops;
};

struct gracht_server {
    struct server_link_ops* ops;
    int                     initialized;
    int                     completion_iod;
    int                     client_iod;
    int                     dgram_iod;
    struct gracht_list      protocols;
    struct gracht_list      clients;
} server_object = { NULL, 0, -1, -1, -1, { 0 }, { 0 } };

int gracht_server_initialize(gracht_server_configuration_t* configuration)
{
    assert(server_object.initialized == 0);
    
    // store handler
    server_object.initialized = 1;
    server_object.ops = configuration->link;
    
    // create the io event set, for async io
    server_object.completion_iod = gracht_aio_create();
    if (server_object.completion_iod < 0) {
        ERROR("gracht_server: failed to create aio handle\n");
        return -1;
    }
    
    // try to create the listening link. We do support that one of the links
    // are not supported by the link operations.
    server_object.client_iod = server_object.ops->listen(server_object.ops, LINK_LISTEN_SOCKET);
    if (server_object.client_iod < 0) {
        ERROR("gracht_server: failed to create client listener\n");
        if (errno != ENOTSUP) {
            return -1;
        }
    }
    else {
        gracht_aio_add(server_object.completion_iod, server_object.client_iod);
    }
    
    server_object.dgram_iod = server_object.ops->listen(server_object.ops, LINK_LISTEN_DGRAM);
    if (server_object.dgram_iod < 0) {
        ERROR("gracht_server: failed to create packet listener\n");
        if (errno != ENOTSUP) {
            return -1;
        }
    }
    else {
        gracht_aio_add(server_object.completion_iod, server_object.dgram_iod);
    }
    
    return 0;
}

static int handle_client_socket(void)
{
    struct gracht_server_client* client;
    struct link_ops*             client_ops;
    int                          client_iod;
    
    client_iod = server_object.ops->accept(server_object.ops, &client_ops);
    if (client_iod < 0) {
        ERROR("gracht_server: failed to accept client\n");
        return -1;
    }
    
    client = (struct gracht_server_client*)malloc(sizeof(struct gracht_server_client));
    if (!client) {
        ERROR("gracht_server: failed to allocate data for client\n");
        client_ops->close(client_ops);
        errno = (ENOMEM);
        return -1;
    }
    
    client->header.id = client_iod;
    client->iod = client_iod;
    client->ops = client_ops;
    
    // add client to list and aio
    gracht_list_append(&server_object.clients, &client->header);
    gracht_aio_add(server_object.completion_iod, client_iod);
    return 0;
}

static int handle_sync_event(int iod, uint32_t events, void* storage)
{
    struct gracht_recv_message message = { .storage = storage };
    int                        status;
    TRACE("[handle_sync_event] %i, 0x%x\n", iod, events);
    
    status = server_object.ops->recv_packet(server_object.ops, &message, MSG_DONTWAIT);
    if (status) {
        ERROR("[handle_sync_event] gracht_connection_recv_message returned %i\n", errno);
        return -1;
    }
    return server_invoke_action(&server_object.protocols, &message);
}

static int handle_async_event(int iod, uint32_t events, void* storage)
{
    int                          status;
    struct gracht_recv_message   message = { .storage = storage };
    struct gracht_server_client* client = 
        (struct gracht_server_client*)gracht_list_lookup(&server_object.clients, iod);
    TRACE("[handle_async_event] %i, 0x%x\n", iod, events);
    
    // Check for control event. On non-passive sockets, control event is the
    // disconnect event.
    if (events & GRACHT_AIO_EVENT_CTRL) {
        status = gracht_aio_remove(server_object.completion_iod, iod);
        if (status) {
            // TODO log
        }
        
        status = client->ops->close(client->ops);
        gracht_list_remove(&server_object.clients, &client->header);
        free(client);
    }
    else if ((events & GRACHT_AIO_EVENT_IN) || !events) {
        while (1) {
            status = client->ops->recv(client->ops, &message, MSG_DONTWAIT);
            if (status) {
                ERROR("[handle_async_event] gracht_connection_recv_message returned %i\n", errno);
                break;
            }
            
            status = server_invoke_action(&server_object.protocols, &message);
        }
    }
    return 0;
}

static int gracht_server_shutdown(void)
{
    struct gracht_server_client* client;
    struct gracht_server_client* prev;
    
    assert(server_object.initialized == 1);
    
    client = (struct gracht_server_client*)server_object.clients.head;
    while (client) {
        client->ops->close(client->ops);
        
        prev   = client;
        client = (struct gracht_server_client*)client->header.link;
        free(prev);
    }
    server_object.clients.head = NULL;
    
    if (server_object.completion_iod != -1) {
        gracht_aio_destroy(server_object.completion_iod);
    }
    
    if (server_object.ops != NULL) {
        server_object.ops->destroy(server_object.ops);
    }
    
    server_object.initialized = 0;
    return 0;
}

int gracht_server_main_loop(void)
{
    void*              storage;
    gracht_aio_event_t events[32];
    int                i;
    
    storage = malloc(GRACHT_MAX_MESSAGE_SIZE);
    if (!storage) {
        errno = (ENOMEM);
        return -1;
    }

    TRACE("gracht_server: started... [%i, %i]\n", server_object.client_iod, server_object.dgram_iod);
    while (server_object.initialized) {
        int num_events = gracht_io_wait(server_object.completion_iod, &events[0], 32);
        TRACE("gracht_server: %i events received!\n", num_events);
        for (i = 0; i < num_events; i++) {
            int      iod   = gracht_aio_event_iod(&events[i]);
            uint32_t flags = gracht_aio_event_events(&events[i]);

            TRACE("gracht_server: event %u from %i\n", flags, iod);
            if (iod == server_object.client_iod) {
                if (handle_client_socket()) {
                    // TODO - log
                }
            }
            else if (iod == server_object.dgram_iod) {
                handle_sync_event(server_object.dgram_iod, flags, storage);
            }
            else {
                handle_async_event(iod, flags, storage);
            }
        }
    }
    
    free(storage);
    return gracht_server_shutdown();
}

int gracht_server_respond(struct gracht_recv_message* messageContext, struct gracht_message* message)
{
    struct gracht_server_client* clientOps;

    if (!messageContext || !message) {
        errno = (EINVAL);
        return -1;
    }

    if (messageContext->client == server_object.dgram_iod) {
        return server_object.ops->respond(server_object.ops, messageContext, message);
    }

    clientOps = (struct gracht_server_client*)gracht_list_lookup(&server_object.clients, messageContext->client);
    if (!clientOps) {
        errno = (ENOENT);
        return -1;
    }

    return clientOps->ops->send(clientOps->ops, message, MSG_WAITALL);
}

int gracht_server_send_event(int client, struct gracht_message* message, unsigned int flags)
{
    struct gracht_server_client* clientOps = 
        (struct gracht_server_client*)gracht_list_lookup(&server_object.clients, client);
    if (!clientOps) {
        errno = (ENOENT);
        return -1;
    }
    
    return clientOps->ops->send(clientOps->ops, message, flags);
}

int gracht_server_broadcast_event(struct gracht_message* message, unsigned int flags)
{
    struct gracht_server_client* client;
    
    client = (struct gracht_server_client*)server_object.clients.head;
    while (client) {
        client->ops->send(client->ops, message, flags);
        client = (struct gracht_server_client*)client->header.link;
    }
    return 0;
}

int gracht_server_register_protocol(gracht_protocol_t* protocol)
{
    if (!protocol) {
        errno = (EINVAL);
        return -1;
    }
    
    gracht_list_append(&server_object.protocols, &protocol->header);
    return 0;
}

int gracht_server_unregister_protocol(gracht_protocol_t* protocol)
{
    if (!protocol) {
        errno = (EINVAL);
        return -1;
    }
    
    gracht_list_remove(&server_object.protocols, &protocol->header);
    return 0;
}

int gracht_server_get_dgram_iod(void)
{
    return server_object.dgram_iod;
}
