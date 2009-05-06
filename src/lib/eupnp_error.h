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


#ifndef _EUPNP_ERROR_H
#define _EUPNP_ERROR_H

#include <Eina.h>

#define WARN(...) EINA_ERROR_PWARN(__VA_ARGS__)
#define DEBUG(...) EINA_ERROR_PDBG(__VA_ARGS__)
#define INFO(...) EINA_ERROR_PINFO(__VA_ARGS__)
#define ERROR(...) EINA_ERROR_PERR(__VA_ARGS__)


int eupnp_error_init(void);
int eupnp_error_shutdown(void);


#endif /* _EUPNP_ERROR_H */
