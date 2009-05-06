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

#include "eupnp_error.h"
#include "eupnp_udp_transport.h"
#include "eupnp_ssdp.h"


/*
 * Private API
 */

struct _Eupnp_HTTP_Header {
   const char *key;
   const char *value;
};

struct _Eupnp_HTTP_Message {
   Eina_Array *headers;
   const char *method;
   const char *http_version;
   const char *resource;
};

typedef struct _Eupnp_HTTP_Message Eupnp_HTTP_Message;
typedef struct _Eupnp_HTTP_Header Eupnp_HTTP_Header;

static int _eupnp_ssdp_main_count = 0;

static Eupnp_HTTP_Header *
eupnp_ssdp_http_header_new(const char *key_start, int key_len, const char *value_start, int value_len)
{
   Eupnp_HTTP_Header *h;

   /* Alloc blob */
   h = malloc(sizeof(Eupnp_HTTP_Header) + key_len + 1 + value_len + 1);

   if (!h)
     {
	eina_error_set(EINA_ERROR_OUT_OF_MEMORY);
	ERROR("header alloc error.\n");
	return NULL;
     }

   h->key = (char *)h + sizeof(Eupnp_HTTP_Header);
   h->value = (char *)h + sizeof(Eupnp_HTTP_Header) + sizeof(char)*(key_len + 1);
   memcpy((void *)h->key, key_start, key_len);
   memcpy((void *)h->value, value_start, value_len);
   ((char *) h->key)[key_len] = '\0';
   ((char *) h->value)[value_len] = '\0';

   return h;
}

static void
eupnp_ssdp_http_header_free(Eupnp_HTTP_Header *h)
{
   if (h) free(h);
}

static Eina_Bool
eupnp_ssdp_http_message_add_header(Eupnp_HTTP_Message *m, const char *key_start, int key_len, const char *value_start, int value_len)
{
   Eupnp_HTTP_Header *h;

   h = eupnp_ssdp_http_header_new(key_start, key_len, value_start, value_len);

   if (!h)
     {
	eina_error_set(EINA_ERROR_OUT_OF_MEMORY);
	ERROR("header alloc error.\n");
	return EINA_FALSE;
     }

   DEBUG("Header parsed: %s: %s\n", h->key, h->value);

   if (!eina_array_push(m->headers, h))
     {
	WARN("incomplete headers\n");
	eupnp_ssdp_http_header_free(h);
	return EINA_FALSE;
     }

   return EINA_TRUE;
}

static Eupnp_HTTP_Message *
eupnp_ssdp_http_message_new(const char *method_start, int method_len, const char *httpver_start, int http_version_len, const char *resource_start, int resource_len)
{
   Eupnp_HTTP_Message *h;
   h = calloc(1, sizeof(Eupnp_HTTP_Message));

   if (!h)
     {
	eina_error_set(EINA_ERROR_OUT_OF_MEMORY);
	ERROR("Could not create HTTP message.\n");
	return NULL;
     }

   h->headers = eina_array_new(10);

   if (!h->headers)
     {
	ERROR("Could not allocate memory for HTTP headers table.\n");
	return NULL;
     }

   h->method = eina_stringshare_add_length(method_start, method_len);
   h->http_version = eina_stringshare_add_length(httpver_start, http_version_len);
   h->resource = eina_stringshare_add_length(resource_start, resource_len);
   return h;
}


static void
eupnp_ssdp_http_message_free(Eupnp_HTTP_Message *m)
{
   if (!m)
      return;

   eina_stringshare_del(m->method);
   eina_stringshare_del(m->http_version);
   eina_stringshare_del(m->resource);

   if (m->headers)
     {
	Eina_Array_Iterator it;
	Eupnp_HTTP_Header *h;
	int i;

	EINA_ARRAY_ITER_NEXT(m->headers, i, h, it)
	   eupnp_ssdp_http_header_free(h);

	eina_array_free(m->headers);
     }

   free(m);
}

static Eupnp_HTTP_Message *
eupnp_ssdp_datagram_parse(const char *buf)
{
   Eupnp_HTTP_Message *m;
   const char *method;
   const char *httpver;
   const char *resource;
   const char *begin, *end;
   const char *vbegin, *vend;
   int method_len, httpver_len, resource_len, klen, vlen;
   int i;

   /* Parse the HTTP request line */
   method = buf;
   end = strchr(method, ' ');

   if (!end)
     {
	ERROR("Could not parse HTTP method.\n");
	return NULL;
     }

   method_len = end - method;

   /* Move our starting point to the resource */
   resource = end + 1;
   end = strchr(resource, ' ');

   if (!end)
     {
	ERROR("Could not parse HTTP request.\n");
	return NULL;
     }
   resource_len = end - resource;

   httpver = end + 1;
   end = strstr(httpver, "\r\n");

   if (!end)
     {
	ERROR("Could not parse HTTP request.\n");
	return NULL;
     }

   httpver_len = end - httpver;

   m = eupnp_ssdp_http_message_new(method, method_len, resource, resource_len,
				   httpver, httpver_len);

   if (!m)
     {
	ERROR("Could not create HTTP message.\n");
	return NULL;
     }

   /* Parse the available headers */
   begin = end + 2;

   while (begin != NULL)
     {
	/*
	 * TODO add payload support by checking presence of doubled "\r\n"
	 */

	/* Header name */
	end = strchr(begin, ':');
	if (!end) break;
	klen = end - begin;

	/* Header value */
	vbegin = end + 2;
	vend = strstr(vbegin, "\r\n");

	if (!vend)
	  {
	     /* Malformed header not ending in \r\n. If couldn't find \r\n, also
	      * means that there are no more headers forward.
	      */
	     break;
	  }

	vlen = vend - vbegin;

	if (!eupnp_ssdp_http_message_add_header(m, begin, klen, vbegin, vlen))
	  {
	     WARN("out of memory parsing headers, skipping rest...\n");
	     break;
	  }

	begin = vend + 2;
     }

   return m;
}


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
   Eupnp_HTTP_Message *m;
   Eupnp_UDP_Datagram *d;

   d = eupnp_udp_transport_recvfrom(ssdp->udp_sock);
   DEBUG("Message from %s:%d\n", d->host, d->port);

   if (!d)
     {
	ERROR("Could not retrieve a valid datagram\n");
	return;
     }

   m = eupnp_ssdp_datagram_parse(d->data);
   eupnp_udp_transport_datagram_free(d);

   if (!m)
     {
	ERROR("Failed parsing request datagram\n");
	return;
     }

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

   eupnp_ssdp_http_message_free(m);
}
