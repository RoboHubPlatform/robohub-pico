# What this board says about a program, once the program target exists.
#
# The console is the SDK's stdio, and which of its transports carry it is a
# property of the *program*: the driver array `stdio_init_all` walks is
# compiled into the executable, so the driver has to be named on it rather than
# only linked through a dependency. The generated build sets `RHO_PROGRAM` to
# that target and includes this file after creating it.
#
# The platform's build file records the choice as a cache variable, because a
# variable set in a subdirectory is not visible here.
#
# @project RoboHub Platform
# @license MIT

if(RHO_CONSOLE_USB AND COMMAND pico_enable_stdio_usb)
    pico_enable_stdio_usb(${RHO_PROGRAM} 1)
    pico_enable_stdio_uart(${RHO_PROGRAM} 0)
elseif(COMMAND pico_enable_stdio_uart)
    pico_enable_stdio_uart(${RHO_PROGRAM} 1)
endif()
