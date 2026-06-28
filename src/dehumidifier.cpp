/*
 * klatchian-mist - ESP32 Zigbee dehumidifier controller
 * Copyright 2023-2026  Simon Arlott
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

#include "mist/dehumidifier.h"

#include <esp_crc.h>
#include <esp_err.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <esp_zigbee_cluster.h>
#include <esp_zigbee_type.h>
#include <driver/gpio.h>
#include <ha/esp_zigbee_ha_standard.h>

#include <cmath>
#include <memory>
#include <string>
#include <thread>

#include "mist/debounce.h"
#include "mist/device.h"
#include "mist/ui.h"
#include "mist/util.h"
#include "zcl/esp_zigbee_zcl_fan_control.h"

#ifndef ESP_ZB_HA_ON_OFF_LIGHT_SWITCH_DEVICE_ID
# define ESP_ZB_HA_ON_OFF_LIGHT_SWITCH_DEVICE_ID (static_cast<esp_zb_ha_standard_devices_t>(0x0103))
#endif

namespace mist {

Dehumidifier::Dehumidifier(gpio_num_t rx_pin, gpio_num_t tx_pin) :
		power_switch_cl_(*new dehumidifier::PowerSwitchCluster{*this}),
		auto_defrost_cl_(*new dehumidifier::AutoDefrostCluster{*this}),
		bucket_full_cl_(*new dehumidifier::BucketFullCluster{*this}),
		mode_cl_(*new dehumidifier::ModeCluster{*this}),
		fan_speed_cl_(*new dehumidifier::FanSpeedCluster{*this}),
		humidity_reading_cl_(*new dehumidifier::HumidityReadingCluster{*this}),
		humidity_setpoint_cl_(*new dehumidifier::HumiditySetpointCluster{*this}),
		ioniser_cl_(*new dehumidifier::IoniserCluster{*this}) {
}

void Dehumidifier::attach(Device &device) {
	device_ = &device;
	device.attach({
		*new ZigbeeEndpoint{
			static_cast<ep_id_t>(POWER_SWITCH_EP_ID),
			ESP_ZB_AF_HA_PROFILE_ID, ESP_ZB_HA_HEATING_COOLING_UNIT_DEVICE_ID,
			{
				power_switch_cl_,
				*new dehumidifier::GroupsCluster{},
				*new dehumidifier::ScenesCluster{}
			}},
		*new ZigbeeEndpoint{
			static_cast<ep_id_t>(AUTO_DEFROST_EP_ID),
			ESP_ZB_AF_HA_PROFILE_ID, ESP_ZB_HA_HEATING_COOLING_UNIT_DEVICE_ID,
			{auto_defrost_cl_}},
		*new ZigbeeEndpoint{
			static_cast<ep_id_t>(BUCKET_FULL_EP_ID),
			ESP_ZB_AF_HA_PROFILE_ID, ESP_ZB_HA_HEATING_COOLING_UNIT_DEVICE_ID,
			{bucket_full_cl_}},
		*new ZigbeeEndpoint{
			static_cast<ep_id_t>(MODE_EP_ID),
			ESP_ZB_AF_HA_PROFILE_ID, ESP_ZB_HA_HEATING_COOLING_UNIT_DEVICE_ID,
			{mode_cl_}},
		*new ZigbeeEndpoint{
			static_cast<ep_id_t>(FAN_SPEED_EP_ID),
			ESP_ZB_AF_HA_PROFILE_ID, ESP_ZB_HA_HEATING_COOLING_UNIT_DEVICE_ID,
			{fan_speed_cl_}},
		*new ZigbeeEndpoint{
			static_cast<ep_id_t>(HUMIDITY_READING_EP_ID),
			ESP_ZB_AF_HA_PROFILE_ID, ESP_ZB_HA_HEATING_COOLING_UNIT_DEVICE_ID,
			{humidity_reading_cl_}},
		*new ZigbeeEndpoint{
			static_cast<ep_id_t>(HUMIDITY_SETPOINT_EP_ID),
			ESP_ZB_AF_HA_PROFILE_ID, ESP_ZB_HA_HEATING_COOLING_UNIT_DEVICE_ID,
			{humidity_setpoint_cl_}},
		*new ZigbeeEndpoint{
			static_cast<ep_id_t>(IONISER_EP_ID),
			ESP_ZB_AF_HA_PROFILE_ID, ESP_ZB_HA_HEATING_COOLING_UNIT_DEVICE_ID,
			{ioniser_cl_}},
	});
}

unsigned long Dehumidifier::run() {
	return ULONG_MAX;
}

bool Dehumidifier::power() const {
	std::lock_guard lock{mutex_};
	return power_;
}

bool Dehumidifier::auto_defrost() const {
	std::lock_guard lock{mutex_};
	return auto_defrost_;
}

bool Dehumidifier::bucket_full() const {
	std::lock_guard lock{mutex_};
	return bucket_full_;
}

dehumidifier::Mode Dehumidifier::mode() const {
	std::lock_guard lock{mutex_};
	return mode_;
}

dehumidifier::Fan Dehumidifier::fan_speed() const {
	std::lock_guard lock{mutex_};
	return fan_speed_;
}

int Dehumidifier::humidity_reading() const {
	std::lock_guard lock{mutex_};
	return humidity_reading_;
}

int Dehumidifier::humidity_setpoint() const {
	std::lock_guard lock{mutex_};
	return humidity_setpoint_;
}

bool Dehumidifier::ioniser() const {
	std::lock_guard lock{mutex_};
	return ioniser_;
}

void Dehumidifier::power(bool state) {
	std::lock_guard lock{mutex_};

	ESP_LOGD(TAG, "Set power %d -> %d", power_, state);
	power_ = state;
	device_->ui().remote_control();
}

void Dehumidifier::mode(dehumidifier::Mode mode) {
	std::lock_guard lock{mutex_};

	ESP_LOGD(TAG, "Set mode %d -> %d", mode_, mode);
	mode_ = mode;
	device_->ui().remote_control();
}

void Dehumidifier::fan_speed(dehumidifier::Fan speed) {
	std::lock_guard lock{mutex_};

	ESP_LOGD(TAG, "Set fan speed %d -> %d", fan_speed_, speed);
	fan_speed_ = speed;
	device_->ui().remote_control();
}

void Dehumidifier::humidity_setpoint(int humidity) {
	std::lock_guard lock{mutex_};

	ESP_LOGD(TAG, "Set humidity setpoint %d -> %d", humidity_setpoint_, humidity);
	humidity_setpoint_ = humidity;
	device_->ui().remote_control();
}

void Dehumidifier::ioniser(bool state) {
	std::lock_guard lock{mutex_};

	ESP_LOGD(TAG, "Set ioniser %d -> %d", ioniser_, state);
	ioniser_ = state;
	device_->ui().remote_control();
}

void Dehumidifier::refresh() {
	power_switch_cl_.refresh();
	auto_defrost_cl_.refresh();
	bucket_full_cl_.refresh();
	mode_cl_.refresh();
	fan_speed_cl_.refresh();
	humidity_reading_cl_.refresh();
	humidity_setpoint_cl_.refresh();
	ioniser_cl_.refresh();
}

namespace dehumidifier {

GroupsCluster::GroupsCluster() : ZigbeeCluster(
		ESP_ZB_ZCL_CLUSTER_ID_GROUPS, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE) {
}

void GroupsCluster::configure_cluster_list(esp_zb_cluster_list_t &cluster_list) {
	ESP_ERROR_CHECK(esp_zb_cluster_list_add_groups_cluster(&cluster_list,
		esp_zb_groups_cluster_create(nullptr), role()));
}

ScenesCluster::ScenesCluster() : ZigbeeCluster(
		ESP_ZB_ZCL_CLUSTER_ID_SCENES, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE) {
}

void ScenesCluster::configure_cluster_list(esp_zb_cluster_list_t &cluster_list) {
	ESP_ERROR_CHECK(esp_zb_cluster_list_add_scenes_cluster(&cluster_list,
		esp_zb_scenes_cluster_create(nullptr), role()));
}

BooleanCluster::BooleanCluster(Dehumidifier &dehumidifier, const char *name,
		uint16_t cluster_id, uint16_t attr_id) : ZigbeeCluster(cluster_id,
			ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, {attr_id}),
			dehumidifier_(dehumidifier), name_(name), attr_id_(attr_id) {
}

void BooleanCluster::configure_switch_cluster_list(esp_zb_cluster_list_t &cluster_list) {
	esp_zb_on_off_cluster_cfg_t switch_cfg{};
	switch_cfg.on_off = (state_ = refresh_value() ? 1 : 0);

	ESP_ERROR_CHECK(esp_zb_cluster_list_add_on_off_cluster(&cluster_list,
		esp_zb_on_off_cluster_create(&switch_cfg), role()));
}

void BooleanCluster::configure_binary_input_cluster_list(esp_zb_cluster_list_t &cluster_list,
		uint32_t app_type) {
	esp_zb_binary_input_cluster_cfg_t input_cfg = {
		.out_of_service = 0,
		.status_flags = 0,
		.present_value = refresh_value(),
	};

	app_type_ = app_type;
	state_ = input_cfg.present_value ? 1 : 0;

	esp_zb_attribute_list_t *input_cluster = esp_zb_binary_input_cluster_create(&input_cfg);

	ESP_ERROR_CHECK(esp_zb_cluster_update_attr(input_cluster,
			ESP_ZB_ZCL_ATTR_BINARY_INPUT_PRESENT_VALUE_ID, &state_));

	ESP_ERROR_CHECK(esp_zb_binary_input_cluster_add_attr(input_cluster,
			ESP_ZB_ZCL_ATTR_BINARY_INPUT_APPLICATION_TYPE_ID, &app_type_));

	ESP_ERROR_CHECK(esp_zb_binary_input_cluster_add_attr(input_cluster,
			ESP_ZB_ZCL_ATTR_BINARY_INPUT_DESCRIPTION_ID,
			ZigbeeString(name_).data()));

	ESP_ERROR_CHECK(esp_zb_cluster_list_add_binary_input_cluster(&cluster_list,
		input_cluster, role()));
}

void BooleanCluster::refresh() {
	uint8_t new_state = refresh_value() ? 1 : 0;

	if (new_state != state_) {
		state_ = new_state;
		ESP_LOGD(TAG, "Report %s %u", name_, state_);

		update_attr_value(attr_id_, &state_);
	}
}

esp_err_t BooleanCluster::set_attr_value(uint16_t attr_id,
		const esp_zb_zcl_attribute_data_t *data) {
	if (attr_id == attr_id_) {
		if (data->type == ESP_ZB_ZCL_ATTR_TYPE_BOOL
				&& data->size == sizeof(uint8_t)) {
			state_ = *(uint8_t *)data->value;
			updated_value(state_);
			return ESP_OK;
		}
	}
	return ESP_ERR_INVALID_ARG;
}

AnalogCluster::AnalogCluster(Dehumidifier &dehumidifier, const char *name,
				uint16_t cluster_id, uint16_t attr_id, uint32_t app_type,
				uint16_t eng_units, float min_value, float max_value,
				float resolution)
						: ZigbeeCluster(cluster_id, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
						{attr_id}), dehumidifier_(dehumidifier), name_(name),
						attr_id_(attr_id), app_type_(app_type),
						eng_units_(eng_units), min_value_(min_value),
						max_value_(max_value), resolution_(resolution) {
}

void AnalogCluster::configure_analog_input_cluster_list(esp_zb_cluster_list_t &cluster_list) {
		esp_zb_attribute_list_t *input_cluster = esp_zb_analog_input_cluster_create(nullptr);

		state_ = refresh_value();

		ESP_ERROR_CHECK(esp_zb_cluster_update_attr(input_cluster,
						ESP_ZB_ZCL_ATTR_ANALOG_INPUT_PRESENT_VALUE_ID, &state_));

		ESP_ERROR_CHECK(esp_zb_analog_input_cluster_add_attr(input_cluster,
						ESP_ZB_ZCL_ATTR_ANALOG_INPUT_MIN_PRESENT_VALUE_ID, &min_value_));

		ESP_ERROR_CHECK(esp_zb_analog_input_cluster_add_attr(input_cluster,
						ESP_ZB_ZCL_ATTR_ANALOG_INPUT_MAX_PRESENT_VALUE_ID, &max_value_));

		ESP_ERROR_CHECK(esp_zb_analog_input_cluster_add_attr(input_cluster,
						ESP_ZB_ZCL_ATTR_ANALOG_INPUT_RESOLUTION_ID, &resolution_));

		ESP_ERROR_CHECK(esp_zb_analog_input_cluster_add_attr(input_cluster,
						ESP_ZB_ZCL_ATTR_ANALOG_INPUT_APPLICATION_TYPE_ID, &app_type_));

		ESP_ERROR_CHECK(esp_zb_analog_input_cluster_add_attr(input_cluster,
						ESP_ZB_ZCL_ATTR_ANALOG_INPUT_ENGINEERING_UNITS_ID, &eng_units_));

		ESP_ERROR_CHECK(esp_zb_analog_input_cluster_add_attr(input_cluster,
						ESP_ZB_ZCL_ATTR_ANALOG_INPUT_DESCRIPTION_ID,
						ZigbeeString(name_).data()));

		ESP_ERROR_CHECK(esp_zb_cluster_list_add_analog_input_cluster(&cluster_list,
				input_cluster, role()));
}

void AnalogCluster::configure_analog_output_cluster_list(esp_zb_cluster_list_t &cluster_list) {
		esp_zb_attribute_list_t *output_cluster = esp_zb_analog_output_cluster_create(nullptr);

		state_ = refresh_value();

		ESP_ERROR_CHECK(esp_zb_cluster_update_attr(output_cluster,
						ESP_ZB_ZCL_ATTR_ANALOG_OUTPUT_PRESENT_VALUE_ID, &state_));

		ESP_ERROR_CHECK(esp_zb_analog_output_cluster_add_attr(output_cluster,
						ESP_ZB_ZCL_ATTR_ANALOG_OUTPUT_MIN_PRESENT_VALUE_ID, &min_value_));

		ESP_ERROR_CHECK(esp_zb_analog_output_cluster_add_attr(output_cluster,
						ESP_ZB_ZCL_ATTR_ANALOG_OUTPUT_MAX_PRESENT_VALUE_ID, &max_value_));

		ESP_ERROR_CHECK(esp_zb_analog_output_cluster_add_attr(output_cluster,
						ESP_ZB_ZCL_ATTR_ANALOG_OUTPUT_RESOLUTION_ID, &resolution_));

		ESP_ERROR_CHECK(esp_zb_analog_output_cluster_add_attr(output_cluster,
						ESP_ZB_ZCL_ATTR_ANALOG_OUTPUT_APPLICATION_TYPE_ID, &app_type_));

		ESP_ERROR_CHECK(esp_zb_analog_output_cluster_add_attr(output_cluster,
						ESP_ZB_ZCL_ATTR_ANALOG_OUTPUT_ENGINEERING_UNITS_ID, &eng_units_));

		ESP_ERROR_CHECK(esp_zb_analog_output_cluster_add_attr(output_cluster,
						ESP_ZB_ZCL_ATTR_ANALOG_OUTPUT_DESCRIPTION_ID,
						ZigbeeString(name_).data()));

		ESP_ERROR_CHECK(esp_zb_cluster_list_add_analog_output_cluster(&cluster_list,
				output_cluster, role()));
}

void AnalogCluster::refresh() {
		uint64_t new_value = refresh_value();

		if (new_value != state_) {
				state_ = new_value;
				ESP_LOGD(TAG, "Report %s %.6f", name_, state_);

				update_attr_value(attr_id_, &state_);
		}
}

esp_err_t AnalogCluster::set_attr_value(uint16_t attr_id,
				const esp_zb_zcl_attribute_data_t *data) {
		if (attr_id == attr_id_) {
				if (data->type == ESP_ZB_ZCL_ATTR_TYPE_SINGLE
								&& data->size == sizeof(float)) {
						state_ = *(float *)data->value;
						updated_value(state_);
						return ESP_OK;
				}
		}
		return ESP_ERR_INVALID_ARG;
}

PowerSwitchCluster::PowerSwitchCluster(Dehumidifier &dehumidifier)
		: BooleanCluster(dehumidifier, "power switch",
			ESP_ZB_ZCL_CLUSTER_ID_ON_OFF, ESP_ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID) {
}

void PowerSwitchCluster::configure_cluster_list(esp_zb_cluster_list_t &cluster_list) {
	configure_switch_cluster_list(cluster_list);
}

bool PowerSwitchCluster::refresh_value() {
	return dehumidifier_.power();
}

void PowerSwitchCluster::updated_value(bool state) {
	dehumidifier_.power(state);
}

AutoDefrostCluster::AutoDefrostCluster(Dehumidifier &dehumidifier)
		: BooleanCluster(dehumidifier, "auto defrost",
			ESP_ZB_ZCL_CLUSTER_ID_ON_OFF, ESP_ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID) {
}

void AutoDefrostCluster::configure_cluster_list(esp_zb_cluster_list_t &cluster_list) {
	configure_binary_input_cluster_list(cluster_list,
		  (  0x03 << 24)  /* Group: Binary Input            */
		| (  0x00 << 16)  /* Type:  Application Domain HVAC */
		|  0x0044         /* Index: Humidifier Alarm BI     */);
}

bool AutoDefrostCluster::refresh_value() {
	return dehumidifier_.auto_defrost();
}

void AutoDefrostCluster::updated_value(bool state) {
	/* Ignored */
}

BucketFullCluster::BucketFullCluster(Dehumidifier &dehumidifier)
		: BooleanCluster(dehumidifier, "Bucket Full",
			ESP_ZB_ZCL_CLUSTER_ID_ON_OFF, ESP_ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID) {
}

void BucketFullCluster::configure_cluster_list(esp_zb_cluster_list_t &cluster_list) {
	configure_binary_input_cluster_list(cluster_list,
		  (  0x03 << 24)  /* Group: Binary Input            */
		| (  0x00 << 16)  /* Type:  Application Domain HVAC */
		|  0x0046         /* Index: Humidifier Overload BI  */);
}

bool BucketFullCluster::refresh_value() {
	return dehumidifier_.bucket_full();
}

void BucketFullCluster::updated_value(bool state) {
	/* Ignored */
}

ModeCluster::ModeCluster(Dehumidifier &dehumidifier)
		: AnalogCluster(dehumidifier, "Mode",
			ESP_ZB_ZCL_CLUSTER_ID_ANALOG_OUTPUT,
			ESP_ZB_ZCL_ATTR_ANALOG_OUTPUT_PRESENT_VALUE_ID,
			  (  0x01 << 24)  /* Group: Analog Output                   */
			| (  0xFF << 16)  /* Type:  Other                           */
			|  0xFFFF         /* Index: Other                           */,
			   0x00FF         /* Other                                  */,
			1, 4, 1) {
}

void ModeCluster::configure_cluster_list(esp_zb_cluster_list_t &cluster_list) {
	configure_analog_output_cluster_list(cluster_list);
}

float ModeCluster::refresh_value() {
	return (float)static_cast<std::underlying_type_t<Mode>>(dehumidifier_.mode());
}

void ModeCluster::updated_value(float value) {
		switch ((int)std::rint(value)) {
	case static_cast<std::underlying_type_t<Mode>>(Mode::SetPoint):
		dehumidifier_.mode(Mode::SetPoint);
		break;

	case static_cast<std::underlying_type_t<Mode>>(Mode::Continuous):
		dehumidifier_.mode(Mode::Continuous);
		break;

	case static_cast<std::underlying_type_t<Mode>>(Mode::Smart):
		dehumidifier_.mode(Mode::Smart);
		break;

	case static_cast<std::underlying_type_t<Mode>>(Mode::ClothesDrying):
		dehumidifier_.mode(Mode::ClothesDrying);
		break;

	default:
		dehumidifier_.mode(Mode::Unknown);
		break;
	}
}

FanSpeedCluster::FanSpeedCluster(Dehumidifier &dehumidifier)
		: AnalogCluster(dehumidifier, "Fan Speed",
			ESP_ZB_ZCL_CLUSTER_ID_ANALOG_OUTPUT,
			ESP_ZB_ZCL_ATTR_ANALOG_OUTPUT_PRESENT_VALUE_ID,
			  (  0x01 << 24)  /* Group: Analog Output                   */
			| (  0xFF << 16)  /* Type:  Other                           */
			|  0xFFFF         /* Index: Other                           */,
			   0x00FF         /* Other                                  */,
			1, 3, 1) {

}

void FanSpeedCluster::configure_cluster_list(esp_zb_cluster_list_t &cluster_list) {
	configure_analog_output_cluster_list(cluster_list);
}

float FanSpeedCluster::refresh_value() {
	return (float)static_cast<std::underlying_type_t<Fan>>(dehumidifier_.fan_speed());
}

void FanSpeedCluster::updated_value(float value) {
		switch ((int)std::rint(value)) {
	case static_cast<std::underlying_type_t<Fan>>(Fan::Low):
		dehumidifier_.fan_speed(Fan::Low);
		break;

	case static_cast<std::underlying_type_t<Fan>>(Fan::Medium):
		dehumidifier_.fan_speed(Fan::Medium);
		break;

	case static_cast<std::underlying_type_t<Fan>>(Fan::High):
		dehumidifier_.fan_speed(Fan::High);
		break;

	default:
		dehumidifier_.fan_speed(Fan::Unknown);
		break;
	}
}

HumidityReadingCluster::HumidityReadingCluster(Dehumidifier &dehumidifier)
		: AnalogCluster(dehumidifier, "Humidity Reading",
			ESP_ZB_ZCL_CLUSTER_ID_ANALOG_INPUT,
			ESP_ZB_ZCL_ATTR_ANALOG_INPUT_PRESENT_VALUE_ID,
			  (  0x00 << 24)  /* Group: Analog Input           */
			| (  0x01 << 16)  /* Type:  Relative Humidity in % */
			|  0x0008         /* Index: Zone Humidity          */,
			       29         /* Percent Relative Humidity     */,
			0, 100, 1) {
}

void HumidityReadingCluster::configure_cluster_list(esp_zb_cluster_list_t &cluster_list) {
	configure_analog_input_cluster_list(cluster_list);
}

float HumidityReadingCluster::refresh_value() {
	return dehumidifier_.humidity_reading();
}

void HumidityReadingCluster::updated_value(float value) {
	/* Ignored */
}

HumiditySetpointCluster::HumiditySetpointCluster(Dehumidifier &dehumidifier)
		: AnalogCluster(dehumidifier, "Humidity Setpoint",
			ESP_ZB_ZCL_CLUSTER_ID_ANALOG_OUTPUT,
			ESP_ZB_ZCL_ATTR_ANALOG_OUTPUT_PRESENT_VALUE_ID,
			  (  0x01 << 24)  /* Group: Analog Output                   */
			| (  0x01 << 16)  /* Type:  Relative Humidity in %          */
			|  0x0008         /* Index: Zone Relative Humidity Setpoint */,
			       29         /* Percent Relative Humidity              */,
			MIN_HUMIDITY_SETPOINT, MAX_HUMIDITY_SETPOINT, 5) {
}

void HumiditySetpointCluster::configure_cluster_list(esp_zb_cluster_list_t &cluster_list) {
	configure_analog_output_cluster_list(cluster_list);
}

float HumiditySetpointCluster::refresh_value() {
	return dehumidifier_.humidity_setpoint();
}

void HumiditySetpointCluster::updated_value(float value) {
	dehumidifier_.humidity_setpoint(value);
}

IoniserCluster::IoniserCluster(Dehumidifier &dehumidifier)
		: BooleanCluster(dehumidifier, "Ioniser",
			ESP_ZB_ZCL_CLUSTER_ID_ON_OFF, ESP_ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID) {
}

void IoniserCluster::configure_cluster_list(esp_zb_cluster_list_t &cluster_list) {
	configure_switch_cluster_list(cluster_list);
}

bool IoniserCluster::refresh_value() {
	return dehumidifier_.ioniser();
}

void IoniserCluster::updated_value(bool state) {
	dehumidifier_.ioniser(state);
}

} // namespace dehumidifier

} // namespace mist
