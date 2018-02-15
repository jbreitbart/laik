/*
 * This file is part of the LAIK parallel container library.
 * Copyright (c) 2018 Jens Breitbart, Josef Weidendorfer
 *
 * LAIK is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, version 3.
 *
 * LAIK is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _LAIK_BACKEND_GASPI_H_
#define _LAIK_BACKEND_GASPI_H_

#include "laik.h"

// GASPI backend.

// create a LAIK instance for this backend.
Laik_Instance* laik_init_gaspi();

#endif // _LAIK_BACKEND_GASPI_H_
