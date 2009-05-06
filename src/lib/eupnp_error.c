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

#include "eupnp_error.h"


static int _eupnp_error_init_count = 0;


int
eupnp_error_init(void)
{
   if (_eupnp_error_init_count) return ++_eupnp_error_init_count;

   if (!eina_error_init()) return 0;

   return ++_eupnp_error_init_count;
}

int
eupnp_error_shutdown(void)
{
   if (_eupnp_error_init_count != 1) return --_eupnp_error_init_count;

   eina_error_shutdown();

   return --_eupnp_error_init_count;
}
