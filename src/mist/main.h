/*
 * klatchian-mist - ESP32 Zigbee dehumidifier controller
 * Copyright 2023-2024  Simon Arlott
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <cstddef>
#include <sdkconfig.h>

namespace mist {

static constexpr const char *TAG = "mist";
static constexpr const size_t MAX_LIGHTS = 1;

#ifndef CONFIG_NUTT_SWITCH_ACTIVE_LOW
#define CONFIG_NUTT_SWITCH_ACTIVE_LOW 0
#endif
static constexpr const bool SWITCH_ACTIVE_LOW = CONFIG_NUTT_SWITCH_ACTIVE_LOW;

#ifndef CONFIG_NUTT_RELAY_ACTIVE_LOW
#define CONFIG_NUTT_RELAY_ACTIVE_LOW 0
#endif
static constexpr const bool RELAY_ACTIVE_LOW = CONFIG_NUTT_RELAY_ACTIVE_LOW;

} // namespace mist
