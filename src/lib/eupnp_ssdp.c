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

struct _Eupnp_HTTP_Request {
   Eina_Array *headers;
   const char *method;
   const char *uri;
   const char *http_version;
};

struct _Eupnp_HTTP_Response {
   Eina_Array *headers;
   const char *http_version;
   const char *reason_phrase;
   int status_code;
};

typedef struct _Eupnp_HTTP_Request Eupnp_HTTP_Request;
typedef struct _Eupnp_HTTP_Response Eupnp_HTTP_Response;
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
eupnp_ssdp_http_request_header_add(Eupnp_HTTP_Request *m, const char *key_start, int key_len, const char *value_start, int value_len)
{
   Eupnp_HTTP_Header *h;

   h = eupnp_ssdp_http_header_new(key_start, key_len, value_start, value_len);

   if (!h)
     {
	eina_error_set(EINA_ERROR_OUT_OF_MEMORY);
	ERROR("header alloc error.\n");
	return EINA_FALSE;
     }

   if (!eina_array_push(m->headers, h))
     {
	WARN("incomplete headers\n");
	eupnp_ssdp_http_header_free(h);
	return EINA_FALSE;
     }

   return EINA_TRUE;
}

static Eina_Bool
eupnp_ssdp_http_response_header_add(Eupnp_HTTP_Response *m, const char *key_start, int key_len, const char *value_start, int value_len)
{
   Eupnp_HTTP_Header *h;

   h = eupnp_ssdp_http_header_new(key_start, key_len, value_start, value_len);

   if (!h)
     {
	eina_error_set(EINA_ERROR_OUT_OF_MEMORY);
	ERROR("header alloc error.\n");
	return EINA_FALSE;
     }

   if (!eina_array_push(m->headers, h))
     {
	WARN("incomplete headers\n");
	eupnp_ssdp_http_header_free(h);
	return EINA_FALSE;
     }

   return EINA_TRUE;
}

static Eupnp_HTTP_Request *
eupnp_ssdp_http_request_new(const char *method_start, int method_len, const char *uri_start, int uri_len, const char *httpver_start, int httpver_len)
{
   Eupnp_HTTP_Request *h;
   h = calloc(1, sizeof(Eupnp_HTTP_Request));

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
	free(h);
	return NULL;
     }

   h->method = eina_stringshare_add_length(method_start, method_len);

   if (!h->method)
     {
	ERROR("Could not stringshare HTTP request method.\n");
	eina_array_free(h->headers);
	free(h);
	return NULL;
     }

   h->http_version = eina_stringshare_add_length(httpver_start, httpver_len);

   if (!h->http_version)
     {
	ERROR("Could not stringshare HTTP request version.\n");
	eina_array_free(h->headers);
	eina_stringshare_del(h->method);
	free(h);
	return NULL;
     }

   h->uri = eina_stringshare_add_length(uri_start, uri_len);

   if (!h->uri)
     {
	ERROR("Could not stringshare HTTP request URI.\n");
	eina_array_free(h->headers);
	eina_stringshare_del(h->method);
	eina_stringshare_del(h->http_version);
	free(h);
	return NULL;
     }

   return h;
}


static void
eupnp_ssdp_http_request_free(Eupnp_HTTP_Request *m)
{
   if (!m)
      return;

   if (m->method)
      eina_stringshare_del(m->method);
   if (m->http_version)
      eina_stringshare_del(m->http_version);
   if (m->uri)
      eina_stringshare_del(m->uri);

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

static void
eupnp_ssdp_http_request_dump(Eupnp_HTTP_Request *r)
{
   if (!r)
      return;

   DEBUG("Dumping HTTP request\n");
   if (r->method)
      DEBUG("* Method: %s\n", r->method);
   if (r->uri)
      DEBUG("* URI: %s\n", r->uri);
   if (r->http_version)
      DEBUG("* HTTP Version: %s\n", r->http_version);

   if (r->headers)
     {
	Eina_Array_Iterator it;
	Eupnp_HTTP_Header *h;
	int i;

	EINA_ARRAY_ITER_NEXT(r->headers, i, h, it)
	   DEBUG("** %s: %s\n", h->key, h->value);
     }
}


static Eupnp_HTTP_Response *
eupnp_ssdp_http_response_new(const char *httpver_start, int httpver_len, const char *status_code, int status_code_len, const char *reason_phrase, int reason_phrase_len)
{
   Eupnp_HTTP_Response *r;
   r = calloc(1, sizeof(Eupnp_HTTP_Response));

   if (!r)
     {
	eina_error_set(EINA_ERROR_OUT_OF_MEMORY);
	ERROR("Could not create HTTP response.\n");
	return NULL;
     }

   r->headers = eina_array_new(10);

   if (!r->headers)
     {
	ERROR("Could not allocate memory for HTTP headers.\n");
	free(r);
	return NULL;
     }

   r->http_version = eina_stringshare_add_length(httpver_start, httpver_len);

   if (!r->http_version)
     {
	ERROR("Could not stringshare http response version.\n");
	eina_array_free(r->headers);
	free(r);
	return NULL;
     }

   r->status_code = strtol(status_code, NULL, 10);
   r->reason_phrase = eina_stringshare_add_length(reason_phrase,
						    reason_phrase_len);

   if (!r->reason_phrase)
     {
	ERROR("Could not stringshare http response phrase.\n");
	eina_array_free(r->headers);
	eina_stringshare_del(r->http_version);
	free(r);
	return NULL;
     }

   return r;
}

static void
eupnp_ssdp_http_response_free(Eupnp_HTTP_Response *r)
{
   if (!r) return;

   if (r->http_version)
      eina_stringshare_del(r->http_version);
   if (r->reason_phrase)
      eina_stringshare_del(r->reason_phrase);

   if (r->headers)
     {
	Eina_Array_Iterator it;
	Eupnp_HTTP_Header *h;
	int i;

	EINA_ARRAY_ITER_NEXT(r->headers, i, h, it)
	   eupnp_ssdp_http_header_free(h);

	eina_array_free(r->headers);
     }

   free(r);
}

static void
eupnp_ssdp_http_response_dump(Eupnp_HTTP_Response *r)
{
   if (!r)
      return;

   DEBUG("Dumping HTTP response\n");
   if (r->http_version)
      DEBUG("* HTTP Version: %s\n", r->http_version);
   if (r->status_code)
      DEBUG("* Status Code: %d\n", r->status_code);
   if (r->reason_phrase)
      DEBUG("* Reason Phrase: %s\n", r->reason_phrase);

   if (r->headers)
     {
	Eina_Array_Iterator it;
	Eupnp_HTTP_Header *h;
	int i;

	EINA_ARRAY_ITER_NEXT(r->headers, i, h, it)
	   DEBUG("** %s: %s\n", h->key, h->value);
     }
}

static Eina_Bool
eupnp_ssdp_datagram_line_parse(const char *buf, const char **headers_start, const char **a, int *a_len, const char **b, int *b_len, const char **c, int *c_len)
{
   /*
    * Parse first line of the form "a SP b SP c\r\n"
    */
   const char *begin, *end;
   const char *vbegin, *vend;

   *a = buf;
   end = strchr(*a, ' ');

   if (!end)
     {
	ERROR("Could not parse DATAGRAM.\n");
	return EINA_FALSE;
     }

   *a_len = end - *a;

   /* Move our starting point to b */
   *b = end + 1;
   end = strchr(*b, ' ');

   if (!end)
     {
	ERROR("Could not parse HTTP.\n");
	return EINA_FALSE;
     }

   *b_len = end - *b;
   *c = end + 1;
   end = strstr(*c, "\r\n");

   if (!end)
     {
	ERROR("Could not parse HTTP request.\n");
	return EINA_FALSE;
     }

   *c_len = end - *c;
   *headers_start = end + 2;

   return EINA_TRUE;
}

static Eina_Bool
eupnp_ssdp_datagram_header_next_parse(const char **line_start, const char **hkey, int *hkey_len, const char **hvalue, int *hvalue_len)
{
   const char *end;

   if (!line_start)
      return EINA_FALSE;

   *hkey = *line_start;

   // Find first ':'. Do not trim spaces between the key and ':' - not on
   // RFC2616.
   end = strchr(*hkey, ':');

   if (!end)
     {
	*line_start = NULL;
	return EINA_FALSE;
     }

   *hkey_len = end - *hkey;

   // Move to the first char after ':' and check if the header value is empty.
   *hvalue = end + 1;

   if (**hvalue == '\r' && *(*hvalue+1) == '\n')
     {
	DEBUG("Empty header value!\n");
	*line_start = *hvalue + 2;
	*hvalue_len = 0;
	return EINA_TRUE;
     }

   // Header value not empty, skip whitespaces before the actual value.
   while (**hvalue == ' ') (*hvalue)++;

   if (**hvalue == '\r' && *(*hvalue+1) == '\n')
     {
	DEBUG("Empty header value!\n");
	*line_start = *hvalue + 2;
	*hvalue_len = 0;
	return EINA_TRUE;
     }

   // Mark value and skip possible whitespaces between value and \r\n
   end = *hvalue;

   while (*(end+1) != '\r') end++;

   if (*(end+2) != '\n')
     {
	ERROR("Header parsing error: character after carrier is not \\n\n");
	return EINA_FALSE;
     }

   *hvalue_len = end - *hvalue + 1;

   /* Set line_start for next header */
   *line_start = end + 3;

   return EINA_TRUE;
}

static Eina_Bool
eupnp_ssdp_http_message_is_response(const char *buf)
{
   if (!strncmp(buf, _eupnp_ssdp_http_version, EUPNP_SSDP_HTTP_VERSION_LEN))
      return EINA_TRUE;
   return EINA_FALSE;
}

static Eupnp_HTTP_Request *
eupnp_ssdp_http_request_parse(const char *buf)
{
   Eupnp_HTTP_Request *r;
   const char *method;
   const char *uri;
   const char *http_version;
   const char *headers_start, *next_header;
   const char *hkey_begin, *hv_begin;
   int method_len, uri_len, httpver_len;
   int hk_len, hv_len;

   if (!eupnp_ssdp_datagram_line_parse(buf, &headers_start, &method, &method_len, &uri, &uri_len, &http_version, &httpver_len))
     {
	ERROR("Could not parse request line.\n");
	return NULL;
     }

   r = eupnp_ssdp_http_request_new(method, method_len, uri, uri_len,
				   http_version, httpver_len);

   if (!r)
     {
	ERROR("Could not create new HTTP request.\n");
	return NULL;
     }

   next_header = headers_start;

   while (next_header != NULL)
     {
	if (eupnp_ssdp_datagram_header_next_parse(&next_header, &hkey_begin, &hk_len, &hv_begin, &hv_len))
	  {
	     if (!eupnp_ssdp_http_request_header_add(r, hkey_begin, hk_len, hv_begin, hv_len))
	       {
		  ERROR("Could not add header to the request.\n");
		  break;
	       }
	  }
	else
	  {
	     DEBUG("Finished parsing headers.\n");
	     break;
	  }
     }

   return r;
}


static Eupnp_HTTP_Response *
eupnp_ssdp_http_response_parse(const char *buf)
{
   Eupnp_HTTP_Response *r;
   const char *reason_phrase;
   const char *status_code;
   const char *http_version;
   const char *headers_start, *next_header;
   const char *hkey_begin, *hv_begin;
   int sc_len, rp_len, httpver_len;
   int hk_len, hv_len;

   if (!eupnp_ssdp_datagram_line_parse
		(buf, &headers_start, &http_version, &httpver_len, &status_code,
		 &sc_len, &reason_phrase, &rp_len))
     {
	ERROR("Could not parse request line.\n");
	return NULL;
     }

   r = eupnp_ssdp_http_response_new(http_version, httpver_len, status_code,
				    sc_len, reason_phrase, rp_len);

   if (!r)
     {
	ERROR("Could not create new HTTP response.\n");
	return NULL;
     }

   next_header = headers_start;

   while (next_header != NULL)
     {
	if (eupnp_ssdp_datagram_header_next_parse(&next_header, &hkey_begin, &hk_len, &hv_begin, &hv_len))
	  {
	     if (!eupnp_ssdp_http_response_header_add(r, hkey_begin, hk_len, hv_begin, hv_len))
	       {
		  ERROR("Could not add header to the response.\n");
		  break;
	       }
	  }
	else
	   break;
     }

   return r;
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
   _eupnp_ssdp_http_version = (char *) eina_stringshare_add(EUPNP_SSDP_HTTP_VERSION);

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

   if (eupnp_ssdp_http_message_is_response(d->data))
     {
	DEBUG("Message is response!\n");

	Eupnp_HTTP_Response *r;
	r = eupnp_ssdp_http_response_parse(d->data);
	eupnp_ssdp_http_response_dump(r);
	eupnp_ssdp_http_response_free(r);
     }
   else
     {
	DEBUG("Message is request!\n");
	Eupnp_HTTP_Request *m;
	m = eupnp_ssdp_http_request_parse(d->data);

	if (!m)
	  {
	     ERROR("Failed parsing request datagram\n");
	     return;
	  }

	eupnp_ssdp_http_request_dump(m);

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

	eupnp_ssdp_http_request_free(m);
     }

   eupnp_udp_transport_datagram_free(d);
}
