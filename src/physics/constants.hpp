/* -*- indent-tabs-mode: t -*- */

#ifndef INQ__PHYSICS__CONSTANTS
#define INQ__PHYSICS__CONSTANTS

/*
 Copyright (C) 2020 Xavier Andrade

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU Lesser General Public License as published by
 the Free Software Foundation; either version 3 of the License, or
 (at your option) any later version.
  
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU Lesser General Public License for more details.
  
 You should have received a copy of the GNU Lesser General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

namespace inq {
namespace physics {
namespace constants {

constexpr double proton_charge = -1.0;

}
}
}

#ifdef INQ_PHYSICS_CONSTANTS_UNIT_TEST
#undef INQ_PHYSICS_CONSTANTS_UNIT_TEST
#endif

#endif


