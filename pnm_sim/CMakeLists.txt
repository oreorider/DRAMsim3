cmake_minimum_required(VERSION 3.0.0)
project(pnm_sim)

set(default_build_type "Release")
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "Setting build type to '${default_build_type}' as none was specified.")
  set(CMAKE_BUILD_TYPE "${default_build_type}" CACHE
      STRING "Choose the type of build." FORCE)
  # Set the possible values of build type for cmake-gui
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
    "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

add_library(inih INTERFACE)
target_include_directories(inih INTERFACE ext/headers)

add_library(format INTERFACE)
target_include_directories(format INTERFACE ext/fmt/include)
target_compile_definitions(format INTERFACE FMT_HEADER_ONLY=1)

# argparsing library, only used in main program not the library
add_library(args INTERFACE)
target_include_directories(args INTERFACE ext/headers)

add_library(json INTERFACE)
target_include_directories(json INTERFACE ext/headers)

# Main DRAMSim Lib
add_library(pnm_sim SHARED
    src/bankstate.cc
    src/channel_state.cc
    src/command_queue.cc
    src/common.cc
    src/configuration.cc
    src/controller.cc
    src/pnm.cc
    src/address_decoder.cc
    src/dram_system.cc
    src/hmc.cc
    src/refresh.cc
    src/simple_stats.cc
    src/timing.cc
    src/memory_system.cc
)

if (THERMAL)
    # dependency check
    # sudo apt-get install libatlas-base-dev on ubuntu
    find_package(BLAS REQUIRED)
    find_package(OpenMP REQUIRED)
    # YOU need to build superlu on your own. Do the following:
    # git submodule update --init
    # cd ext/SuperLU_MT_3.1 && make lib
    find_library(SUPERLU
        NAME superlu_mt_OPENMP libsuperlu_mt_OPENMP
        HINTS ${PROJECT_SOURCE_DIR}/ext/SuperLU_MT_3.1/lib/
    )

    target_link_libraries(pnm_sim
        PRIVATE ${SUPERLU} f77blas atlas m ${OpenMP_C_FLAGS}
    )
    target_sources(pnm_sim
        PRIVATE src/thermal.cc src/sp_ienv.c src/thermal_solver.c
    )
    target_compile_options(pnm_sim PRIVATE -DTHERMAL -D_LONGINT -DAdd_ ${OpenMP_C_FLAGS})

    add_executable(thermalreplay src/thermal_replay.cc)
    target_link_libraries(thermalreplay pnm_sim inih)
    target_compile_options(thermalreplay PRIVATE -DTHERMAL -D_LONGINT -DAdd_ ${OpenMP_C_FLAGS})
endif (THERMAL)

if (CMD_TRACE)
    target_compile_options(pnm_sim PRIVATE -DCMD_TRACE)
endif (CMD_TRACE)

if (ADDR_TRACE)
    target_compile_options(pnm_sim PRIVATE -DADDR_TRACE)
endif (ADDR_TRACE)

if (DEBUG_PRINT)
    target_compile_options(pnm_sim PRIVATE -DDEBUG_PRINT)
endif(DEBUG_PRINT)


target_include_directories(pnm_sim INTERFACE src)
target_compile_options(pnm_sim PRIVATE -Wall)
target_link_libraries(pnm_sim PRIVATE inih format)
set_target_properties(pnm_sim PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

# trace CXL, .etc
add_executable(pnmsim src/main.cc src/cxl.cc)
target_link_libraries(pnmsim PRIVATE pnm_sim args)
target_compile_options(pnmsim PRIVATE)
set_target_properties(pnmsim PROPERTIES
    CXX_STANDARD 11
IRED YES
    CXX_EXTENSIONS NO
)

# Unit testing
add_library(Catch INTERFACE)
target_include_directories(Catch INTERFACE ext/headers)

add_executable(pnm_simtest EXCLUDE_FROM_ALL
    tests/test_config.cc
    tests/test_dramsys.cc
    tests/test_hmcsys.cc # IDK somehow this can literally crush your computer
)
target_link_libraries(pnm_simtest Catch pnm_sim)
target_include_directories(pnm_simtest PRIVATE src/)

# We have to use this custome command because there's a bug in cmake
# that if you do `make test` it doesn't build your updated test files
# so we're stucking with `make pnm_simtest` for now
add_custom_command(
    TARGET pnm_simtest POST_BUILD
    COMMAND pnm_simtest
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    DEPENDS pnm_simtest pnm_sim
)
