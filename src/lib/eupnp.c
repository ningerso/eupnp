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
#include <eupnp.h>


static int _eupnp_main_count = 0;


int
eupnp_init(void)
{
   if (_eupnp_main_count) return ++_eupnp_main_count;

   if (!eupnp_error_init())
     {
	fprintf(stderr, "Failed to initialize eina error module\n");
	return _eupnp_main_count;
     }

   if (!eupnp_ssdp_init())
     {
	fprintf(stderr, "Failed to initialize eina array module\n");
	eupnp_error_shutdown();
	return _eupnp_main_count;
     }

   if (!eupnp_control_point_init())
     {
	fprintf(stderr, "Failed to initialize eupnp control point module\n");
	eupnp_error_shutdown();
	eupnp_ssdp_shutdown();
	return _eupnp_main_count;
     }

   return ++_eupnp_main_count;
}

int
eupnp_shutdown(void)
{
   if (_eupnp_main_count != 1) return --_eupnp_main_count;

   eupnp_control_point_shutdown();
   eupnp_ssdp_shutdown();
   eupnp_error_shutdown();

   return --_eupnp_main_count;
}

