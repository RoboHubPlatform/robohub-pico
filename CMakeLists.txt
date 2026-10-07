cmake_minimum_required(VERSION 3.16)
project(robohub-pico C ASM)

# ---------------------------------------------------------------------------
# Locate robohub-common
# ---------------------------------------------------------------------------
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/deps.cmake")
  include("${CMAKE_CURRENT_SOURCE_DIR}/deps.cmake")
endif()

if(NOT ROBOHUB_COMMON_DIR)
  if(EXISTS "/home/miraur/projects/robohub-common")
    set(ROBOHUB_COMMON_DIR "/home/miraur/projects/robohub-common")
  elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../robohub-common")
    get_filename_component(ROBOHUB_COMMON_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../robohub-common" ABSOLUTE)
  elseif(DEFINED ENV{ROBOHUB_COMMON_DIR})
    set(ROBOHUB_COMMON_DIR "$ENV{ROBOHUB_COMMON_DIR}")
  else()
    message(FATAL_ERROR "robohub-common not found. Set ROBOHUB_COMMON_DIR or place it beside this project.")
  endif()
endif()
message(STATUS "[robohub-pico] robohub-common at ${ROBOHUB_COMMON_DIR}")

if(PROJECT_IS_TOP_LEVEL)
  add_subdirectory("${ROBOHUB_COMMON_DIR}" robohub-common)
endif()

# ---------------------------------------------------------------------------
# The Pico SDK
# ---------------------------------------------------------------------------
# A bare-metal build of this board is the SDK's build. The board's toolchain
# prelude located the SDK before `project()`; initializing it here defines the
# targets everything else is expressed in terms of — its startup code, its
# linker script, its clock and USB bring-up, and, when it is wanted, its
# wireless stack.
#
# This is the whole reason the platform does not carry its own crt0, linker
# script or USB driver: those are the vendor's, kept in step with the silicon,
# and a second copy is a second thing to get subtly wrong.
if(NOT DEFINED PICO_SDK_PATH)
  if(DEFINED ENV{PICO_SDK_PATH})
    set(PICO_SDK_PATH "$ENV{PICO_SDK_PATH}")
  elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/vendor/pico-sdk")
    set(PICO_SDK_PATH "${CMAKE_CURRENT_SOURCE_DIR}/vendor/pico-sdk")
  endif()
endif()

set(ROBOHUB_PICO_SDK_AVAILABLE FALSE)
if(PICO_SDK_PATH AND EXISTS "${PICO_SDK_PATH}")
  message(STATUS "[robohub-pico] Pico SDK at ${PICO_SDK_PATH}")
  if(COMMAND pico_sdk_init)
    pico_sdk_init()
  elseif(EXISTS "${PICO_SDK_PATH}/pico_sdk_init.cmake")
    # Initialized here as a fallback, for a build that reached this file
    # without the board's prelude having located the SDK.
    set(PICO_SDK_PATH "${PICO_SDK_PATH}" CACHE PATH "the Pico SDK this platform vendors")
    list(APPEND CMAKE_MODULE_PATH "${PICO_SDK_PATH}/external")
    include(${PICO_SDK_PATH}/pico_sdk_init.cmake)
    pico_sdk_init()
  endif()
  set(ROBOHUB_PICO_SDK_AVAILABLE TRUE)
else()
  message(STATUS "[robohub-pico] Pico SDK not configured; host builds and tests will work, bare-metal builds will not link")
endif()

# ---------------------------------------------------------------------------
# Host vs bare-metal
# ---------------------------------------------------------------------------
set(ROBOHUB_PICO_HOSTED OFF)
if(PROJECT_IS_TOP_LEVEL AND RHO_BUILD_HOST_TESTS AND NOT RHO_TARGET_IS_BAREMETAL)
  set(ROBOHUB_PICO_HOSTED ON)
endif()

# ---------------------------------------------------------------------------
# Sources
# ---------------------------------------------------------------------------
set(ROBOHUB_PICO_SOURCES
    src/platform/pico_console.c
    src/platform/pico_fault.c
    src/platform/pico_platform.c
    src/platform/pico_tick.c
)

if(RHO_ROBO_CONFIG AND EXISTS "${RHO_ROBO_CONFIG}")
  include("${RHO_ROBO_CONFIG}")
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/cmake/robo-config.cmake")
  include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/robo-config.cmake")
endif()

macro(robohub_pico_add_guarded symbol)
  set(options "")
  set(oneValueArgs "")
  set(multiValueArgs SOURCES)
  cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
  if(RHO_CONFIG_${symbol})
    list(APPEND ROBOHUB_PICO_SOURCES ${ARG_SOURCES})
  endif()
endmacro()

robohub_pico_add_guarded(RH_HAL_GPIO SOURCES src/hal/pico_gpio.c)
robohub_pico_add_guarded(RH_HAL_I2C  SOURCES src/hal/pico_i2c.c)
robohub_pico_add_guarded(RH_HAL_SPI  SOURCES src/hal/pico_spi.c)
robohub_pico_add_guarded(RH_HAL_UART SOURCES src/hal/pico_uart.c)
robohub_pico_add_guarded(RH_HAL_TIMER SOURCES src/hal/pico_timer.c)
robohub_pico_add_guarded(RH_HAL_RS485 SOURCES src/hal/pico_rs485.c)

# ---------------------------------------------------------------------------
# Library target
# ---------------------------------------------------------------------------
add_library(robohub-pico STATIC ${ROBOHUB_PICO_SOURCES})

target_include_directories(robohub-pico PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/robohub
    ${CMAKE_CURRENT_SOURCE_DIR}/include/robohub/hal
    ${CMAKE_CURRENT_SOURCE_DIR}/include/robohub/platform
    ${CMAKE_CURRENT_SOURCE_DIR}/include/robohub/drivers
    ${ROBOHUB_COMMON_DIR}/include
    ${ROBOHUB_COMMON_DIR}/include/robohub
    ${ROBOHUB_COMMON_DIR}/include/robohub/hal
    ${ROBOHUB_COMMON_DIR}/include/robohub/drivers
)

target_link_libraries(robohub-pico PUBLIC robohub-common)

if(ROBOHUB_PICO_SDK_AVAILABLE)
  # `RH_PICO_SDK` is the name the sources branch on. A different name here
  # leaves every guarded block out — the console never initialises and the HAL
  # implementations become stubs — while the build still succeeds, which is the
  # quietest way for a define to be wrong.
  target_compile_definitions(robohub-pico PRIVATE RH_PICO_SDK=1)

  # This library's own sources include the SDK's headers, and it cannot get
  # them by linking `pico_stdlib`: that target carries the SDK's startup and
  # runtime as interface *sources*, so linking it here would compile them into
  # the library and again into the program that links the library. Its headers
  # are therefore named directly, together with the directory the SDK generates
  # its board configuration into.
  file(GLOB ROBOHUB_SDK_INCLUDE_DIRS
    "${PICO_SDK_PATH}/src/common/*/include"
    "${PICO_SDK_PATH}/src/common/*/*/include"
    "${PICO_SDK_PATH}/src/rp2_common/*/include"
    "${PICO_SDK_PATH}/src/rp2350/*/include"
    "${PICO_SDK_PATH}/src/boards/include"
  )
  target_include_directories(robohub-pico PRIVATE
    ${ROBOHUB_SDK_INCLUDE_DIRS}
    "${CMAKE_BINARY_DIR}/generated/pico_base"
  )

  # The architecture this platform's own sources are built for. The SDK sets
  # the same flags on its targets, but those reach a *program*, not the library
  # a platform compiles; and putting them in the target spec instead would leak
  # them into the SDK's host-built tooling, which is not this architecture.
  target_compile_options(robohub-pico PRIVATE
    -mcpu=cortex-m33 -mthumb -mfloat-abi=softfp -mfpu=fpv5-sp-d16
    -ffunction-sections -fdata-sections
  )
  # `pico_stdlib` is the SDK's own view of a program: the startup code, the
  # linker script and the runtime, carried as flags and sources on a target.
  # Linking it is what makes this an SDK build rather than one beside it.
  target_link_libraries(robohub-pico INTERFACE pico_stdlib)
endif()

# ---------------------------------------------------------------------------
# Console over USB
# ---------------------------------------------------------------------------
# The board's own USB port can carry the console as a CDC serial device. The
# SDK's `pico_stdio_usb` is that device — its descriptors, its CDC class and
# its TinyUSB device stack — so the platform links it rather than carrying a
# copy, and the console below is the SDK's stdio either way.
if(RHO_CONFIG_RH_CONSOLE_USB AND ROBOHUB_PICO_SDK_AVAILABLE)
  message(STATUS "[robohub-pico] console over USB (the SDK's pico_stdio_usb)")
  target_compile_definitions(robohub-pico PRIVATE RH_CONSOLE_USB=1)
  # Recorded as a cache variable so the fragment that says it to the program —
  # included from the generated build, outside this directory — can read it: a
  # variable set here is not visible there.
  set(RHO_CONSOLE_USB ON CACHE BOOL "the console is the board's USB port" FORCE)
  # pico_stdio_usb is enabled on the program by the generated build file
  # (pico_enable_stdio_usb), which is where the driver array is compiled.
endif()

target_compile_features(robohub-pico PUBLIC c_std_11)

# ---------------------------------------------------------------------------
# Host tests
# ---------------------------------------------------------------------------
if(ROBOHUB_PICO_HOSTED)
  message(STATUS "[robohub-pico] registering host unit tests")
  enable_testing()
  add_executable(robohub-pico-test-error tests/test_error.c)
  target_link_libraries(robohub-pico-test-error PRIVATE robohub-pico)
  add_test(NAME robohub-pico-test-error COMMAND robohub-pico-test-error)
endif()

# ---------------------------------------------------------------------------
# Install
# ---------------------------------------------------------------------------
install(FILES module.yml DESTINATION share/robohub-pico)
install(DIRECTORY include/ DESTINATION include)
install(TARGETS robohub-pico
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib
        RUNTIME DESTINATION bin)
