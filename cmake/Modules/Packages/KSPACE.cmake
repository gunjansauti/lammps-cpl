option(FFT_SINGLE "Use single precision FFTs instead of double precision FFTs" OFF)
set(FFTW "FFTW3")
if(FFT_SINGLE)
  set(FFTW "FFTW3F")
  target_compile_definitions(lammps PRIVATE -DFFT_SINGLE)
endif()
find_package(${FFTW} QUIET)
if(${FFTW}_FOUND)
  set(FFT "FFTW3" CACHE STRING "FFT library for KSPACE package")
else()
  set(FFT "KISS" CACHE STRING "FFT library for KSPACE package")
endif()
set(FFT_VALUES KISS FFTW3 MKL)
set_property(CACHE FFT PROPERTY STRINGS ${FFT_VALUES})
validate_option(FFT FFT_VALUES)
string(TOUPPER ${FFT} FFT)

if(FFT STREQUAL "FFTW3")
  find_package(${FFTW} REQUIRED)
  target_compile_definitions(lammps PRIVATE -DFFT_FFTW3)
  target_link_libraries(lammps PRIVATE ${FFTW}::${FFTW})
  if(FFTW3_OMP_LIBRARIES OR FFTW3F_OMP_LIBRARIES)
    option(FFT_FFTW_THREADS "Use threaded FFTW library" ON)
  else()
    option(FFT_FFTW_THREADS "Use threaded FFT library" OFF)
  endif()

  if(FFT_FFTW_THREADS)
    if(FFTW3_OMP_LIBRARIES OR FFTW3F_OMP_LIBRARIES)
      target_compile_definitions(lammps PRIVATE -DFFT_FFTW_THREADS)
      target_link_libraries(lammps PRIVATE ${FFTW}::${FFTW}_OMP)
    else()
      message(FATAL_ERROR "Need OpenMP enabled FFTW3 library for FFT_THREADS")
    endif()
  endif()
elseif(FFT STREQUAL "MKL")
  find_package(MKL REQUIRED)
  target_compile_definitions(lammps PRIVATE -DFFT_MKL)
  option(FFT_MKL_THREADS "Use threaded MKL FFT" ON)
  if(FFT_MKL_THREADS)
    target_compile_definitions(lammps PRIVATE -DFFT_MKL_THREADS)
  endif()
  target_link_libraries(lammps PRIVATE MKL::MKL)
else()
  # last option is KISSFFT
  target_compile_definitions(lammps PRIVATE -DFFT_KISS)
endif()

option(FFT_HEFFTE  "Use heFFTe as the distributed FFT engine."  OFF)
if(FFT_HEFFTE)
  # if FFT_HEFFTE is enabled, switch the builtin FFT engine with Heffte
  if(FFT STREQUAL "FFTW3") # respect the backend choice, FFTW or MKL
    set(HEFFTE_COMPONENTS "FFTW")
  elseif(FFT STREQUAL "MKL")
    set(HEFFTE_COMPONENTS "MKL")
  else()
    message(FATAL_ERROR "Using -DFFT_HEFFTE=ON, requires FFT either FFTW or MKL")
  endif()

  find_package(Heffte 2.1.0 REQUIRED ${HEFFTE_COMPONENTS})
  target_compile_definitions(lammps PRIVATE -DHEFFTE)
  target_link_libraries(lammps PRIVATE Heffte::Heffte)
endif()

set(FFT_PACK "array" CACHE STRING "Optimization for FFT")
set(FFT_PACK_VALUES array pointer memcpy)
set_property(CACHE FFT_PACK PROPERTY STRINGS ${FFT_PACK_VALUES})
validate_option(FFT_PACK FFT_PACK_VALUES)
if(NOT FFT_PACK STREQUAL "array")
  string(TOUPPER ${FFT_PACK} FFT_PACK)
  target_compile_definitions(lammps PRIVATE -DFFT_PACK_${FFT_PACK})
endif()
