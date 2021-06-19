find_package(FFTW3 REQUIRED)

set(SELM_SOURCE_DIR ${LAMMPS_LIB_SOURCE_DIR}/selm)
file(GLOB SELM_SOURCES ${SELM_SOURCE_DIR}/[^.]*.cpp)

option(SELM_PLUGINS "Enable support for SELM plugins" OFF)
if((CMAKE_SYSTEM_NAME STREQUAL "Windows") AND SELM_PLUGINS)
  message(FATAL_ERROR "SELM plugins are not supported on Windows")
endif()

set(SELM_DEFINES FFT_FFTW)
set(SELM_LIBRARIES MPI::MPI_CXX;FFTW3::FFTW3)
if(SELM_PLUGINS)
  list(APPEND SELM_DEFINES DYNAMIC_LINK_PLUG_IN)
  list(APPEND SELM_LIBRARIES ${CMAKE_DL_LIBS})
endif()

add_library(selm STATIC ${SELM_SOURCES})
set_target_properties(selm PROPERTIES OUTPUT_NAME lammps_selm${LAMMPS_MACHINE})
target_compile_definitions(selm PRIVATE ${SELM_DEFINES})
target_include_directories(selm PUBLIC ${SELM_SOURCE_DIR})
target_include_directories(selm PRIVATE ${LAMMPS_SOURCE_DIR} ${LAMMPS_SOURCE_DIR}/USER-SELM)
target_link_libraries(selm PRIVATE ${SELM_LIBRARIES})
target_link_libraries(lammps PRIVATE selm)
