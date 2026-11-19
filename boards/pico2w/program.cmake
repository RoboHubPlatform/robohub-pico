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

# The production preset, only when the project asked for it (CONFIG_RH_PRODUCTION
# in its configuration; off by default). The image is signed for RP2350 secure
# boot with the key RH_SECURE_BOOT_SIGNING_KEY names (secp256k1, PEM), and the
# OTP rows that enable secure boot for that key are written beside the build as
# `<program>.otp.json`.
#
# Building burns nothing. Loading that OTP file into the chip is the
# irreversible step, and only `rho flash --production` does it, after printing
# it and having the board's name typed (production/rp2350.json, docs/doc/index.md).
#
# The resolved configuration is `<build>/config-check/.config`, two levels above
# the generated project; a value passed with -DRHO_CONFIG_RH_PRODUCTION wins.
if(NOT DEFINED RHO_CONFIG_RH_PRODUCTION)
    set(RHO_CONFIG_RH_PRODUCTION OFF)
    get_filename_component(ROBOHUB_PICO_RESOLVED_CONFIG "${CMAKE_SOURCE_DIR}/../../config-check/.config" ABSOLUTE)
    if(EXISTS "${ROBOHUB_PICO_RESOLVED_CONFIG}")
        file(STRINGS "${ROBOHUB_PICO_RESOLVED_CONFIG}" ROBOHUB_PICO_PRODUCTION_LINE REGEX "^CONFIG_RH_PRODUCTION=y$")
        if(ROBOHUB_PICO_PRODUCTION_LINE)
            set(RHO_CONFIG_RH_PRODUCTION ON)
        endif()
    endif()
endif()
if(RHO_CONFIG_RH_PRODUCTION)
    if(NOT DEFINED ENV{RH_SECURE_BOOT_SIGNING_KEY} OR NOT EXISTS "$ENV{RH_SECURE_BOOT_SIGNING_KEY}")
        message(FATAL_ERROR
            "[robohub-pico] CONFIG_RH_PRODUCTION is set, so the image is signed for RP2350 secure boot: "
            "set RH_SECURE_BOOT_SIGNING_KEY to the secp256k1 PEM signing key "
            "(openssl ecparam -name secp256k1 -genkey -out <file>). Keep that key: once its hash "
            "is in OTP the board boots nothing it did not sign.")
    endif()
    if(NOT COMMAND pico_sign_binary OR NOT COMMAND picotool_postprocess_binary)
        message(FATAL_ERROR "[robohub-pico] this Pico SDK cannot sign images (pico_sign_binary); a production build needs SDK 2.0 or later and picotool")
    endif()
    get_filename_component(ROBOHUB_PICO_SIGNING_KEY "$ENV{RH_SECURE_BOOT_SIGNING_KEY}" ABSOLUTE)
    set(ROBOHUB_PICO_OTP_FILE "${CMAKE_SOURCE_DIR}/../../output/${RHO_PROGRAM}.otp.json")
    get_filename_component(ROBOHUB_PICO_OTP_FILE "${ROBOHUB_PICO_OTP_FILE}" ABSOLUTE)
    pico_set_otp_key_output_file(${RHO_PROGRAM} "${ROBOHUB_PICO_OTP_FILE}")
    pico_sign_binary(${RHO_PROGRAM} "${ROBOHUB_PICO_SIGNING_KEY}")
    picotool_postprocess_binary(${RHO_PROGRAM})
    message(WARNING
        "[robohub-pico] PRODUCTION build: the image is signed for secure boot and the OTP rows "
        "that enable it are written to ${ROBOHUB_PICO_OTP_FILE}. Loading them burns OTP "
        "irreversibly; flash with `rho flash --production`, which lists every step first.")
endif()
