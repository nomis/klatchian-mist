/*
 * klatchian-mist - ESP32 Zigbee dehumidifier controller
 * Copyright 2023-2024,2026  Simon Arlott
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

#include <esp_zigbee_cluster.h>
#include <esp_zigbee_type.h>
#include <nvs.h>
#include <nvs_handle.hpp>
#include <driver/gpio.h>

#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "debounce.h"
#include "device.h"
#include "main.h"
#include "zigbee.h"

namespace mist {

class Device;

namespace dehumidifier {

enum class Mode {
	Unknown = 0,
	SetPoint = 1,
	Continuous = 2,
	Smart = 3,
	ClothesDrying = 4,
};

enum class Fan {
	Unknown = 0,
	Low = 1,
	Medium = 2,
	High = 3,
};

constexpr float MIN_HUMIDITY_SETPOINT = 35;
constexpr float MAX_HUMIDITY_SETPOINT = 85;

class GroupsCluster: public ZigbeeCluster {
public:
	GroupsCluster();
	~GroupsCluster() = default;

	void configure_cluster_list(esp_zb_cluster_list_t &cluster_list) override;
};

class ScenesCluster: public ZigbeeCluster {
public:
	ScenesCluster();
	~ScenesCluster() = default;

	void configure_cluster_list(esp_zb_cluster_list_t &cluster_list) override;
};

class BooleanCluster: public ZigbeeCluster {
protected:
	BooleanCluster(Dehumidifier &dehumidifier, const char *name,
		uint16_t cluster_id, uint16_t attr_id);
	~BooleanCluster() = default;

public:
	void refresh();
	esp_err_t set_attr_value(uint16_t attr_id,
		const esp_zb_zcl_attribute_data_t *value) override;

protected:
	static constexpr const char *TAG = "mist.Dehumidifier";

	void configure_switch_cluster_list(esp_zb_cluster_list_t &cluster_list);
	void configure_binary_input_cluster_list(esp_zb_cluster_list_t &cluster_list,
		uint32_t app_type);
	virtual bool refresh_value() = 0;
	virtual void updated_value(bool value) = 0;

	Dehumidifier &dehumidifier_;

private:
	const char *name_;
	const uint16_t attr_id_;
	uint32_t app_type_{0};
	uint8_t state_{0};
};

class AnalogCluster: public ZigbeeCluster {
protected:
	AnalogCluster(Dehumidifier &dehumidifier, const char *name,
			uint16_t cluster_id, uint16_t attr_id, uint32_t app_type,
			uint16_t eng_units, float min_value, float max_value,
			float resolution);
	~AnalogCluster() = default;

public:
	void refresh();
	esp_err_t set_attr_value(uint16_t attr_id,
			const esp_zb_zcl_attribute_data_t *value) override;

protected:
	static constexpr const char *TAG = "mist.Dehumidifier";

	void configure_analog_input_cluster_list(esp_zb_cluster_list_t &cluster_list);
	void configure_analog_output_cluster_list(esp_zb_cluster_list_t &cluster_list);
	virtual float refresh_value() = 0;
	virtual void updated_value(float value) = 0;

	Dehumidifier &dehumidifier_;

private:
	const char *name_;
	const char *label_prefix_{"Unknown"};
	const char *label_suffix_{"Unknown"};
	const uint16_t attr_id_;
	uint32_t app_type_{0};
	uint16_t eng_units_{0};
	float min_value_{NAN};
	float max_value_{NAN};
	float resolution_{NAN};
	float state_{NAN};
};

class PowerSwitchCluster: public BooleanCluster {
public:
	explicit PowerSwitchCluster(Dehumidifier &dehumidifier);
	~PowerSwitchCluster() = delete;

	void configure_cluster_list(esp_zb_cluster_list_t &cluster_list) override;

protected:
	bool refresh_value() override;
	void updated_value(bool value) override;
};

class AutoDefrostCluster: public BooleanCluster {
public:
	explicit AutoDefrostCluster(Dehumidifier &dehumidifier);
	~AutoDefrostCluster() = delete;

	void configure_cluster_list(esp_zb_cluster_list_t &cluster_list) override;

protected:
	bool refresh_value() override;
	void updated_value(bool value) override;
};

class BucketFullCluster: public BooleanCluster {
public:
	explicit BucketFullCluster(Dehumidifier &dehumidifier);
	~BucketFullCluster() = delete;

	void configure_cluster_list(esp_zb_cluster_list_t &cluster_list) override;

protected:
	bool refresh_value() override;
	void updated_value(bool value) override;
};

class ModeCluster: public AnalogCluster {
public:
	explicit ModeCluster(Dehumidifier &dehumidifier);
	~ModeCluster() = delete;

	void configure_cluster_list(esp_zb_cluster_list_t &cluster_list) override;

protected:
	float refresh_value() override;
	void updated_value(float value) override;
};

class FanSpeedCluster: public AnalogCluster {
public:
	explicit FanSpeedCluster(Dehumidifier &dehumidifier);
	~FanSpeedCluster() = delete;

	void configure_cluster_list(esp_zb_cluster_list_t &cluster_list) override;

protected:
	float refresh_value() override;
	void updated_value(float value) override;
};

class HumidityReadingCluster: public AnalogCluster {
public:
	explicit HumidityReadingCluster(Dehumidifier &dehumidifier);
	~HumidityReadingCluster() = delete;

	void configure_cluster_list(esp_zb_cluster_list_t &cluster_list) override;

protected:
	float refresh_value() override;
	void updated_value(float value) override;
};

class HumiditySetpointCluster: public AnalogCluster {
public:
	explicit HumiditySetpointCluster(Dehumidifier &dehumidifier);
	~HumiditySetpointCluster() = delete;

	void configure_cluster_list(esp_zb_cluster_list_t &cluster_list) override;

protected:
	float refresh_value() override;
	void updated_value(float value) override;
};

class IoniserCluster: public BooleanCluster {
public:
	explicit IoniserCluster(Dehumidifier &dehumidifier);
	~IoniserCluster() = delete;

	void configure_cluster_list(esp_zb_cluster_list_t &cluster_list) override;

protected:
	bool refresh_value() override;
	void updated_value(bool value) override;
};

} // namespace dehumidifier

class Dehumidifier {
public:
	Dehumidifier(gpio_num_t rx_pin, gpio_num_t tx_pin);
	~Dehumidifier() = delete;

	static constexpr const char *TAG = "mist.Dehumidifier";
	static constexpr const size_t NUM_EP = 8;
	void attach(Device &device);

	unsigned long run();
	void refresh();

	bool power() const;
	bool auto_defrost() const;
	bool bucket_full() const;
	dehumidifier::Mode mode() const;
	dehumidifier::Fan fan_speed() const;
	int humidity_reading() const;
	int humidity_setpoint() const;
	bool ioniser() const;

	void power(bool state);
	void mode(dehumidifier::Mode mode);
	void fan_speed(dehumidifier::Fan speed);
	void humidity_setpoint(int humidity);
	void ioniser(bool state);

private:
	static constexpr const ep_id_t POWER_SWITCH_EP_ID = 10;
	static constexpr const ep_id_t AUTO_DEFROST_EP_ID = 11;
	static constexpr const ep_id_t BUCKET_FULL_EP_ID = 12;
	static constexpr const ep_id_t MODE_EP_ID = 20;
	static constexpr const ep_id_t FAN_SPEED_EP_ID = 21;
	static constexpr const ep_id_t HUMIDITY_READING_EP_ID = 30;
	static constexpr const ep_id_t HUMIDITY_SETPOINT_EP_ID = 31;
	static constexpr const ep_id_t IONISER_EP_ID = 40;

	mutable std::mutex mutex_;
	bool power_{false};
	bool auto_defrost_{false};
	bool bucket_full_{false};
	dehumidifier::Mode mode_{0};
	dehumidifier::Fan fan_speed_{0};
	int humidity_reading_{0};
	int humidity_setpoint_{0};
	bool ioniser_{false};

	dehumidifier::PowerSwitchCluster &power_switch_cl_;
	dehumidifier::AutoDefrostCluster &auto_defrost_cl_;
	dehumidifier::BucketFullCluster &bucket_full_cl_;
	dehumidifier::ModeCluster &mode_cl_;
	dehumidifier::FanSpeedCluster &fan_speed_cl_;
	dehumidifier::HumidityReadingCluster &humidity_reading_cl_;
	dehumidifier::HumiditySetpointCluster &humidity_setpoint_cl_;
	dehumidifier::IoniserCluster &ioniser_cl_;

	Device *device_{nullptr};
};

} // namespace mist
