/* Eupnp - UPnP library
 *
 * Copyright (C) 2009 Andre Dieb Martins <andre.dieb@gmail.com>
 *
 * This file is part of Eupnp.
 *
 * Eupnp is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Eupnp is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with Eupnp.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <stdio.h>
#include <Eina.h>
#include <string.h>

#include "eupnp_ssdp.h"
#include "eupnp_error.h"
#include "eupnp_udp_transport.h"
#include "eupnp_http_message.h"


/*
 * Private API
 */

static int _eupnp_ssdp_main_count = 0;

/*
 * Public API
 */

int
eupnp_ssdp_init(void)
{
   if (_eupnp_ssdp_main_count) return ++_eupnp_ssdp_main_count;

   if (!eupnp_error_init())
     {
	fprintf(stderr, "Failed to initialize eupnp error module.\n");
	return _eupnp_ssdp_main_count;
     }

   if (!eina_array_init())
     {
	fprintf(stderr, "Failed to initialize eina array module\n");
	return _eupnp_ssdp_main_count;
     }

   if (!eina_stringshare_init())
     {
	fprintf(stderr, "Failed to initialize eina stringshare module\n");
	return _eupnp_ssdp_main_count;
     }

   _eupnp_ssdp_notify = (char *) eina_stringshare_add("NOTIFY");
   _eupnp_ssdp_msearch = (char *) eina_stringshare_add("M-SEARCH");
   _eupnp_ssdp_http_version = (char *) eina_stringshare_add(EUPNP_HTTP_VERSION);

   return ++_eupnp_ssdp_main_count;
}

int
eupnp_ssdp_shutdown(void)
{
   if (_eupnp_ssdp_main_count != 1) return --_eupnp_ssdp_main_count;

   eupnp_error_shutdown();
   eina_array_shutdown();
   eina_stringshare_shutdown();

   return --_eupnp_ssdp_main_count;
}

Eupnp_SSDP_Server *
eupnp_ssdp_server_new(void)
{
   Eupnp_SSDP_Server *ssdp;

   ssdp = calloc(1, sizeof(Eupnp_SSDP_Server));

   if (!ssdp)
     {
	eina_error_set(EINA_ERROR_OUT_OF_MEMORY);
	ERROR("Could not create SSDP server instance.\n");
	return NULL;
     }

   ssdp->udp_sock = eupnp_udp_transport_new(EUPNP_SSDP_ADDR,
					    EUPNP_SSDP_PORT,
					    EUPNP_SSDP_LOCAL_IFACE);

   if (!ssdp->udp_sock)
     {
	ERROR("Could not create SSDP server instance.\n");
	free(ssdp);
	return NULL;
     }

   return ssdp;
}

void
eupnp_ssdp_server_free(Eupnp_SSDP_Server *ssdp)
{
   if (!ssdp) return;
   eupnp_udp_transport_free(ssdp->udp_sock);
   free(ssdp);
}

/*
 * Sends a search message for devices to the network(a.k.a. M-Search)
 *
 * @param ssdp Eupnp_SSDP_Server instance.
 * @param mx maximum wait time in seconds for devices to wait before answering
 *        the search message.
 * @param search_target target for the search. Common values are "ssdp:all",
 *        "upnp:rootdevice", and so on (refer to the UPnP device architecture
 *        document for more).
 * @return On success EINA_TRUE, EINA_FALSE on error.
 */
Eina_Bool
eupnp_ssdp_discovery_request_send(Eupnp_SSDP_Server *ssdp, int mx, char *search_target)
{
   char *msearch;

   if (asprintf(&msearch, EUPNP_SSDP_MSEARCH_TEMPLATE,
                EUPNP_SSDP_ADDR, EUPNP_SSDP_PORT, mx, search_target) < 0)
     {
	ERROR("Could not allocate buffer for search message.\n");
	return EINA_FALSE;
     }

    /* Use UDP socket from SSDP */
   if (eupnp_udp_transport_sendto(ssdp->udp_sock, msearch, EUPNP_SSDP_ADDR,
                               EUPNP_SSDP_PORT) < 0)
     {
	ERROR("Could not send search message.\n");
	return EINA_FALSE;
     }

   free(msearch);
   return EINA_TRUE;
}

/*
 * Called when a datagram is ready to be read from the socket. Parses it and
 * takes the appropriate actions, considering the method of the request.
 *
 * TODO auto-register me on the event loop.
 */
void
_eupnp_ssdp_on_datagram_available(Eupnp_SSDP_Server *ssdp)
{
   Eupnp_HTTP_Request *m;
   Eupnp_UDP_Datagram *d;

   d = eupnp_udp_transport_recvfrom(ssdp->udp_sock);

   DEBUG("Message from %s:%d\n", d->host, d->port);

   if (!d)
     {
	ERROR("Could not retrieve a valid datagram\n");
	return;
     }

   if (eupnp_http_message_is_response(d->data))
     {
	DEBUG("Message is response!\n");

	Eupnp_HTTP_Response *r;
	r = eupnp_http_response_parse(d->data);

	if (!r)
	  {
	     ERROR("Failed parsing response datagram\n");
	     eupnp_udp_transport_datagram_free(d);
	     return;
	  }

	eupnp_http_response_dump(r);
	eupnp_http_response_free(r);
     }
   else
     {
	DEBUG("Message is request!\n");
	Eupnp_HTTP_Request *m;
	m = eupnp_http_request_parse(d->data);

	if (!m)
	  {
	     ERROR("Failed parsing request datagram\n");
	     eupnp_udp_transport_datagram_free(d);
	     return;
	  }

	eupnp_http_request_dump(m);

	if (m->method == _eupnp_ssdp_notify)
	  {
	     // TODO Handle notify message (ssdp:alive or ssdp:byebye)
	     DEBUG("Received NOTIFY request.\n");
	  }
	else if (m->method == _eupnp_ssdp_msearch)
	  {
	     // TODO Remove me.
	     DEBUG("Received M-SEARCH request\n'");
	  }

	eupnp_http_request_free(m);
     }

   eupnp_udp_transport_datagram_free(d);
}
