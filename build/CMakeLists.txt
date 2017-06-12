CMAKE_MINIMUM_REQUIRED( VERSION 2.6 )
PROJECT( CIMT )

# Look in the build directory for cmake modules
set( CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${PROJECT_SOURCE_DIR}/" )

# We need VTK
FIND_PACKAGE( VTK REQUIRED )
INCLUDE( ${VTK_USE_FILE} )
IF("${VTK_MAJOR_VERSION}.${VTK_MINOR_VERSION}" VERSION_LESS 6.0)
  MESSAGE(FATAL_ERROR "VTK 6.0 or later is required.")
ENDIF()

find_package(DCMTK REQUIRED)
include_directories(${DCMTK_INCLUDE_DIRS})

# We need vtk-dicom
FIND_PACKAGE( DICOM REQUIRED )
IF( DICOM_FOUND )
 INCLUDE( ${DICOM_USE_FILE} )
ENDIF()

SET( CIMT_ROOT_DIR ${PROJECT_SOURCE_DIR}/.. )
SET( CIMT_SRC_DIR ${CIMT_ROOT_DIR}/src )

SET( CMAKE_CXX_FLAGS "-std=c++0x " )

# Targets
ADD_EXECUTABLE( cimt_sr_decode ${CIMT_SRC_DIR}/cimt_sr_decode.cxx )

SET(VTK_LIBS
  vtkCommonCore
  vtkCommonSystem
  vtksys
)

TARGET_LINK_LIBRARIES( cimt_sr_decode
  vtkDICOM
  ${VTK_LIBS}
)

ADD_EXECUTABLE( cimt_sr_measurement ${CIMT_SRC_DIR}/cimt_sr_measurement.cxx )

TARGET_LINK_LIBRARIES( cimt_sr_measurement
  vtkDICOM
  ${VTK_LIBS}
)

ADD_EXECUTABLE( cimt_sr_dcmtk ${CIMT_SRC_DIR}/cimt_sr_DCMTK.cxx )

TARGET_LINK_LIBRARIES( cimt_sr_dcmtk
  ${DCMTK_LIBRARIES}
)

ADD_EXECUTABLE( cimt_sr_pair_files_dcmtk
${CIMT_SRC_DIR}/cimt_sr_pair_files_dcmtk.cxx  
${CIMT_SRC_DIR}/options.cxx  
)

TARGET_LINK_LIBRARIES( cimt_sr_pair_files_dcmtk
  ${DCMTK_LIBRARIES}
  ${VTK_LIBS}
)

ADD_EXECUTABLE( cimt_sr_pair_files 
${CIMT_SRC_DIR}/cimt_sr_pair_files.cxx  
${CIMT_SRC_DIR}/options.cxx  
)

TARGET_LINK_LIBRARIES( cimt_sr_pair_files
  vtkDICOM
  ${VTK_LIBS}
)
