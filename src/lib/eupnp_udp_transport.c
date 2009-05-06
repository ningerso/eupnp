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
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include <eupnp_error.h>
#include <eupnp_udp_transport.h>


/*
 * Private API
 */

static Eupnp_UDP_Datagram *
eupnp_udp_datagram_new(const char *host, int port, char *data)
{
   Eupnp_UDP_Datagram *datagram;

   datagram = calloc(1, sizeof(Eupnp_UDP_Datagram));

   if (!datagram)
     {
	eina_error_set(EINA_ERROR_OUT_OF_MEMORY);
	ERROR("datagram alloc failed.\n");
	return NULL;
     }

   if (data)
      datagram->data = data;
   else
      datagram->data = calloc(1, EUPNP_UDP_PACKET_SIZE);
   if (host)
      datagram->host = host;
   if (port)
      datagram->port = port;
   return datagram;
}

static void
eupnp_udp_datagram_free(Eupnp_UDP_Datagram *datagram)
{
   if (!datagram) return;
   if (datagram->data) free(datagram->data);
   free(datagram);
}

static Eina_Bool
eupnp_udp_transport_prepare(Eupnp_UDP_Transport *s)
{
   int reuse_addr = 1; // yes

   if (fcntl(s->socket, F_SETFL, O_NONBLOCK) < 0)
     {
 	ERROR(" Error setting nonblocking option on socket. %s\n",
	      strerror(errno));
	return EINA_FALSE;
     }

   if (bind(s->socket, (struct sockaddr *) &s->in_addr, s->in_addr_len) < 0)
     {
 	ERROR("Error binding. %s\n", strerror(errno));
	return EINA_FALSE;
     }

   if (setsockopt(s->socket, SOL_SOCKET, SO_REUSEADDR, &reuse_addr, sizeof(int)) < 0)
     {
	ERROR("setsockopt SO_REUSE_ADDR failed. %s\n", strerror(errno));
	return EINA_FALSE;
     }

   if (setsockopt(s->socket, IPPROTO_IP, IP_ADD_MEMBERSHIP,&s->mreq,
		   sizeof(struct ip_mreq)) < 0)
     {
	ERROR("setsockopt IP_ADD_MEMBERSHIP failed. %s\n", strerror(errno));
	return EINA_FALSE;
     }

   return EINA_TRUE;
}


/*
 * Public API
 */

Eupnp_UDP_Transport *
eupnp_udp_transport_new(const char *addr, int port, const char *iface_addr)
{
   Eupnp_UDP_Transport *s;

   s = calloc(1, sizeof(Eupnp_UDP_Transport));
   s->socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

   if (s->socket < 0)
     {
	ERROR("Error creating socket. %s\n", strerror(errno));
	free(s);
	return NULL;
     }

   memset(&s->in_addr, 0, sizeof(s->in_addr));
   s->in_addr.sin_family = AF_INET;
   s->in_addr.sin_port = htons(port);
   s->in_addr.sin_addr.s_addr = inet_addr(iface_addr);
   s->in_addr_len = sizeof(struct sockaddr_in);
   s->mreq.imr_multiaddr.s_addr = inet_addr(addr);
   s->mreq.imr_interface.s_addr = htonl(INADDR_ANY);

   if (!eupnp_udp_transport_prepare(s))
     {
	ERROR("Could not prepare socket.\n");
	free(s);
	return NULL;
     }

   return s;
}

int
eupnp_udp_transport_close(Eupnp_UDP_Transport *s)
{
   if (!s) return -1;
   return close(s->socket);
}

void
eupnp_udp_transport_free(Eupnp_UDP_Transport *s)
{
   if (s) free(s);
}

Eupnp_UDP_Datagram *
eupnp_udp_transport_recv(Eupnp_UDP_Transport *s)
{
   Eupnp_UDP_Datagram *d;

   d = eupnp_udp_datagram_new("", 0, NULL);

   if (!d) return NULL;

   recv(s->socket, d->data, EUPNP_UDP_PACKET_SIZE, 0);

   return d;
}

Eupnp_UDP_Datagram *
eupnp_udp_transport_recvfrom(Eupnp_UDP_Transport *s)
{
   Eupnp_UDP_Datagram *d;

   d = eupnp_udp_datagram_new("", 0, NULL);

   if (!d) return NULL;

   recvfrom(s->socket, d->data, EUPNP_UDP_PACKET_SIZE, 0,
	    (struct sockaddr *)&(s->in_addr), &(s->in_addr_len));
   d->host = inet_ntoa(s->in_addr.sin_addr);
   d->port = ntohs(s->in_addr.sin_port);

   return d;
}

int
eupnp_udp_transport_sendto(Eupnp_UDP_Transport *s, const void *buffer, const char *addr, int port)
{
   int cnt;

   s->in_addr.sin_family = AF_INET;
   s->in_addr.sin_port = htons(port);

   if (inet_aton(addr, &(s->in_addr.sin_addr)) == 0)
     {
	ERROR("could not convert address. %s\n", strerror(errno));
	return -1;
     }

   cnt = sendto(s->socket, buffer,
		strlen((char *)buffer)*sizeof(char), 0,
		(struct sockaddr *)&(s->in_addr), s->in_addr_len);

   return cnt;
}

void
eupnp_udp_transport_datagram_free(Eupnp_UDP_Datagram *datagram)
{
   eupnp_udp_datagram_free(datagram);
}


