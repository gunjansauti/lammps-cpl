set(GPU_SOURCES_DIR ${LAMMPS_SOURCE_DIR}/GPU)
set(GPU_SOURCES ${GPU_SOURCES_DIR}/gpu_extra.h
                ${GPU_SOURCES_DIR}/fix_gpu.h
                ${GPU_SOURCES_DIR}/fix_gpu.cpp
                ${GPU_SOURCES_DIR}/fix_nh_gpu.h
                ${GPU_SOURCES_DIR}/fix_nh_gpu.cpp)
target_compile_definitions(lammps PRIVATE -DLMP_GPU)

set(GPU_API "opencl" CACHE STRING "API used by GPU package")
set(GPU_API_VALUES opencl cuda hip)
set_property(CACHE GPU_API PROPERTY STRINGS ${GPU_API_VALUES})
validate_option(GPU_API GPU_API_VALUES)
string(TOUPPER ${GPU_API} GPU_API)

set(GPU_PREC "mixed" CACHE STRING "LAMMPS GPU precision")
set(GPU_PREC_VALUES double mixed single)
set_property(CACHE GPU_PREC PROPERTY STRINGS ${GPU_PREC_VALUES})
validate_option(GPU_PREC GPU_PREC_VALUES)
string(TOUPPER ${GPU_PREC} GPU_PREC)

if(GPU_PREC STREQUAL "DOUBLE")
  set(GPU_PREC_SETTING "DOUBLE_DOUBLE")
elseif(GPU_PREC STREQUAL "MIXED")
  set(GPU_PREC_SETTING "SINGLE_DOUBLE")
elseif(GPU_PREC STREQUAL "SINGLE")
  set(GPU_PREC_SETTING "SINGLE_SINGLE")
endif()

option(GPU_DEBUG "Enable debugging code of the GPU package" OFF)
mark_as_advanced(GPU_DEBUG)

if(PKG_AMOEBA AND FFT_SINGLE)
  message(FATAL_ERROR "GPU acceleration of AMOEBA is not (yet) compatible with single precision FFT")
endif()

if (PKG_AMOEBA)
  list(APPEND GPU_SOURCES
              ${GPU_SOURCES_DIR}/amoeba_convolution_gpu.h
              ${GPU_SOURCES_DIR}/amoeba_convolution_gpu.cpp)
endif()

file(GLOB GPU_LIB_SOURCES CONFIGURE_DEPENDS ${LAMMPS_LIB_SOURCE_DIR}/gpu/[^.]*.cpp)
file(MAKE_DIRECTORY ${LAMMPS_LIB_BINARY_DIR}/gpu)

if(GPU_API STREQUAL "CUDA")
  if(NOT CMAKE_CUDA_ARCHITECTURES)
    option(CUDA_BUILD_MULTIARCH "Enable building CUDA kernels for all supported GPU architectures" OFF)
    mark_as_advanced(GPU_BUILD_MULTIARCH)

    set(GPU_ARCH "sm_50" CACHE STRING "LAMMPS GPU CUDA SM primary architecture (e.g. sm_60)")
    if(CUDA_BUILD_MULTIARCH)
      set(CMAKE_CUDA_ARCHITECTURES "all" CACHE STRING "")
    else()
      string(SUBSTRING "${GPU_ARCH}" 3 -1 CUDA_ARCHITECTURES)
      set(CMAKE_CUDA_ARCHITECTURES "${CUDA_ARCHITECTURES}" CACHE STRING "")
    endif()
  endif()

  find_package(CUDAToolkit REQUIRED)

  if(NOT CMAKE_CUDA_COMPILER)
      set(CMAKE_CUDA_COMPILER "${CUDAToolkit_NVCC_EXECUTABLE}" CACHE STRING "" FORCE)
  endif()

  enable_language(CUDA)

  find_program(BIN2C bin2c PATHS ${CUDAToolkit_BIN_DIR})
  if(NOT BIN2C)
    message(FATAL_ERROR "Could not find bin2c, use -DBIN2C=/path/to/bin2c to help cmake finding it.")
  endif()
  option(CUDPP_OPT "Enable GPU binning via CUDAPP (should be off for modern GPUs)" OFF)
  option(CUDA_MPS_SUPPORT "Enable tweaks to support CUDA Multi-process service (MPS)" OFF)
  if(CUDA_MPS_SUPPORT)
    if(CUDPP_OPT)
      message(FATAL_ERROR "Must use -DCUDPP_OPT=OFF with -DCUDA_MPS_SUPPORT=ON")
    endif()
    set(GPU_CUDA_MPS_FLAGS "-DCUDA_MPS_SUPPORT")
  endif()

  # ensure that no *cubin.h files exist from a compile in the lib/gpu folder
  file(GLOB GPU_LIB_OLD_CUBIN_HEADERS CONFIGURE_DEPENDS ${LAMMPS_LIB_SOURCE_DIR}/gpu/*_cubin.h)
  if(GPU_LIB_OLD_CUBIN_HEADERS)
    message(FATAL_ERROR "########################################################################\n"
      "Found file(s) generated by the make-based build system in lib/gpu\n"
      "Please run\n"
      "  make -C ${LAMMPS_LIB_SOURCE_DIR}/gpu -f Makefile.serial clean\n"
      "to remove\n"
      "########################################################################")
  endif()

  file(GLOB GPU_LIB_CU CONFIGURE_DEPENDS ${LAMMPS_LIB_SOURCE_DIR}/gpu/[^.]*.cu ${CMAKE_CURRENT_SOURCE_DIR}/gpu/[^.]*.cu)
  list(REMOVE_ITEM GPU_LIB_CU ${LAMMPS_LIB_SOURCE_DIR}/gpu/lal_pppm.cu)

  if(CUDPP_OPT)
    file(GLOB GPU_LIB_CUDPP_SOURCES CONFIGURE_DEPENDS ${LAMMPS_LIB_SOURCE_DIR}/gpu/cudpp_mini/[^.]*.cpp)
    file(GLOB GPU_LIB_CUDPP_CU CONFIGURE_DEPENDS ${LAMMPS_LIB_SOURCE_DIR}/gpu/cudpp_mini/[^.]*.cu)
  endif()

  add_library(gpu_kernels OBJECT ${GPU_LIB_CU})
  target_compile_options(gpu_kernels PRIVATE ${CUDA_REQUEST_PIC}  -O3 --use_fast_math -Wno-deprecated-gpu-targets -allow-unsupported-compiler)
  target_compile_definitions(gpu_kernels PRIVATE -DUNIX -DNV_KERNEL -DUCL_CUDADR -D_${GPU_PREC_SETTING} -DLAMMPS_${LAMMPS_SIZES})
  target_include_directories(gpu_kernels PRIVATE ${LAMMPS_LIB_SOURCE_DIR}/gpu ${LAMMPS_LIB_BINARY_DIR}/gpu)
  if(CUDPP_OPT)
    target_include_directories(gpu_kernels PRIVATE ${LAMMPS_LIB_SOURCE_DIR}/gpu/cudpp_mini)
  endif()
  set_property(TARGET gpu_kernels PROPERTY CUDA_FATBIN_COMPILATION ON)

  set_source_files_properties(${GPU_LIB_CUDPP_CU} PROPERTIES LANGUAGE CUDA)

  foreach(CU_FILE ${GPU_LIB_CU})
    get_filename_component(CU_NAME ${CU_FILE} NAME_WE)
    string(REGEX REPLACE "^.*lal_" "" CU_NAME "${CU_NAME}")
    list(APPEND CU_CUBINS ${LAMMPS_LIB_BINARY_DIR}/gpu/${CU_NAME}_cubin.h)
  endforeach()

  add_custom_command(OUTPUT ${CU_CUBINS}
	  COMMAND ${CMAKE_COMMAND}
      -DBIN2C=${BIN2C}
      -DOBJS="$<LIST:JOIN,$<TARGET_OBJECTS:gpu_kernels>,$<SEMICOLON>>"
      -DOUTPUT_DIR=${LAMMPS_LIB_BINARY_DIR}/gpu
      -P ${CMAKE_CURRENT_LIST_DIR}/convert_to_cubin_h.cmake
		DEPENDS gpu_kernels
	)

  set_directory_properties(PROPERTIES ADDITIONAL_MAKE_CLEAN_FILES "${LAMMPS_LIB_BINARY_DIR}/gpu/*_cubin.h")

  add_library(gpu STATIC ${GPU_LIB_SOURCES} ${GPU_LIB_CUDPP_SOURCES} ${GPU_OBJS} ${GPU_LIB_CUDPP_CU} ${CU_CUBINS})
  target_link_libraries(gpu PRIVATE CUDA::cudart CUDA::cuda_driver)
  target_include_directories(gpu PRIVATE ${LAMMPS_LIB_SOURCE_DIR}/gpu ${LAMMPS_LIB_BINARY_DIR}/gpu)
  target_compile_definitions(gpu PRIVATE -DUSE_CUDA -D_${GPU_PREC_SETTING} ${GPU_CUDA_MPS_FLAGS} -DUNIX -DNV_KERNEL -DUCL_CUDADR -DLAMMPS_${LAMMPS_SIZES})
  if(GPU_DEBUG)
    target_compile_definitions(gpu PRIVATE -DUCL_DEBUG -DGERYON_KERNEL_DUMP)
  else()
    target_compile_definitions(gpu PRIVATE -DMPI_GERYON -DUCL_NO_EXIT)
  endif()
  if(CUDPP_OPT)
    target_include_directories(gpu PRIVATE ${LAMMPS_LIB_SOURCE_DIR}/gpu/cudpp_mini)
    target_compile_definitions(gpu PRIVATE -DUSE_CUDPP)
  endif()


  add_executable(nvc_get_devices ${LAMMPS_LIB_SOURCE_DIR}/gpu/geryon/ucl_get_devices.cpp)
  target_compile_definitions(nvc_get_devices PRIVATE -DUCL_CUDADR)
  target_link_libraries(nvc_get_devices PRIVATE CUDA::cudart CUDA::cuda_driver)

elseif(GPU_API STREQUAL "OPENCL")
  # the static OpenCL loader doesn't seem to work on macOS. use the system provided
  # version by default instead (for as long as it will be available)
  if("${CMAKE_SYSTEM_NAME}" STREQUAL "Darwin")
    set(_opencl_static_default OFF)
  else()
    set(_opencl_static_default ON)
  endif()
  option(USE_STATIC_OPENCL_LOADER "Download and include a static OpenCL ICD loader" ${_opencl_static_default})
  mark_as_advanced(USE_STATIC_OPENCL_LOADER)
  if(USE_STATIC_OPENCL_LOADER)
    include(OpenCLLoader)
  else()
    find_package(OpenCL REQUIRED)
  endif()

  include(OpenCLUtils)
  set(OCL_COMMON_HEADERS ${LAMMPS_LIB_SOURCE_DIR}/gpu/lal_preprocessor.h ${LAMMPS_LIB_SOURCE_DIR}/gpu/lal_aux_fun1.h)

  file(GLOB GPU_LIB_CU CONFIGURE_DEPENDS ${LAMMPS_LIB_SOURCE_DIR}/gpu/[^.]*.cu)
  list(REMOVE_ITEM GPU_LIB_CU
    ${LAMMPS_LIB_SOURCE_DIR}/gpu/lal_gayberne.cu
    ${LAMMPS_LIB_SOURCE_DIR}/gpu/lal_gayberne_lj.cu
    ${LAMMPS_LIB_SOURCE_DIR}/gpu/lal_re_squared.cu
    ${LAMMPS_LIB_SOURCE_DIR}/gpu/lal_re_squared_lj.cu
    ${LAMMPS_LIB_SOURCE_DIR}/gpu/lal_tersoff.cu
    ${LAMMPS_LIB_SOURCE_DIR}/gpu/lal_tersoff_zbl.cu
    ${LAMMPS_LIB_SOURCE_DIR}/gpu/lal_tersoff_mod.cu
    ${LAMMPS_LIB_SOURCE_DIR}/gpu/lal_hippo.cu
  )

  foreach(GPU_KERNEL ${GPU_LIB_CU})
      get_filename_component(basename ${GPU_KERNEL} NAME_WE)
      string(SUBSTRING ${basename} 4 -1 KERNEL_NAME)
      GenerateOpenCLHeader(${KERNEL_NAME} ${CMAKE_CURRENT_BINARY_DIR}/gpu/${KERNEL_NAME}_cl.h ${OCL_COMMON_HEADERS} ${GPU_KERNEL})
      list(APPEND GPU_LIB_SOURCES ${CMAKE_CURRENT_BINARY_DIR}/gpu/${KERNEL_NAME}_cl.h)
  endforeach()

  GenerateOpenCLHeader(gayberne ${CMAKE_CURRENT_BINARY_DIR}/gpu/gayberne_cl.h ${OCL_COMMON_HEADERS} ${LAMMPS_LIB_SOURCE_DIR}/gpu/lal_ellipsoid_extra.h ${LAMMPS_LIB_SOURCE_DIR}/gpu/lal_gayberne.cu)
  GenerateOpenCLHeader(gayberne_lj ${CMAKE_CURRENT_BINARY_DIR}/gpu/gayberne_lj_cl.h ${OCL_COMMON_HEADERS} ${LAMMPS_LIB_SOURCE_DIR}/gpu/lal_ellipsoid_extra.h ${LAMMPS_LIB_SOURCE_DIR}/gpu/lal_gayberne_lj.cu)
  GenerateOpenCLHeader(re_squared ${CMAKE_CURRENT_BINARY_DIR}/gpu/re_squared_cl.h ${OCL_COMMON_HEADERS} ${LAMMPS_LIB_SOURCE_DIR}/gpu/lal_ellipsoid_extra.h ${LAMMPS_LIB_SOURCE_DIR}/gpu/lal_re_squared.cu)
  GenerateOpenCLHeader(re_squared_lj ${CMAKE_CURRENT_BINARY_DIR}/gpu/re_squared_lj_cl.h ${OCL_COMMON_HEADERS} ${LAMMPS_LIB_SOURCE_DIR}/gpu/lal_ellipsoid_extra.h ${LAMMPS_LIB_SOURCE_DIR}/gpu/lal_re_squared_lj.cu)
  GenerateOpenCLHeader(tersoff ${CMAKE_CURRENT_BINARY_DIR}/gpu/tersoff_cl.h ${OCL_COMMON_HEADERS} ${LAMMPS_LIB_SOURCE_DIR}/gpu/lal_tersoff_extra.h ${LAMMPS_LIB_SOURCE_DIR}/gpu/lal_tersoff.cu)
  GenerateOpenCLHeader(tersoff_zbl ${CMAKE_CURRENT_BINARY_DIR}/gpu/tersoff_zbl_cl.h ${OCL_COMMON_HEADERS} ${LAMMPS_LIB_SOURCE_DIR}/gpu/lal_tersoff_zbl_extra.h ${LAMMPS_LIB_SOURCE_DIR}/gpu/lal_tersoff_zbl.cu)
  GenerateOpenCLHeader(tersoff_mod ${CMAKE_CURRENT_BINARY_DIR}/gpu/tersoff_mod_cl.h ${OCL_COMMON_HEADERS} ${LAMMPS_LIB_SOURCE_DIR}/gpu/lal_tersoff_mod_extra.h ${LAMMPS_LIB_SOURCE_DIR}/gpu/lal_tersoff_mod.cu)
  GenerateOpenCLHeader(hippo ${CMAKE_CURRENT_BINARY_DIR}/gpu/hippo_cl.h ${OCL_COMMON_HEADERS} ${LAMMPS_LIB_SOURCE_DIR}/gpu/lal_hippo_extra.h ${LAMMPS_LIB_SOURCE_DIR}/gpu/lal_hippo.cu)

  list(APPEND GPU_LIB_SOURCES
    ${CMAKE_CURRENT_BINARY_DIR}/gpu/gayberne_cl.h
    ${CMAKE_CURRENT_BINARY_DIR}/gpu/gayberne_lj_cl.h
    ${CMAKE_CURRENT_BINARY_DIR}/gpu/re_squared_cl.h
    ${CMAKE_CURRENT_BINARY_DIR}/gpu/re_squared_lj_cl.h
    ${CMAKE_CURRENT_BINARY_DIR}/gpu/tersoff_cl.h
    ${CMAKE_CURRENT_BINARY_DIR}/gpu/tersoff_zbl_cl.h
    ${CMAKE_CURRENT_BINARY_DIR}/gpu/tersoff_mod_cl.h
    ${CMAKE_CURRENT_BINARY_DIR}/gpu/hippo_cl.h
  )

  add_library(gpu STATIC ${GPU_LIB_SOURCES})
  target_link_libraries(gpu PRIVATE OpenCL::OpenCL)
  target_include_directories(gpu PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/gpu)
  target_compile_definitions(gpu PRIVATE -DUSE_OPENCL -D_${GPU_PREC_SETTING})
  if(GPU_DEBUG)
    target_compile_definitions(gpu PRIVATE -DUCL_DEBUG -DGERYON_KERNEL_DUMP)
  else()
    target_compile_definitions(gpu PRIVATE -DMPI_GERYON -DGERYON_NUMA_FISSION -DUCL_NO_EXIT)
  endif()

  add_executable(ocl_get_devices ${LAMMPS_LIB_SOURCE_DIR}/gpu/geryon/ucl_get_devices.cpp)
  target_compile_definitions(ocl_get_devices PRIVATE -DUCL_OPENCL)
  target_link_libraries(ocl_get_devices PRIVATE OpenCL::OpenCL)
  add_dependencies(ocl_get_devices OpenCL::OpenCL)

elseif(GPU_API STREQUAL "HIP")
  include(DetectHIPInstallation)
  find_package(hip REQUIRED)
  option(HIP_USE_DEVICE_SORT "Use GPU sorting" ON)

  if(NOT DEFINED HIP_PLATFORM)
      if(NOT DEFINED ENV{HIP_PLATFORM})
          set(HIP_PLATFORM "amd" CACHE PATH "HIP Platform to be used during compilation")
      else()
          set(HIP_PLATFORM $ENV{HIP_PLATFORM} CACHE PATH "HIP Platform used during compilation")
      endif()
  endif()

  set(ENV{HIP_PLATFORM} ${HIP_PLATFORM})

  if(HIP_PLATFORM STREQUAL "amd")
    set(HIP_ARCH "gfx906" CACHE STRING "HIP target architecture")
  elseif(HIP_PLATFORM STREQUAL "spirv")
    set(HIP_ARCH "spirv" CACHE STRING "HIP target architecture")
  elseif(HIP_PLATFORM STREQUAL "nvcc")
    find_package(CUDA REQUIRED)
    set(HIP_ARCH "sm_50" CACHE STRING "HIP primary CUDA architecture (e.g. sm_60)")

    if(CUDA_VERSION VERSION_LESS 8.0)
      message(FATAL_ERROR "CUDA Toolkit version 8.0 or later is required")
    elseif(CUDA_VERSION VERSION_GREATER_EQUAL "12.0")
      message(WARNING "Untested CUDA Toolkit version ${CUDA_VERSION}. Use at your own risk")
      set(HIP_CUDA_GENCODE "-arch=all")
    else()
      # build arch/gencode commands for nvcc based on CUDA toolkit version and use choice
      # --arch translates directly instead of JIT, so this should be for the preferred or most common architecture
      # comparison chart according to: https://en.wikipedia.org/wiki/CUDA#GPUs_supported
      set(HIP_CUDA_GENCODE "-arch=${HIP_ARCH}")
      # Kepler (GPU Arch 3.0) is supported by CUDA 5 to CUDA 10.2
      if((CUDA_VERSION VERSION_GREATER_EQUAL "5.0") AND (CUDA_VERSION VERSION_LESS "11.0"))
        string(APPEND HIP_CUDA_GENCODE " -gencode arch=compute_30,code=[sm_30,compute_30]")
      endif()
      # Kepler (GPU Arch 3.5) is supported by CUDA 5 to CUDA 11.0
      if((CUDA_VERSION VERSION_GREATER_EQUAL "5.0") AND (CUDA_VERSION VERSION_LESS "12.0"))
        string(APPEND HIP_CUDA_GENCODE " -gencode arch=compute_35,code=[sm_35,compute_35]")
      endif()
      # Maxwell (GPU Arch 5.x) is supported by CUDA 6 and later
      if(CUDA_VERSION VERSION_GREATER_EQUAL "6.0")
        string(APPEND HIP_CUDA_GENCODE " -gencode arch=compute_50,code=[sm_50,compute_50] -gencode arch=compute_52,code=[sm_52,compute_52]")
      endif()
      # Pascal (GPU Arch 6.x) is supported by CUDA 8 and later
      if(CUDA_VERSION VERSION_GREATER_EQUAL "8.0")
        string(APPEND HIP_CUDA_GENCODE " -gencode arch=compute_60,code=[sm_60,compute_60] -gencode arch=compute_61,code=[sm_61,compute_61]")
      endif()
      # Volta (GPU Arch 7.0) is supported by CUDA 9 and later
      if(CUDA_VERSION VERSION_GREATER_EQUAL "9.0")
        string(APPEND HIP_CUDA_GENCODE " -gencode arch=compute_70,code=[sm_70,compute_70]")
      endif()
      # Turing (GPU Arch 7.5) is supported by CUDA 10 and later
      if(CUDA_VERSION VERSION_GREATER_EQUAL "10.0")
        string(APPEND HIP_CUDA_GENCODE " -gencode arch=compute_75,code=[sm_75,compute_75]")
      endif()
      # Ampere (GPU Arch 8.0) is supported by CUDA 11 and later
      if(CUDA_VERSION VERSION_GREATER_EQUAL "11.0")
        string(APPEND HIP_CUDA_GENCODE " -gencode arch=compute_80,code=[sm_80,compute_80]")
      endif()
      # Ampere (GPU Arch 8.6) is supported by CUDA 11.1 and later
      if(CUDA_VERSION VERSION_GREATER_EQUAL "11.1")
        string(APPEND HIP_CUDA_GENCODE " -gencode arch=compute_86,code=[sm_86,compute_86]")
      endif()
      # Lovelace (GPU Arch 8.9) is supported by CUDA 11.8 and later
      if(CUDA_VERSION VERSION_GREATER_EQUAL "11.8")
        string(APPEND HIP_CUDA_GENCODE " -gencode arch=compute_90,code=[sm_90,compute_90]")
      endif()
      # Hopper (GPU Arch 9.0) is supported by CUDA 12.0 and later
      if(CUDA_VERSION VERSION_GREATER_EQUAL "12.0")
        string(APPEND HIP_CUDA_GENCODE " -gencode arch=compute_90,code=[sm_90,compute_90]")
      endif()
    endif()
  endif()

  file(GLOB GPU_LIB_CU CONFIGURE_DEPENDS ${LAMMPS_LIB_SOURCE_DIR}/gpu/[^.]*.cu ${CMAKE_CURRENT_SOURCE_DIR}/gpu/[^.]*.cu)
  list(REMOVE_ITEM GPU_LIB_CU ${LAMMPS_LIB_SOURCE_DIR}/gpu/lal_pppm.cu)

  set(GPU_LIB_CU_HIP "")
  foreach(CU_FILE ${GPU_LIB_CU})
    get_filename_component(CU_NAME ${CU_FILE} NAME_WE)
    string(REGEX REPLACE "^.*lal_" "" CU_NAME "${CU_NAME}")

    set(CU_CPP_FILE  "${LAMMPS_LIB_BINARY_DIR}/gpu/${CU_NAME}.cu.cpp")
    set(CUBIN_FILE   "${LAMMPS_LIB_BINARY_DIR}/gpu/${CU_NAME}.cubin")
    set(CUBIN_H_FILE "${LAMMPS_LIB_BINARY_DIR}/gpu/${CU_NAME}_cubin.h")

    if(HIP_PLATFORM STREQUAL "amd")
        configure_file(${CU_FILE} ${CU_CPP_FILE} COPYONLY)

        if(HIP_COMPILER STREQUAL "clang")
          add_custom_command(OUTPUT ${CUBIN_FILE}
            VERBATIM COMMAND ${HIP_HIPCC_EXECUTABLE} --genco --offload-arch=${HIP_ARCH} -O3 -DUSE_HIP -D_${GPU_PREC_SETTING} -DLAMMPS_${LAMMPS_SIZES} -I${LAMMPS_LIB_SOURCE_DIR}/gpu -o ${CUBIN_FILE} ${CU_CPP_FILE}
            DEPENDS ${CU_CPP_FILE}
            COMMENT "Generating ${CU_NAME}.cubin")
        else()
          add_custom_command(OUTPUT ${CUBIN_FILE}
            VERBATIM COMMAND ${HIP_HIPCC_EXECUTABLE} --genco -t="${HIP_ARCH}" -f=\"-O3 -DUSE_HIP -D_${GPU_PREC_SETTING} -DLAMMPS_${LAMMPS_SIZES} -I${LAMMPS_LIB_SOURCE_DIR}/gpu\" -o ${CUBIN_FILE} ${CU_CPP_FILE}
            DEPENDS ${CU_CPP_FILE}
            COMMENT "Generating ${CU_NAME}.cubin")
        endif()
    elseif(HIP_PLATFORM STREQUAL "nvcc")
        add_custom_command(OUTPUT ${CUBIN_FILE}
          VERBATIM COMMAND ${HIP_HIPCC_EXECUTABLE} --fatbin --use_fast_math -DUSE_HIP -D_${GPU_PREC_SETTING} -DLAMMPS_${LAMMPS_SIZES} ${HIP_CUDA_GENCODE} -I${LAMMPS_LIB_SOURCE_DIR}/gpu -o ${CUBIN_FILE} ${CU_FILE}
          DEPENDS ${CU_FILE}
          COMMENT "Generating ${CU_NAME}.cubin")
    elseif(HIP_PLATFORM STREQUAL "spirv")
      configure_file(${CU_FILE} ${CU_CPP_FILE} COPYONLY)

      add_custom_command(OUTPUT ${CUBIN_FILE}
        VERBATIM COMMAND ${HIP_HIPCC_EXECUTABLE} -c -O3 -DUSE_HIP -D_${GPU_PREC_SETTING} -DLAMMPS_${LAMMPS_SIZES} -I${LAMMPS_LIB_SOURCE_DIR}/gpu -o ${CUBIN_FILE} ${CU_CPP_FILE}
        DEPENDS ${CU_CPP_FILE}
        COMMENT "Gerating ${CU_NAME}.cubin")
      endif()

    add_custom_command(OUTPUT ${CUBIN_H_FILE}
      COMMAND ${CMAKE_COMMAND} -D SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR} -D VARNAME=${CU_NAME} -D HEADER_FILE=${CUBIN_H_FILE} -D SOURCE_FILE=${CUBIN_FILE} -P ${CMAKE_CURRENT_SOURCE_DIR}/Modules/GenerateBinaryHeader.cmake
      DEPENDS ${CUBIN_FILE}
      COMMENT "Generating ${CU_NAME}_cubin.h")

    list(APPEND GPU_LIB_SOURCES ${CUBIN_H_FILE})
  endforeach()

  set_directory_properties(PROPERTIES ADDITIONAL_MAKE_CLEAN_FILES "${LAMMPS_LIB_BINARY_DIR}/gpu/*_cubin.h ${LAMMPS_LIB_BINARY_DIR}/gpu/*.cu.cpp")

  add_library(gpu STATIC ${GPU_LIB_SOURCES})
  target_include_directories(gpu PRIVATE ${LAMMPS_LIB_BINARY_DIR}/gpu)
  target_compile_definitions(gpu PRIVATE -DUSE_HIP -D_${GPU_PREC_SETTING})
  if(GPU_DEBUG)
    target_compile_definitions(gpu PRIVATE -DUCL_DEBUG -DGERYON_KERNEL_DUMP)
  else()
    target_compile_definitions(gpu PRIVATE -DMPI_GERYON -DUCL_NO_EXIT)
  endif()
  target_link_libraries(gpu PRIVATE hip::host)

  if(HIP_USE_DEVICE_SORT)
    if(HIP_PLATFORM STREQUAL "amd")
      # newer version of ROCm (5.1+) require c++14 for rocprim
      set_property(TARGET gpu PROPERTY CXX_STANDARD 14)
    endif()
    # add hipCUB
    find_package(hipcub REQUIRED)
    target_link_libraries(gpu PRIVATE hip::hipcub)
    target_compile_definitions(gpu PRIVATE -DUSE_HIP_DEVICE_SORT)

    if(HIP_PLATFORM STREQUAL "nvcc")
      find_package(CUB)

      if(CUB_FOUND)
        set(DOWNLOAD_CUB_DEFAULT OFF)
      else()
        set(DOWNLOAD_CUB_DEFAULT ON)
      endif()

      option(DOWNLOAD_CUB "Download and compile the CUB library instead of using an already installed one" ${DOWNLOAD_CUB_DEFAULT})

      if(DOWNLOAD_CUB)
        message(STATUS "CUB download requested")
        # TODO: test update to current version 1.17.2
        set(CUB_URL "https://github.com/nvidia/cub/archive/1.12.0.tar.gz" CACHE STRING "URL for CUB tarball")
        set(CUB_MD5 "1cf595beacafff104700921bac8519f3" CACHE STRING "MD5 checksum of CUB tarball")
        mark_as_advanced(CUB_URL)
        mark_as_advanced(CUB_MD5)
        GetFallbackURL(CUB_URL CUB_FALLBACK)

        include(ExternalProject)

        ExternalProject_Add(CUB
          URL     ${CUB_URL} ${CUB_FALLBACK}
          URL_MD5 ${CUB_MD5}
          PREFIX "${CMAKE_CURRENT_BINARY_DIR}"
          CONFIGURE_COMMAND ""
          BUILD_COMMAND ""
          INSTALL_COMMAND ""
          UPDATE_COMMAND ""
        )
        ExternalProject_get_property(CUB SOURCE_DIR)
        set(CUB_INCLUDE_DIR ${SOURCE_DIR})
      else()
        find_package(CUB)
        if(NOT CUB_FOUND)
          message(FATAL_ERROR "CUB library not found. Help CMake to find it by setting CUB_INCLUDE_DIR, or set DOWNLOAD_CUB=ON to download it")
        endif()
      endif()

      target_include_directories(gpu PRIVATE ${CUB_INCLUDE_DIR})
    endif()
  endif()

  add_executable(hip_get_devices ${LAMMPS_LIB_SOURCE_DIR}/gpu/geryon/ucl_get_devices.cpp)
  target_compile_definitions(hip_get_devices PRIVATE -DUCL_HIP)
  target_link_libraries(hip_get_devices PRIVATE hip::host)

  if(HIP_PLATFORM STREQUAL "nvcc")
    target_compile_definitions(gpu PRIVATE -D__HIP_PLATFORM_NVCC__)
    target_include_directories(gpu PRIVATE ${CUDA_INCLUDE_DIRS})
    target_link_libraries(gpu PRIVATE ${CUDA_LIBRARIES} ${CUDA_CUDA_LIBRARY})

    target_compile_definitions(hip_get_devices PRIVATE -D__HIP_PLATFORM_NVCC__)
    target_include_directories(hip_get_devices PRIVATE ${CUDA_INCLUDE_DIRS})
    target_link_libraries(hip_get_devices PRIVATE ${CUDA_LIBRARIES} ${CUDA_CUDA_LIBRARY})
  endif()
endif()

if(BUILD_OMP)
  find_package(OpenMP COMPONENTS CXX REQUIRED)
  target_link_libraries(gpu PRIVATE OpenMP::OpenMP_CXX)
endif()
target_link_libraries(lammps PRIVATE gpu)

set_property(GLOBAL PROPERTY "GPU_SOURCES" "${GPU_SOURCES}")
# detect styles which have a GPU version
RegisterStylesExt(${GPU_SOURCES_DIR} gpu GPU_SOURCES)
RegisterFixStyle(${GPU_SOURCES_DIR}/fix_gpu.h)

get_property(GPU_SOURCES GLOBAL PROPERTY GPU_SOURCES)
if(BUILD_MPI)
  target_link_libraries(gpu PRIVATE MPI::MPI_CXX)
else()
  target_link_libraries(gpu PRIVATE mpi_stubs)
endif()

target_compile_definitions(gpu PRIVATE -DLAMMPS_${LAMMPS_SIZES})
set_target_properties(gpu PROPERTIES OUTPUT_NAME lammps_gpu${LAMMPS_MACHINE})
target_sources(lammps PRIVATE ${GPU_SOURCES})
target_include_directories(lammps PRIVATE ${GPU_SOURCES_DIR})
