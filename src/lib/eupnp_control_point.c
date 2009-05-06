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
#include <eupnp_ssdp.h>
#include <eupnp_error.h>
#include "eupnp_control_point.h"


static int _eupnp_control_point_main_count = 0;


int
eupnp_control_point_init(void)
{
   if (_eupnp_control_point_main_count)
      return ++_eupnp_control_point_main_count;

   if (!eupnp_ssdp_init())
     {
	fprintf(stderr, "Failed to initialize eupnp ssdp module\n");
	return _eupnp_control_point_main_count;
     }

   if (!eupnp_error_init())
     {
	fprintf(stderr, "Failed to initialize eupnp error module\n");
	eupnp_ssdp_shutdown();
	return _eupnp_control_point_main_count;
     }

   return ++_eupnp_control_point_main_count;
}

int
eupnp_control_point_shutdown(void)
{
   if (_eupnp_control_point_main_count != 1)
      return --_eupnp_control_point_main_count;

   eupnp_ssdp_shutdown();
   eupnp_error_shutdown();

   return --_eupnp_control_point_main_count;
}



Eupnp_Control_Point *
eupnp_control_point_new(void)
{
   Eupnp_Control_Point *c;

   c = calloc(1, sizeof(Eupnp_Control_Point));

   if (!c)
     {
	eina_error_set(EINA_ERROR_OUT_OF_MEMORY);
	ERROR("Could not create control point.\n");
	return NULL;
     }

   c->ssdp_server = eupnp_ssdp_server_new();

   if (!c->ssdp_server)
     {
	ERROR("Could not create control point.\n");
	free(c);
	return NULL;
     }

   return c;
}

void
eupnp_control_point_free(Eupnp_Control_Point *c)
{
   if (!c)
      return;

   if (c->ssdp_server) eupnp_ssdp_server_free(c->ssdp_server);
   free(c);
}

Eina_Bool
eupnp_control_point_discovery_request_send(Eupnp_Control_Point *c, int mx, char *search_target)
{
    return eupnp_ssdp_discovery_request_send(c->ssdp_server, mx, search_target);
}

