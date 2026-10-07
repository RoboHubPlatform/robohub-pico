# What makes this board's build the Pico SDK's build.
#
# Included from the generated toolchain.cmake, before `project()`, because
# locating the SDK is something only a toolchain file can do. Everything the
# SDK brings is then reached through the targets it defines: the startup code
# and linker script that put an image on an RP2350, the clock and USB
# bring-up, and — when the wireless chip is wanted — `pico_cyw43_arch` with
# the async context its event loop runs on.
#
# The point of going through the SDK rather than beside it is that these are
# the vendor's, kept in step with the silicon by the same people who document
# it. A platform that reimplemented them would be reimplementing the parts
# that are hardest to get right and easiest to get subtly wrong.
#
# @project RoboHub Platform
# @license MIT

# The part and the board. The board is where the memory map, the pinout and
# the wireless configuration come from, so it is named rather than derived.
set(PICO_PLATFORM "rp2350" CACHE STRING "the RP2350, which is the part this board carries")
set(PICO_BOARD "pico2_w" CACHE STRING "the board, which is where its memory map comes from")

# The SDK this platform vendors, beside it rather than somewhere on the
# machine: the version a board was built and tested against is the version it
# builds with.
get_filename_component(PICO_SDK_PATH "${CMAKE_CURRENT_LIST_DIR}/../../vendor/pico-sdk" ABSOLUTE)
set(PICO_SDK_PATH "${PICO_SDK_PATH}" CACHE PATH "the Pico SDK this platform vendors")

# The SDK's startup is assembly and its runtime is C++. The generated build
# enables these before `project()` classifies the files, once the compiler
# below is known; naming them here keeps "which languages a board needs" with
# the board.
set(RHO_PROGRAM_LANGUAGES "CXX;ASM")

include(${PICO_SDK_PATH}/external/pico_sdk_import.cmake)
