/*
 * pico_platform.h — Platform initialization for Pico boards.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#ifndef INCLUDE_ROBOHUB_PLATFORM_PICO_PLATFORM_H
#define INCLUDE_ROBOHUB_PLATFORM_PICO_PLATFORM_H

#ifdef __cplusplus
extern "C" {
#endif

#pragma once

#include "error.h"

RoboHubError pico_platform_init(void);
void pico_system_clock_init(void);

#ifdef __cplusplus
}
#endif

#endif /* INCLUDE_ROBOHUB_PLATFORM_PICO_PLATFORM_H */
