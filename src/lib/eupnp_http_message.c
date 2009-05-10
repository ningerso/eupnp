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

#include <stdlib.h>
#include <string.h>
#include <Eina.h>

#include "eupnp_error.h"
#include "eupnp_http_message.h"

/*
 * Private API
 */

/*
 * Parses the first line of a HTTP message
 *
 * Parses first line of the form "a<SP>b<SP>c<SP>" and stores the points on the
 * pointers @p a, @p b and @p c given. Also marks @p headers_start on the
 * beginning of the headers 
 */
static Eina_Bool
eupnp_http_datagram_line_parse(const char *msg, const char **headers_start, const char **a, int *a_len, const char **b, int *b_len, const char **c, int *c_len)
{
   /*
    * Parse first line of the form "a SP b SP c\r\n"
    */
   const char *begin, *end;
   const char *vbegin, *vend;

   *a = msg;
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

/*
 * Parses HTTP headers
 *
 * Given the starting point, parses the next header and sets the starting point
 * to the next header, if present. Sets the given pointers to the parsed key
 * and value.
 */
static Eina_Bool
eupnp_http_datagram_header_next_parse(const char **line_start, const char **hkey, int *hkey_len, const char **hvalue, int *hvalue_len)
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

/*
 * Public API
 */

/*
 * Constructor for the Eupnp_HTTP_Header structure
 *
 * Receives pointers to key and value starting points and the length that will
 * be copied from that point on (length).
 *
 * @param key starting point of the key
 * @param key_len key length
 * @param value starting point of the value
 * @param value_len value length
 *
 * @return Eupnp_HTTP_Header instance
 */
Eupnp_HTTP_Header *
eupnp_http_header_new(const char *key, int key_len, const char *value, int value_len)
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
   memcpy((void *)h->key, key, key_len);
   memcpy((void *)h->value, value, value_len);
   ((char *) h->key)[key_len] = '\0';
   ((char *) h->value)[value_len] = '\0';

   return h;
}

/*
 * Destructor for the Eupnp_HTTP_Header structure
 *
 * Frees the object and its attributes.
 *
 * @param h previously created header
 */
void
eupnp_http_header_free(Eupnp_HTTP_Header *h)
{
   if (h) free(h);
}

/*
 * Constructor for the Eupnp_HTTP_Request structure
 *
 * Receives pointers to starting points and lengths of the method, uri
 * and http version attributes and constructs the object. Also initializes the
 * headers array. For dealing with headers, refer to the
 * eupnp_http_request_header_* functions.
 *
 * @param method starting point of the method
 * @param method_len method length
 * @param uri starting point of the uri
 * @param uri_len uri length
 * @param httpver starting point of the http version
 * @param httpver_len http version length
 *
 * @return Eupnp_HTTP_Request instance
 */
Eupnp_HTTP_Request *
eupnp_http_request_new(const char *method, int method_len, const char *uri, int uri_len, const char *httpver, int httpver_len)
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

   h->method = eina_stringshare_add_length(method, method_len);

   if (!h->method)
     {
	ERROR("Could not stringshare HTTP request method.\n");
	eina_array_free(h->headers);
	free(h);
	return NULL;
     }

   h->http_version = eina_stringshare_add_length(httpver, httpver_len);

   if (!h->http_version)
     {
	ERROR("Could not stringshare HTTP request version.\n");
	eina_array_free(h->headers);
	eina_stringshare_del(h->method);
	free(h);
	return NULL;
     }

   h->uri = eina_stringshare_add_length(uri, uri_len);

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

/*
 * Destructor for the Eupnp_HTTP_Request structure
 *
 * Frees the object and its attributes, including headers added.
 *
 * @param r previously created request
 */
void
eupnp_http_request_free(Eupnp_HTTP_Request *r)
{
   if (!r)
      return;

   if (r->method)
      eina_stringshare_del(r->method);
   if (r->http_version)
      eina_stringshare_del(r->http_version);
   if (r->uri)
      eina_stringshare_del(r->uri);

   if (r->headers)
     {
	Eina_Array_Iterator it;
	Eupnp_HTTP_Header *h;
	int i;

	EINA_ARRAY_ITER_NEXT(r->headers, i, h, it)
	   eupnp_http_header_free(h);

	eina_array_free(r->headers);
     }

   free(r);
}


/*
 * Prints out info about the Eupnp_HTTP_Request object
 *
 * Use EINA_ERROR_LEVEL=3 for seeing the printed messages.
 *
 * @param r: previously created request
 */
void
eupnp_http_request_dump(Eupnp_HTTP_Request *r)
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

/*
 * Adds a header to a request
 *
 * Adds a header to the request object given pointers to the key
 * and value starting points and respective string lengths.
 *
 * @param m request to add the header into
 * @param key starting point for the key
 * @param key_len key length
 * @param value starting point for the value
 * @param value_len value length
 *
 * @note This function does not check duplicate headers. If added twice,
 *       header will be duplicated.
 *
 * @return On success returns EINA_TRUE, otherwise EINA_FALSE.
 */
Eina_Bool
eupnp_http_request_header_add(Eupnp_HTTP_Request *m, const char *key, int key_len, const char *value, int value_len)
{
   Eupnp_HTTP_Header *h;

   h = eupnp_http_header_new(key, key_len, value, value_len);

   if (!h)
     {
	eina_error_set(EINA_ERROR_OUT_OF_MEMORY);
	ERROR("header alloc error.\n");
	return EINA_FALSE;
     }

   if (!eina_array_push(m->headers, h))
     {
	WARN("incomplete headers\n");
	eupnp_http_header_free(h);
	return EINA_FALSE;
     }

   return EINA_TRUE;
}


/*
 * Constructor for the Eupnp_HTTP_Response structure
 *
 * Receives pointers to starting points and lengths of the http version, status
 * code and http version attributes and constructs the object. Also initializes
 * the headers array. For dealing with headers, refer to the
 * eupnp_http_response_header_* functions.
 *
 * @param httpver starting point of the http version
 * @param httpver_len http version length
 * @param status_code starting point of the status code
 * @param status_len status length
 * @param reason_phrase starting point of the reason phrase
 * @param reason_phrase_len reason phrase length
 *
 * @return Eupnp_HTTP_Response instance
 */
Eupnp_HTTP_Response *
eupnp_http_response_new(const char *httpver, int httpver_len, const char *status_code, int status_code_len, const char *reason_phrase, int reason_phrase_len)
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

   r->http_version = eina_stringshare_add_length(httpver, httpver_len);

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

/*
 * Destructor for the Eupnp_HTTP_Response structure
 *
 * Frees the object and its attributes, including headers added.
 *
 * @param r previously created response
 */
void
eupnp_http_response_free(Eupnp_HTTP_Response *r)
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
	   eupnp_http_header_free(h);

	eina_array_free(r->headers);
     }

   free(r);
}

/*
 * Prints out info about the Eupnp_HTTP_Response object
 *
 * Use EINA_ERROR_LEVEL=3 for seeing the printed messages.
 *
 * @param r: previously created response
 */
void
eupnp_http_response_dump(Eupnp_HTTP_Response *r)
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

/*
 * Adds a header to a response
 *
 * Adds a header to the response object given pointers to the key
 * and value starting points and respective string lengths.
 *
 * @param m request to add the header into
 * @param key starting point for the key
 * @param key_len key length
 * @param value starting point for the value
 * @param value_len value length
 *
 * @note This function does not check duplicate headers. If added twice,
 *       header will be duplicated.
 *
 * @return On success returns EINA_TRUE, otherwise EINA_FALSE.
 */
Eina_Bool
eupnp_http_response_header_add(Eupnp_HTTP_Response *r, const char *key, int key_len, const char *value, int value_len)
{
   Eupnp_HTTP_Header *h;

   h = eupnp_http_header_new(key, key_len, value, value_len);

   if (!h)
     {
	eina_error_set(EINA_ERROR_OUT_OF_MEMORY);
	ERROR("header alloc error.\n");
	return EINA_FALSE;
     }

   if (!eina_array_push(r->headers, h))
     {
	WARN("incomplete headers\n");
	eupnp_http_header_free(h);
	return EINA_FALSE;
     }

   return EINA_TRUE;
}


/*
 * Checks if a message type is response
 *
 * @p msg HTTP message
 *
 * @return EINA_TRUE if the message type is response, EINA_FALSE otherwise.
 */
Eina_Bool
eupnp_http_message_is_response(const char *msg)
{
   if (!strncmp(msg, EUPNP_HTTP_VERSION, EUPNP_HTTP_VERSION_LEN))
      return EINA_TRUE;
   return EINA_FALSE;
}

/*
 * Checks if a message type is request
 *
 * @p msg HTTP message
 *
 * @return EINA_TRUE if the message type is request, EINA_FALSE otherwise.
 */
Eina_Bool
eupnp_http_message_is_request(const char *msg)
{
   return (!eupnp_http_message_is_response(msg));
}

/*
 * Parses a request message and mounts the request object
 *
 * Parses a HTTP request (previously known to be a request, see
 * eupnp_http_message_is_request()) and returns the
 * request object with attributes already set.
 *
 * @param msg HTTP message
 *
 * @return Eupnp_HTTP_Request instance if parsed successfully, NULL otherwise.
 */
Eupnp_HTTP_Request *
eupnp_http_request_parse(const char *msg)
{
   Eupnp_HTTP_Request *r;
   const char *method;
   const char *uri;
   const char *http_version;
   const char *headers_start, *next_header;
   const char *hkey_begin, *hv_begin;
   int method_len, uri_len, httpver_len;
   int hk_len, hv_len;

   if (!eupnp_http_datagram_line_parse(msg, &headers_start, &method, &method_len, &uri, &uri_len, &http_version, &httpver_len))
     {
	ERROR("Could not parse request line.\n");
	return NULL;
     }

   r = eupnp_http_request_new(method, method_len, uri, uri_len,
				   http_version, httpver_len);

   if (!r)
     {
	ERROR("Could not create new HTTP request.\n");
	return NULL;
     }

   next_header = headers_start;

   while (next_header != NULL)
     {
	if (eupnp_http_datagram_header_next_parse(&next_header, &hkey_begin, &hk_len, &hv_begin, &hv_len))
	  {
	     if (!eupnp_http_request_header_add(r, hkey_begin, hk_len, hv_begin, hv_len))
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

/*
 * Parses a response message and mounts the response object
 *
 * Parses a HTTP response (previously known to be a response, see
 * eupnp_http_message_is_response()) and returns the
 * response object with attributes already set.
 *
 * @param msg HTTP message
 *
 * @return Eupnp_HTTP_Response instance if parsed successfully, NULL otherwise.
 */
Eupnp_HTTP_Response *
eupnp_http_response_parse(const char *msg)
{
   Eupnp_HTTP_Response *r;
   const char *reason_phrase;
   const char *status_code;
   const char *http_version;
   const char *headers_start, *next_header;
   const char *hkey_begin, *hv_begin;
   int sc_len, rp_len, httpver_len;
   int hk_len, hv_len;

   if (!eupnp_http_datagram_line_parse
		(msg, &headers_start, &http_version, &httpver_len, &status_code,
		 &sc_len, &reason_phrase, &rp_len))
     {
	ERROR("Could not parse response line.\n");
	return NULL;
     }

   r = eupnp_http_response_new(http_version, httpver_len, status_code,
				    sc_len, reason_phrase, rp_len);

   if (!r)
     {
	ERROR("Could not create new HTTP response.\n");
	return NULL;
     }

   next_header = headers_start;

   while (next_header != NULL)
     {
	if (eupnp_http_datagram_header_next_parse(&next_header, &hkey_begin, &hk_len, &hv_begin, &hv_len))
	  {
	     if (!eupnp_http_response_header_add(r, hkey_begin, hk_len, hv_begin, hv_len))
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
