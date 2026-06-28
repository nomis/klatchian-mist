/*
 * klatchian-mist - ESP32 Zigbee dehumidifier controller
 * Copyright 2026  Simon Arlott
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

#include <driver/gpio.h>
#include <sdkconfig.h>

#include <deque>
#include <mutex>
#include <vector>

#include "dehumidifier.h"
#include "thread.h"

namespace mist {

class Device;

enum class SetState {
	NONE,
	PENDING,
	SENT,
};

class SerialIO: public WakeupThread {
public:
	SerialIO(gpio_num_t tx_pin, gpio_num_t rx_pin);
	~SerialIO() = delete;

	// cppcheck-suppress duplInheritedMember
	static constexpr const char *TAG = "mist.SerialIO";

	void attach(Device &device);
	void start();

	void set_power(bool state);
	void set_mode(dehumidifier::Mode mode);
	void set_fan_speed(dehumidifier::Fan speed);
	void set_humidity_setpoint(int humidity);
	void set_ioniser(bool state);

private:
	static constexpr const unsigned long REFRESH_INTERVAL_US = 1 * 1000 * 1000;
	static constexpr const unsigned long RX_BUFFER_SIZE = 256;

	static uint8_t calc_crc8(const uint8_t *data, size_t size);
	static uint8_t calc_checksum(const uint8_t *data, size_t size);

	unsigned long run_tasks() override;
	void rx_loop();
	void try_parse_message();
	void parse_message();

	void rx_parse_state();
	void tx_message(uint8_t version, uint8_t type, const std::vector<uint8_t> &data, const char *desc);
	void tx_request_state();
	void tx_network_status();
	void prepare_set_state();
	void tx_set_state();

	void debug_message(const char *direction, const uint8_t *data, size_t size, const char *desc);

	void update_state();

	Device *device_{nullptr};

	std::deque<uint8_t> rx_buf_;
	uint64_t tx_request_state_us_{0};

	std::mutex mutex_;
	bool request_state_busy_{false};
	SetState set_state_busy_{SetState::NONE};
	struct {
		bool power;
		uint8_t mode;
		uint8_t fan_speed;
		bool timer;
		unsigned int on_timer_mins;
		unsigned int off_timer_mins;
		uint8_t humidity_setpoint;
		uint8_t humidity_reading;
		bool ioniser;
		bool auto_defrost;
		float temperature_c;
		bool bucket_full;
		bool valid;
	} state_{};
	struct {
		std::vector<uint8_t> data;
		bool set_power;
		bool power;
		bool set_mode;
		uint8_t mode;
		bool set_fan_speed;
		uint8_t fan_speed;
		bool set_humidity_setpoint;
		uint8_t humidity_setpoint;
		bool set_ioniser;
		bool ioniser;
	} set_state_{};
	bool tx_network_status_{false};
};

} // namespace mist
