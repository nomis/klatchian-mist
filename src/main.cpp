/*
 * klatchian-mist - ESP32 Zigbee dehumidifier controller
 * Copyright 2023,2026  Simon Arlott
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

#include "mist/main.h"

#include <esp_err.h>
#include <esp_log.h>
#include <esp_task_wdt.h>
#include <nvs_flash.h>
#include <driver/gpio.h>

#include "mist/comms.h"
#include "mist/dehumidifier.h"
#include "mist/device.h"
#include "mist/log.h"
#include "mist/ui.h"

using namespace mist;

static_assert(mist::Device::NUM_EP + mist::Dehumidifier::NUM_EP <= ZB_MAX_EP_NUMBER,
	"You'll need to ask Espressif to let you use more endpoints");

extern "C" void app_main() {
	ESP_ERROR_CHECK(esp_task_wdt_add(nullptr));

	esp_err_t err = nvs_flash_init();
	if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		ESP_ERROR_CHECK(nvs_flash_erase());
		err = nvs_flash_init();
	}
	ESP_ERROR_CHECK(err);
	ESP_ERROR_CHECK(esp_task_wdt_reset());

	auto &logging = *new Logging{};

	ESP_ERROR_CHECK(gpio_install_isr_service(ESP_INTR_FLAG_LEVEL2));

	auto &ui = *new UserInterface{logging, GPIO_NUM_4, true};
	auto &device = *new Device{ui, GPIO_NUM_2, GPIO_NUM_3};
	auto &comms = *new SerialIO{GPIO_NUM_10, GPIO_NUM_11};

	ESP_ERROR_CHECK(esp_task_wdt_reset());
	device.start();

	ui.attach(device);
	ui.start();
	ESP_ERROR_CHECK(esp_task_wdt_reset());

	comms.attach(device);
	comms.start();
	ESP_ERROR_CHECK(esp_task_wdt_reset());

	TaskStatus_t status;

	vTaskGetInfo(nullptr, &status, pdTRUE, eRunning);
	ESP_LOGD(TAG, "Free stack: %lu", status.usStackHighWaterMark);

	ESP_ERROR_CHECK(esp_task_wdt_delete(nullptr));
}
