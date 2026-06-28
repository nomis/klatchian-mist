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

#include "mist/comms.h"

#include <esp_err.h>
#include <esp_heap_caps.h>
#include <esp_log.h>
#include <esp_partition.h>
#include <esp_timer.h>
#include <driver/gpio.h>
#include <driver/uart.h>
#include <freertos/FreeRTOS.h>
#include <hal/uart_ll.h>
#include <led_strip.h>
#include <soc/uart_pins.h>
#include <string.h>

#include <cstring>
#include <deque>
#include <memory>
#include <thread>
#include <vector>

#include "mist/device.h"
#include "mist/log.h"

namespace mist {

SerialIO::SerialIO(gpio_num_t tx_pin, gpio_num_t rx_pin)
		: WakeupThread("SerialIO", false) {
	ESP_ERROR_CHECK(uart_set_pin(UART_NUM_1, tx_pin, rx_pin,
		UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

	uart_config_t uart_config{};
	uart_config.baud_rate = 9600;
	uart_config.data_bits = UART_DATA_8_BITS;
	uart_config.parity = UART_PARITY_DISABLE;
	uart_config.stop_bits = UART_STOP_BITS_1;
	uart_config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;

	ESP_ERROR_CHECK(uart_param_config(UART_NUM_1, &uart_config));
	ESP_ERROR_CHECK(uart_driver_install(UART_NUM_1, SOC_UART_FIFO_LEN + 1,
		0, 0, nullptr, ESP_INTR_FLAG_LEVEL1));

	uart_intr_config_t uart_int_config{};
	uart_int_config.intr_enable_mask = UART_INTR_RXFIFO_FULL;
	uart_int_config.rxfifo_full_thresh = 1;

	ESP_ERROR_CHECK(uart_intr_config(UART_NUM_1, &uart_int_config));
	ESP_ERROR_CHECK(uart_enable_rx_intr(UART_NUM_1));
}

void SerialIO::attach(Device &device) {
	device_ = &device;
}

void SerialIO::start() {
	std::thread t;

	tx_request_state_us_ = esp_timer_get_time();

	make_thread(t, "serial_io_tx", 6144, 4, &SerialIO::run_loop, this);
	t.detach();

	make_thread(t, "serial_io_rx", 6144, 3, &SerialIO::rx_loop, this);
	t.detach();
}

unsigned long SerialIO::run_tasks() {
	{
		std::unique_lock lock{mutex_};

		if (tx_network_status_) {
			tx_network_status_ = false;
			if (lock) {
				lock.unlock();
			}
			tx_network_status();
		}
	}

	if (esp_timer_get_time() - tx_request_state_us_ >= REFRESH_INTERVAL_US) {
		tx_request_state();
		tx_request_state_us_ = esp_timer_get_time();
	}

	return std::max((uint64_t)0U,
		REFRESH_INTERVAL_US - (esp_timer_get_time() - tx_request_state_us_)) / 1000ULL;
}

void SerialIO::rx_loop() {
	char buf[1];

	while (true) {
		if (uart_read_bytes(UART_NUM_1, buf, sizeof(buf), portMAX_DELAY) == 1) {
			while (rx_buf_.size() >= RX_BUFFER_SIZE)
				rx_buf_.pop_front();

			rx_buf_.push_back(buf[0]);
			try_parse_message();
		}
	}
}

void SerialIO::try_parse_message() {
	while (rx_buf_.size() >= 10) {
		if (rx_buf_[0] != 0xAA) {
			rx_buf_.pop_front();
			continue;
		}

		if (rx_buf_[1] < 11) {
			rx_buf_.erase(rx_buf_.begin(), rx_buf_.begin() + 1);
			continue;
		}

		if (rx_buf_[2] != 0xA1) {
			rx_buf_.erase(rx_buf_.begin(), rx_buf_.begin() + 2);
			continue;
		}

		if (rx_buf_[3] != 0x00) {
			rx_buf_.erase(rx_buf_.begin(), rx_buf_.begin() + 3);
			continue;
		}

		if (rx_buf_[4] != 0x00) {
			rx_buf_.erase(rx_buf_.begin(), rx_buf_.begin() + 4);
			continue;
		}

		if (rx_buf_[5] != 0x00) {
			rx_buf_.erase(rx_buf_.begin(), rx_buf_.begin() + 5);
			continue;
		}

		if (rx_buf_[6] != 0x00) {
			rx_buf_.erase(rx_buf_.begin(), rx_buf_.begin() + 6);
			continue;
		}

		if (rx_buf_[7] != 0x00) {
			rx_buf_.erase(rx_buf_.begin(), rx_buf_.begin() + 7);
			continue;
		}

		break;
	}

	if (rx_buf_.size() < rx_buf_[1] + 1) {
		return;
	}

	parse_message();

	rx_buf_.erase(rx_buf_.begin(), rx_buf_.begin() + rx_buf_[1] + 1);
}

void SerialIO::parse_message() {
	const uint8_t length = rx_buf_[1] - 10 - 1;
	const uint8_t version = rx_buf_[8];
	const uint8_t type = rx_buf_[9];
	bool crc8, checksum, valid;

	crc8 = (rx_buf_[10 + length] == calc_crc8(&rx_buf_[10], length));
	checksum = (rx_buf_[10 + length + 1] == calc_checksum(&rx_buf_[1], 10 + length));
	valid = crc8 && checksum;

	if (version != 0x03 || length < 1) {
		debug_message("<-", &rx_buf_[0], rx_buf_[1] + 1, "Unknown");
		return;
	}

	if (valid && type == 0x03 && rx_buf_[10] == 0xC8) {
		debug_message("<-", &rx_buf_[0], rx_buf_[1] + 1, "State");
		rx_parse_state();
	} else if (valid && type == 0x03 && rx_buf_[10] == 0x63) {
		debug_message("<-", &rx_buf_[0], rx_buf_[1] + 1, "Network status request");
		{
			std::lock_guard lock{mutex_};

			tx_network_status_ = true;
		}
		wake_up();
	} else if ((valid || (rx_buf_[10 + length] == 0 && checksum))
			&& type == 0x64 && rx_buf_[10] == 0x00 && length >= 6
			&& rx_buf_[11] == 0x01 && rx_buf_[15] == 0x01) {
		/* This message will be sent 3 times and the CRC8 is invalid (0x00) */
		debug_message("<-", &rx_buf_[0], rx_buf_[1] + 1, "WiFi reset");
		device_->join_network();
	}
}

void SerialIO::rx_parse_state() {
	request_state_busy_ = false;

	std::lock_guard lock{mutex_};

	state_.power = rx_buf_[11] & 0x01;
	state_.mode = rx_buf_[12] & 0x0F;
	state_.fan_speed = rx_buf_[13] & 0x7F;
	state_.timer = rx_buf_[13] & 0x80;
	state_.on_timer_mins = std::max(0U, rx_buf_[14] - 0x7FU) * 15;
	state_.off_timer_mins = std::max(0U, rx_buf_[15] - 0x7FU) * 15;
	state_.humidity_setpoint = rx_buf_[17];
	state_.ioniser = rx_buf_[19] & 0x40;
	state_.auto_defrost = rx_buf_[20] & 0x80;
	state_.humidity_reading = rx_buf_[26];
	state_.temperature_c = ((int)rx_buf_[27] - 50) / 2.0;
	state_.temperature_c += ((state_.temperature_c >= 0) ? 0.0 : -1.0) * (rx_buf_[28] & 0xF) * 0.1;
	state_.bucket_full = rx_buf_[31] == 38;

	ESP_LOGI(TAG, "State: power=%u mode=%u fan_speed=%u timer=%u on_timer_mins=%u off_timer_mins=%u humidity_setpoint=%u ioniser=%u auto_defrost=%u humidity_reading=%u temperature_c=%.1f error_code=%u",
		state_.power, state_.mode, state_.fan_speed, state_.timer,
		state_.on_timer_mins, state_.off_timer_mins, state_.humidity_setpoint,
		state_.ioniser, state_.auto_defrost, state_.humidity_reading,
		state_.temperature_c, state_.bucket_full);
}

uint8_t SerialIO::calc_crc8(const uint8_t *data, size_t size) {
	/* CRC-8-Dallas/Maxim (0x31) */
	static const uint8_t crc_table[] = {
		0x00, 0x5E, 0xBC, 0xE2, 0x61, 0x3F, 0xDD, 0x83,
		0xC2, 0x9C, 0x7E, 0x20, 0xA3, 0xFD, 0x1F, 0x41,
		0x9D, 0xC3, 0x21, 0x7F, 0xFC, 0xA2, 0x40, 0x1E,
		0x5F, 0x01, 0xE3, 0xBD, 0x3E, 0x60, 0x82, 0xDC,
		0x23, 0x7D, 0x9F, 0xC1, 0x42, 0x1C, 0xFE, 0xA0,
		0xE1, 0xBF, 0x5D, 0x03, 0x80, 0xDE, 0x3C, 0x62,
		0xBE, 0xE0, 0x02, 0x5C, 0xDF, 0x81, 0x63, 0x3D,
		0x7C, 0x22, 0xC0, 0x9E, 0x1D, 0x43, 0xA1, 0xFF,
		0x46, 0x18, 0xFA, 0xA4, 0x27, 0x79, 0x9B, 0xC5,
		0x84, 0xDA, 0x38, 0x66, 0xE5, 0xBB, 0x59, 0x07,
		0xDB, 0x85, 0x67, 0x39, 0xBA, 0xE4, 0x06, 0x58,
		0x19, 0x47, 0xA5, 0xFB, 0x78, 0x26, 0xC4, 0x9A,
		0x65, 0x3B, 0xD9, 0x87, 0x04, 0x5A, 0xB8, 0xE6,
		0xA7, 0xF9, 0x1B, 0x45, 0xC6, 0x98, 0x7A, 0x24,
		0xF8, 0xA6, 0x44, 0x1A, 0x99, 0xC7, 0x25, 0x7B,
		0x3A, 0x64, 0x86, 0xD8, 0x5B, 0x05, 0xE7, 0xB9,
		0x8C, 0xD2, 0x30, 0x6E, 0xED, 0xB3, 0x51, 0x0F,
		0x4E, 0x10, 0xF2, 0xAC, 0x2F, 0x71, 0x93, 0xCD,
		0x11, 0x4F, 0xAD, 0xF3, 0x70, 0x2E, 0xCC, 0x92,
		0xD3, 0x8D, 0x6F, 0x31, 0xB2, 0xEC, 0x0E, 0x50,
		0xAF, 0xF1, 0x13, 0x4D, 0xCE, 0x90, 0x72, 0x2C,
		0x6D, 0x33, 0xD1, 0x8F, 0x0C, 0x52, 0xB0, 0xEE,
		0x32, 0x6C, 0x8E, 0xD0, 0x53, 0x0D, 0xEF, 0xB1,
		0xF0, 0xAE, 0x4C, 0x12, 0x91, 0xCF, 0x2D, 0x73,
		0xCA, 0x94, 0x76, 0x28, 0xAB, 0xF5, 0x17, 0x49,
		0x08, 0x56, 0xB4, 0xEA, 0x69, 0x37, 0xD5, 0x8B,
		0x57, 0x09, 0xEB, 0xB5, 0x36, 0x68, 0x8A, 0xD4,
		0x95, 0xCB, 0x29, 0x77, 0xF4, 0xAA, 0x48, 0x16,
		0xE9, 0xB7, 0x55, 0x0B, 0x88, 0xD6, 0x34, 0x6A,
		0x2B, 0x75, 0x97, 0xC9, 0x4A, 0x14, 0xF6, 0xA8,
		0x74, 0x2A, 0xC8, 0x96, 0x15, 0x4B, 0xA9, 0xF7,
		0xB6, 0xE8, 0x0A, 0x54, 0xD7, 0x89, 0x6B, 0x35,
	};

	uint8_t crc = 0;

	for (size_t i = 0; i < size; i++) {
		crc = crc_table[data[i] ^ crc];
	}

	return crc;
}

uint8_t SerialIO::calc_checksum(const uint8_t *data, size_t size) {
	uint8_t count = 0;

	for (size_t i = 0; i < size; i++) {
		count += data[i];
	}

	return 256 - count;
}

void SerialIO::tx_message(uint8_t version, uint8_t type,
		const std::vector<uint8_t> &data, const char *desc) {
	std::vector<uint8_t> message(10 + data.size() + 2);

	message[0] = 0xAA;
	message[1] = 10 + data.size() + 1;
	message[2] = 0xA1;
	message[8] = version;
	message[9] = type;

	std::memcpy(&message[10], &data[0], data.size());
	message[10 + data.size()] = calc_crc8(&data[0], data.size());
	message[10 + data.size() + 1] = calc_checksum(&message[1], 10 + data.size());

	debug_message("->", &message[0], message.size(), desc);

	uart_write_bytes(UART_NUM_1, &message[0], message.size());
}

void SerialIO::tx_request_state() {
	static const std::vector<uint8_t> data{
		0x41, 0x81, 0x00, 0xFF, 0x03, 0xFF, 0x00, 0x02, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x03,
	};

	request_state_busy_ = true;

	tx_message(0x03, 0x03, data, "Request state");
}

void SerialIO::tx_network_status() {
	static const std::vector<uint8_t> connected_data{
		0x01, 0x01, 0x04, 0x01, 0x00, 0x00, 0x7F, 0xFF, 0x00, 0x00,
		0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	};
	static const std::vector<uint8_t> disconnected_data{
		0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x01, 0x01,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	};

	tx_message(0x03, 0x0D, false ? connected_data : disconnected_data, "Network status");
}

void SerialIO::debug_message(const char *direction, const uint8_t *data,
		size_t size, const char *desc) {
	std::vector<char> text(size * 3 + 1);

	for (size_t i = 0; i < size; i++) {
		uint8_t h = (data[i] >> 4) & 0xF;
		uint8_t l = data[i] & 0xF;

		text[3 * i] = ' ';
		text[3 * i + 1] = h < 10 ? ('0' + h) : ('A' + h - 10);
		text[3 * i + 2] = l < 10 ? ('0' + l) : ('A' + l - 10);
	}

	ESP_LOGD(TAG, "%s (%s)%s", direction, desc, text.data());
}

} // namespace mist
