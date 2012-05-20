# Copyright 2012 OSMOCOM Project
#
# This file is part of OsmoSDR
#
# GNU Radio is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3, or (at your option)
# any later version.
#
# GNU Radio is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with GNU Radio; see the file COPYING.  If not, write to
# the Free Software Foundation, Inc., 51 Franklin Street,
# Boston, MA 02110-1301, USA.

########################################################################
# Setup library
########################################################################
add_library(osmosdr_shared SHARED
    libosmosdr.c
)

target_link_libraries(osmosdr_shared
    ${LIBUSB_LIBRARIES}
)

set_target_properties(osmosdr_shared PROPERTIES DEFINE_SYMBOL "osmosdr_EXPORTS")
set_target_properties(osmosdr_shared PROPERTIES OUTPUT_NAME osmosdr)
set_target_properties(osmosdr_shared PROPERTIES SOVERSION 0 VERSION 0.0.0)

add_library(osmosdr_static STATIC
    libosmosdr.c
)

target_link_libraries(osmosdr_static
    ${LIBUSB_LIBRARIES}
)

set_property(TARGET osmosdr_static APPEND PROPERTY COMPILE_DEFINITIONS "osmosdr_STATIC" )

if(NOT WIN32)
# Force same library filename for static and shared variants of the library
set_target_properties(osmosdr_static PROPERTIES OUTPUT_NAME osmosdr)
endif()

########################################################################
# Build utility
########################################################################
add_executable(osmo_sdr osmo_sdr.c)
target_link_libraries(osmo_sdr osmosdr_static
    ${LIBUSB_LIBRARIES}
)

if(WIN32)
set_property(TARGET osmo_sdr APPEND PROPERTY COMPILE_DEFINITIONS "osmosdr_STATIC" )
endif()

########################################################################
# Install built library files & utilities
########################################################################
install(TARGETS osmosdr_shared osmosdr_static osmo_sdr
    LIBRARY DESTINATION lib${LIB_SUFFIX} # .so/.dylib file
    ARCHIVE DESTINATION lib${LIB_SUFFIX} # .lib file
    RUNTIME DESTINATION bin              # .dll file
)
