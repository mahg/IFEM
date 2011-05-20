IF(VTFWRITER_INCLUDES AND VTFWRITER_LIBRARIES)
  SET(VTFWRITER_FIND_QUIETLY TRUE)
ENDIF(VTFWRITER_INCLUDES AND VTFWRITER_LIBRARIES)

FIND_PATH(VTFWRITER_INCLUDES
  NAMES VTFAPI.h
  PATHS $ENV{HOME}/include
  /sima/libs/GLviewExpressWriter/include
)

FIND_LIBRARY(VTFWRITER_LIBRARIES
  NAMES VTFExpressAPI
  PATHS $ENV{HOME}/lib
  /sima/libs/GLviewExpressWriter/lib
)

INCLUDE(FindPackageHandleStandardArgs)
find_package_handle_standard_args(VTFWRITER DEFAULT_MSG
                                  VTFWRITER_INCLUDES VTFWRITER_LIBRARIES)

MARK_AS_ADVANCED(VTFWRITER_INCLUDES VTFWRITER_LIBRARIES)