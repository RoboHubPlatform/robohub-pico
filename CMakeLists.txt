cmake_minimum_required(VERSION 3.16)
project(robohub-pico C ASM)

# ---------------------------------------------------------------------------
# Locating a sibling module
# ---------------------------------------------------------------------------
# A path is never hard-coded. The order is: a variable the parent build already
# set — rho writes one for every dependency — then the environment, then a
# sibling checkout, which is the ordinary multi-repo layout, and finally the
# module's own repository, fetched. The same file is then true on every machine.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/deps.cmake")
  include("${CMAKE_CURRENT_SOURCE_DIR}/deps.cmake")
endif()

# robohub_locate_module(<name> <out-var>): find a sibling module and set the
# variable to its directory. `<name>` is the module's name, so the variable is
# its upper-case form with `_DIR` appended — robohub-common becomes
# ROBOHUB_COMMON_DIR, the name a parent build or the environment can carry.
function(robohub_locate_module module out_var)
  string(TOUPPER "${module}" upper)
  string(REPLACE "-" "_" upper "${upper}")
  if(${out_var})
    return()
  endif()
  if(DEFINED ENV{${upper}_DIR})
    set(${out_var} "$ENV{${upper}_DIR}" PARENT_SCOPE)
    return()
  endif()
  # A sibling checkout, searched upward: a module may sit nested — a driver
  # inside a collection — with the sibling several levels above it.
  set(dir "${CMAKE_CURRENT_SOURCE_DIR}")
  foreach(level RANGE 6)
    get_filename_component(parent "${dir}/.." ABSOLUTE)
    if(EXISTS "${parent}/${module}/CMakeLists.txt")
      set(${out_var} "${parent}/${module}" PARENT_SCOPE)
      return()
    endif()
    set(dir "${parent}")
  endforeach()
  include(FetchContent)
  message(STATUS "[${PROJECT_NAME}] fetching ${module}")
  FetchContent_Declare(${module}
    GIT_REPOSITORY "https://github.com/RoboHubPlatform/${module}.git"
    GIT_TAG main GIT_SHALLOW TRUE GIT_PROGRESS TRUE)
  FetchContent_MakeAvailable(${module})
  set(${out_var} "${${module}_SOURCE_DIR}" PARENT_SCOPE)
endfunction()

robohub_locate_module(robohub-common ROBOHUB_COMMON_DIR)
if(NOT ROBOHUB_COMMON_DIR)
  message(FATAL_ERROR "robohub-common not found and could not be fetched")
endif()
message(STATUS "[${PROJECT_NAME}] robohub-common at ${ROBOHUB_COMMON_DIR}")

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
robohub_pico_add_guarded(RH_HAL_ONEWIRE SOURCES src/hal/pico_onewire.c)
robohub_pico_add_guarded(RH_HAL_DHT SOURCES src/hal/pico_dht.c)

# ---------------------------------------------------------------------------
# FreeRTOS
# ---------------------------------------------------------------------------
# When a project enables FreeRTOS, the platform fills in the contract
# robohub-freertos reads. The kernel this platform vendors is the Raspberry Pi
# one, whose RP2350 port upstream FreeRTOS does not carry; adding it here defines
# `FreeRTOS-Kernel`, and the middleware uses that instead of fetching another.
#
# This is the platform's job and not the middleware's: which port a core needs
# is a fact about the core, and robohub-freertos is written to run on all of
# them. A platform with no vendor kernel leaves the contract empty and the
# middleware fetches an upstream one with the port the contract names.
if(RHO_CONFIG_RH_FREERTOS)
  set(ROBOHUB_FREERTOS_VENDOR_KERNEL "${CMAKE_CURRENT_SOURCE_DIR}/vendor/FreeRTOS-Kernel")
  if(ROBOHUB_PICO_SDK_AVAILABLE
     AND EXISTS "${ROBOHUB_FREERTOS_VENDOR_KERNEL}/portable/ThirdParty/GCC/RP2350_ARM_NTZ/CMakeLists.txt")
    message(STATUS "[${PROJECT_NAME}] FreeRTOS: adding the RP2350 port the vendored kernel carries")
    add_subdirectory(
      "${ROBOHUB_FREERTOS_VENDOR_KERNEL}/portable/ThirdParty/GCC/RP2350_ARM_NTZ"
      "${CMAKE_BINARY_DIR}/_robohub-freertos-port" EXCLUDE_FROM_ALL)
    set(ROBOHUB_FREERTOS_PLATFORM_KERNEL ON CACHE BOOL
        "the selected platform defines FreeRTOS-Kernel itself" FORCE)
    set(ROBOHUB_CPU_CLOCK_HZ 150000000 CACHE STRING "the RP2350 clock" FORCE)
    set(ROBOHUB_HEAP_SIZE 32768 CACHE STRING "the FreeRTOS heap, in bytes" FORCE)
  else()
    message(STATUS "[${PROJECT_NAME}] FreeRTOS enabled but no vendored kernel port; robohub-freertos will fetch one")
  endif()
endif()

# ---------------------------------------------------------------------------
# Library target
# ---------------------------------------------------------------------------
add_library(robohub-pico STATIC ${ROBOHUB_PICO_SOURCES})

# A contract macro: which platform this is, for a module that behaves
# differently on one — the FreeRTOS configuration reads it.
target_compile_definitions(robohub-pico PUBLIC ROBOHUB_PLATFORM_PICO=1)

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

# ---------------------------------------------------------------------------
# Wireless networking
# ---------------------------------------------------------------------------
# When a project enables the network, the Pico W's radio and LwIP are the SDK's.
# `pico_cyw43_arch` is linked as an interface target, the way pico_stdlib is, so
# the stack compiles once in the program and not into this library; this
# library's own net source needs only its headers and definitions. The arch is
# the threadsafe-background one, which is `NO_SYS=1`: the RAW API, not sockets.
if(RHO_CONFIG_RH_NET AND ROBOHUB_PICO_SDK_AVAILABLE)
  # A board without the wireless chip names no `pico_cyw43_arch` arch; say so
  # rather than failing later on an undefined link target.
  if(NOT TARGET pico_cyw43_arch_lwip_threadsafe_background)
    message(FATAL_ERROR
      "[robohub-pico] RH_NET is enabled but board '${PICO_BOARD}' has no cyw43 wireless chip")
  endif()
  message(STATUS "[robohub-pico] network over cyw43 + LwIP (the SDK's pico_cyw43_arch)")
  # The net source is compiled where the wireless headers are — in the program,
  # as an interface source, the same shape the SDK's own targets use. Building
  # it inside this library would mean naming every header `pico_cyw43_arch`
  # already names, and keeping that list in step by hand.
  target_sources(robohub-pico INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/src/platform/pico_net.c")
  target_link_libraries(robohub-pico INTERFACE pico_cyw43_arch_lwip_threadsafe_background)
  # LwIP is configured by a header the application supplies; this is the board's.
  target_include_directories(robohub-pico INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/config")
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
