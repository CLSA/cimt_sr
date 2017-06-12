# - find DCMTK libraries and applications
#

#  DCMTK_INCLUDE_DIRS   - Directories to include to use DCMTK
#  DCMTK_LIBRARIES     - Files to link against to use DCMTK
#  DCMTK_FOUND         - If false, don't try to use DCMTK
#  DCMTK_DIR           - Source directory for DCMTK
#  DCMTK_ROOT          - Build directory for DCMTK  
#
# DCMTK_DIR can be used to make it simpler to find the various include
# directories and compiled libraries if you've just compiled it in the
# source tree. Just set it to the root of the tree where you extracted
# the source (default to /usr/include/dcmtk/)

#=============================================================================
# Copyright 2004-2009 Kitware, Inc.
# Copyright 2009-2011 Mathieu Malaterre <mathieu.malaterre@gmail.com>
# Copyright 2010 Thomas Sondergaard <ts@medical-insight.com>
#
# Distributed under the OSI-approved BSD License (the "License");
# see accompanying file Copyright.txt for details.
#
# This software is distributed WITHOUT ANY WARRANTY; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the License for more information.
#=============================================================================
# (To distributed this file outside of CMake, substitute the full
#  License text for the above reference.)

#
# Written for VXL by Amitha Perera.
# Upgraded for GDCM by Mathieu Malaterre.
# Modified for EasyViz by Thomas Sondergaard.
# Modified for Alder by Dean Inglis
#

if(NOT DCMTK_FOUND AND NOT DCMTK_DIR)
  set(DCMTK_DIR
    "/usr/include/dcmtk/"
    CACHE
    PATH
    "Root of DCMTK source tree.")
  mark_as_advanced(DCMTK_DIR)
endif()

if(NOT DCMTK_FOUND AND NOT DCMTK_ROOT)
  set( DCMTK_ROOT
    "/usr/include/dcmtk/"
    CACHE
    PATH
    "Root of DCMTK build tree.")
  mark_as_advanced(DCMTK_ROOT)
endif()

foreach(lib
  dcmdata
  dcmimage
  dcmimgle
  dcmjpeg
  dcmnet
  dcmpstat
  dcmqrdb
  dcmsign
  dcmsr
  dcmtls
  ijg12
  ijg16
  ijg8
  ofstd)

  find_library(DCMTK_${lib}_LIBRARY
    ${lib}
    PATHS
    ${DCMTK_ROOT}/${lib}/libsrc
    ${DCMTK_ROOT}/${lib}/libsrc/Release
    ${DCMTK_ROOT}/${lib}/libsrc/Debug
    ${DCMTK_ROOT}/${lib}/Release
    ${DCMTK_ROOT}/${lib}/Debug
    ${DCMTK_ROOT}/lib)

  mark_as_advanced(DCMTK_${lib}_LIBRARY)

  if(DCMTK_${lib}_LIBRARY)
    list(APPEND DCMTK_LIBRARIES ${DCMTK_${lib}_LIBRARY})
  endif()

endforeach()

set(DCMTK_config_TEST_HEADER osconfig.h)
set(DCMTK_dcmdata_TEST_HEADER dctypes.h)
set(DCMTK_dcmimage_TEST_HEADER dicoimg.h)
set(DCMTK_dcmimgle_TEST_HEADER dcmimage.h)
set(DCMTK_dcmjpeg_TEST_HEADER djdecode.h)
set(DCMTK_dcmnet_TEST_HEADER assoc.h)
set(DCMTK_dcmpstat_TEST_HEADER dcmpstat.h)
set(DCMTK_dcmqrdb_TEST_HEADER dcmqrdba.h)
set(DCMTK_dcmsign_TEST_HEADER sicert.h)
set(DCMTK_dcmsr_TEST_HEADER dsrtree.h)
set(DCMTK_dcmtls_TEST_HEADER tlslayer.h)
set(DCMTK_ofstd_TEST_HEADER ofstdinc.h)
set(DCMTK_oflog_TEST_HEADER oflog.h)

foreach(dir
  config
  dcmdata
  dcmimage
  dcmimgle
  dcmjpeg
  dcmnet
  dcmpstat
  dcmqrdb
  dcmsign
  dcmsr
  dcmtls
  ofstd
  oflog)

  find_path(DCMTK_${dir}_INCLUDE_DIR
    ${DCMTK_${dir}_TEST_HEADER}
    PATHS
    ${DCMTK_DIR}/${dir}/include
    ${DCMTK_DIR}/${dir}
    ${DCMTK_DIR}/include/${dir}
    ${DCMTK_DIR}/${dir}/include/dcmtk/${dir}
    ${DCMTK_ROOT}/${dir}/include/dcmtk/${dir})

  mark_as_advanced(DCMTK_${dir}_INCLUDE_DIR)

  if(DCMTK_${dir}_INCLUDE_DIR)
  set(result "empty")
  string(REGEX REPLACE 
    /dcmtk/${dir}$ 
    "" 
    result
    ${DCMTK_${dir}_INCLUDE_DIR})

  if(NOT ${result} MATCHES "empty")
    set(DCMTK_${dir}_INCLUDE_DIR ${result})
  endif()
  
  list(APPEND
    DCMTK_INCLUDE_DIRS
    ${DCMTK_${dir}_INCLUDE_DIR})

  endif()
endforeach()

if(WIN32)
  list(APPEND DCMTK_LIBRARIES netapi32 wsock32)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(DCMTK DEFAULT_MSG
  DCMTK_config_INCLUDE_DIR
  DCMTK_ofstd_INCLUDE_DIR
  DCMTK_ofstd_LIBRARY
  DCMTK_dcmdata_INCLUDE_DIR
  DCMTK_dcmdata_LIBRARY
  DCMTK_dcmimgle_INCLUDE_DIR
  DCMTK_dcmimgle_LIBRARY)

# Compatibility: This variable is deprecated
set(DCMTK_INCLUDE_DIR ${DCMTK_INCLUDE_DIRS})

foreach(executable 
  dcmdump 
  dcmdjpeg
  dcmdrle
  dcmdjpls
  storescu
  echoscu
  movescu
  findscu
  dcmqrscp)
  
  string(TOUPPER ${executable} EXECUTABLE)
  find_program(DCMTK_${EXECUTABLE}_EXECUTABLE 
    ${executable} 
    ${DCMTK_DIR}/bin)
  mark_as_advanced(DCMTK_${EXECUTABLE}_EXECUTABLE)
endforeach()
