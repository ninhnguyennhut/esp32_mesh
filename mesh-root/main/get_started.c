/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2018 <ESPRESSIF SYSTEMS (SHANGHAI) PTE LTD>
 *
 * Permission is hereby granted for use on all ESPRESSIF SYSTEMS products, in which case,
 * it is free of charge, to any person obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include "mdf_common.h"
#include "mwifi.h"
#include "driver/gpio.h"
#include "mlink.h"

#include "app_data.h"

#include "ssd1306.h"
#include "font8x8_basic.h"

control_t node_cmd;

const mesh_addr_t node_info_broadcast_group = { .addr = { 0x01, 0x00, 0x5e, 0xae, 0xae, 0xae } };

#define MESH_LIST_MAGIC 0xcc

struct mesh_list_hdr {
	uint8_t magic;
	uint8_t num_entries;
	mesh_addr_t entries[0];
}  __attribute__((packed));

// #define MEMORY_DEBUG

static const char *TAG = "get_started";
#define tag "SSD1306"


#define GPIO_OUTPUT_IO_0    18
#define GPIO_OUTPUT_IO_1    19
#define GPIO_OUTPUT_PIN_SEL  ((1ULL<<GPIO_OUTPUT_IO_0) | (1ULL<<GPIO_OUTPUT_IO_1))

void network_status_led() {
	gpio_config_t io_conf;
	//disable interrupt
	io_conf.intr_type = GPIO_PIN_INTR_DISABLE;
	//set as output mode
	io_conf.mode = GPIO_MODE_OUTPUT;
	//bit mask of the pins that you want to set,e.g.GPIO18/19
	io_conf.pin_bit_mask = GPIO_OUTPUT_PIN_SEL;
	//disable pull-down mode
	io_conf.pull_down_en = 0;
	//disable pull-up mode
	io_conf.pull_up_en = 0;
	//configure GPIO with the given settings
	gpio_config(&io_conf);
}

void ssd1306_config() {
	SSD1306_t dev;
	int center, top, bottom;
	char lineChar[20];

	ESP_LOGI(tag, "INTERFACE is i2c");
	ESP_LOGI(tag, "CONFIG_SDA_GPIO=%d",CONFIG_SDA_GPIO);
	ESP_LOGI(tag, "CONFIG_SCL_GPIO=%d",CONFIG_SCL_GPIO);
	ESP_LOGI(tag, "CONFIG_RESET_GPIO=%d",CONFIG_RESET_GPIO);
	i2c_master_init(&dev, CONFIG_SDA_GPIO, CONFIG_SCL_GPIO, CONFIG_RESET_GPIO);

	ESP_LOGI(tag, "Panel is 128x64");
	ssd1306_init(&dev, 128, 64);

	ssd1306_clear_screen(&dev, false);
	ssd1306_contrast(&dev, 0xff);

	top = 2;
	center = 3;
	bottom = 8;

	// Scroll Up
	ssd1306_clear_screen(&dev, false);
	ssd1306_contrast(&dev, 0xff);
	ssd1306_display_text(&dev, 0, "---Scroll  UP---", 16, true);
	//ssd1306_software_scroll(&dev, 7, 1);
	ssd1306_software_scroll(&dev, (dev._pages - 1), 1);
	for (int line=0;line<bottom+10;line++) {
		lineChar[0] = 0x01;
		sprintf(&lineChar[1], " Line %02d", line);
		ssd1306_scroll_text(&dev, lineChar, strlen(lineChar), false);
		vTaskDelay(500 / portTICK_PERIOD_MS);
	}
	vTaskDelay(3000 / portTICK_PERIOD_MS);

}

static void root_task(void *arg)
{
	mdf_err_t ret                    = MDF_OK;
	char *data                       = MDF_MALLOC(MWIFI_PAYLOAD_LEN);
	size_t size                      = MWIFI_PAYLOAD_LEN;
	uint8_t src_addr[MWIFI_ADDR_LEN] = {0x0};
	mwifi_data_type_t data_type      = {0};

	MDF_LOGI("Root is running");

	for (int i = 0;; ++i) {
		if (!mwifi_is_started()) {
			vTaskDelay(500 / portTICK_RATE_MS);
			continue;
		}

		size = MWIFI_PAYLOAD_LEN;
		memset(data, 0, MWIFI_PAYLOAD_LEN);
		ret = mwifi_root_read(src_addr, &data_type, data, &size, portMAX_DELAY);
		MDF_ERROR_CONTINUE(ret != MDF_OK, "<%s> mwifi_root_read", mdf_err_to_name(ret));
		MDF_LOGI("Root receive, addr: " MACSTR ", size: %d, data: %s", MAC2STR(src_addr), size, data);

		size = sprintf(data, "(%d) Hello node!", i);
		ret = mwifi_root_write(src_addr, 1, &data_type, data, size, true);
		MDF_ERROR_CONTINUE(ret != MDF_OK, "mwifi_root_recv, ret: %x", ret);
		MDF_LOGI("Root send, addr: " MACSTR ", size: %d, data: %s", MAC2STR(src_addr), size, data);
	}

	MDF_LOGW("Root is exit");

	MDF_FREE(data);
	vTaskDelete(NULL);
}

static void node_read_task(void *arg)
{
	mdf_err_t ret = MDF_OK;
	char *data    = MDF_MALLOC(MWIFI_PAYLOAD_LEN);
	size_t size   = MWIFI_PAYLOAD_LEN;
	mwifi_data_type_t data_type      = {0x0};
	uint8_t src_addr[MWIFI_ADDR_LEN] = {0x0};

	MDF_LOGI("Note read task is running");

	int cnt = 0;

	for (;;) {
		if (!mwifi_is_connected()) {
			vTaskDelay(500 / portTICK_RATE_MS);
			continue;
		}

		cnt++;

		gpio_set_level(GPIO_OUTPUT_IO_0, cnt % 2);
		gpio_set_level(GPIO_OUTPUT_IO_1, cnt % 2);

		size = MWIFI_PAYLOAD_LEN;
		memset(data, 0, MWIFI_PAYLOAD_LEN);
		ret = mwifi_read(src_addr, &data_type, data, &size, portMAX_DELAY);
		MDF_ERROR_CONTINUE(ret != MDF_OK, "mwifi_read, ret: %x", ret);
		MDF_LOGI("[Fanning] Node receive, addr: " MACSTR ", size: %d, data: %s", MAC2STR(src_addr), size, data);
	}

	MDF_LOGW("Note read task is exit");

	MDF_FREE(data);
	vTaskDelete(NULL);
}

void node_write_task(void *arg)
{
	mdf_err_t ret = MDF_OK;
	int count     = 0;
	size_t size   = 0;
	char *data    = MDF_MALLOC(MWIFI_PAYLOAD_LEN);
	mwifi_data_type_t data_type = {0x0};

	MDF_LOGI("Node write task is running");

	for (;;) {
		if (!mwifi_is_connected()) {
			vTaskDelay(500 / portTICK_RATE_MS);
			continue;
		}

		size = sprintf(data, "(%d) Hello root!", count++);
		ret = mwifi_write(NULL, &data_type, data, size, true);
		MDF_ERROR_CONTINUE(ret != MDF_OK, "mwifi_write, ret: %x", ret);

		vTaskDelay(1000 / portTICK_RATE_MS);
	}

	MDF_LOGW("Node write task is exit");

	MDF_FREE(data);
	vTaskDelete(NULL);
}

static void ssd1306_display_task(void *arg)
{

	mdf_err_t ret = MDF_OK;
	int count     = 0;
	size_t size   = 0;
	char *data    = MDF_MALLOC(MWIFI_PAYLOAD_LEN);
	mwifi_data_type_t data_type = {0x0};

	SSD1306_t dev;
	int center, top, bottom;
	char lineChar[20];

	ESP_LOGI(tag, "INTERFACE is i2c");
	ESP_LOGI(tag, "CONFIG_SDA_GPIO=%d",CONFIG_SDA_GPIO);
	ESP_LOGI(tag, "CONFIG_SCL_GPIO=%d",CONFIG_SCL_GPIO);
	ESP_LOGI(tag, "CONFIG_RESET_GPIO=%d",CONFIG_RESET_GPIO);
	i2c_master_init(&dev, CONFIG_SDA_GPIO, CONFIG_SCL_GPIO, CONFIG_RESET_GPIO);

	ESP_LOGI(tag, "Panel is 128x64");
	ssd1306_init(&dev, 128, 64);

	ssd1306_clear_screen(&dev, false);
	ssd1306_contrast(&dev, 0xff);

	top = 2;
	center = 3;
	bottom = 8;

	MDF_LOGI("ssd1306 task is running");

	for (;;) {

		// Scroll Up
		ssd1306_clear_screen(&dev, false);
		ssd1306_contrast(&dev, 0xff);
		ssd1306_display_text(&dev, 0, "---Scroll  UP---", 16, true);
		//ssd1306_software_scroll(&dev, 7, 1);
		ssd1306_software_scroll(&dev, (dev._pages - 1), 1);
		for (int line=0;line<bottom+10;line++) {
			lineChar[0] = 0x01;
			sprintf(&lineChar[1], " Line %02d", line);
			ssd1306_scroll_text(&dev, lineChar, strlen(lineChar), false);
			vTaskDelay(500 / portTICK_PERIOD_MS);
		}

		vTaskDelay(500 / portTICK_RATE_MS);

	}

	MDF_LOGW("ssd1306 task is exit");

	MDF_FREE(data);
	vTaskDelete(NULL);
}

/**
 * @brief Timed printing system information
 */
static void print_system_info_timercb(void *timer)
{
	uint8_t primary                 = 0;
	wifi_second_chan_t second       = 0;
	mesh_addr_t parent_bssid        = {0};
	uint8_t sta_mac[MWIFI_ADDR_LEN] = {0};
	mesh_assoc_t mesh_assoc         = {0x0};
	wifi_sta_list_t wifi_sta_list   = {0x0};

	esp_wifi_get_mac(ESP_IF_WIFI_STA, sta_mac);
	esp_wifi_ap_get_sta_list(&wifi_sta_list);
	esp_wifi_get_channel(&primary, &second);
	esp_wifi_vnd_mesh_get(&mesh_assoc);
	esp_mesh_get_parent_bssid(&parent_bssid);

	MDF_LOGI("System information, channel: %d, layer: %d, self mac: " MACSTR ", parent bssid: " MACSTR
			 ", parent rssi: %d, node num: %d, free heap: %u", primary,
			 esp_mesh_get_layer(), MAC2STR(sta_mac), MAC2STR(parent_bssid.addr),
			 mesh_assoc.rssi, esp_mesh_get_total_node_num(), esp_get_free_heap_size());

	for (int i = 0; i < wifi_sta_list.num; i++) {
		MDF_LOGI("Child mac: " MACSTR, MAC2STR(wifi_sta_list.sta[i].mac));
	}

#ifdef MEMORY_DEBUG
	if (!heap_caps_check_integrity_all(true)) {
		MDF_LOGE("At least one heap is corrupt");
	}

	mdf_mem_print_heap();
	mdf_mem_print_record();
#endif /**< MEMORY_DEBUG */
}

static mdf_err_t wifi_init()
{
	mdf_err_t ret          = nvs_flash_init();
	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

	if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		MDF_ERROR_ASSERT(nvs_flash_erase());
		ret = nvs_flash_init();
	}

	MDF_ERROR_ASSERT(ret);

	tcpip_adapter_init();
	MDF_ERROR_ASSERT(esp_event_loop_init(NULL, NULL));
	MDF_ERROR_ASSERT(esp_wifi_init(&cfg));
	MDF_ERROR_ASSERT(esp_wifi_set_storage(WIFI_STORAGE_FLASH));
	MDF_ERROR_ASSERT(esp_wifi_set_mode(WIFI_MODE_STA));
	MDF_ERROR_ASSERT(esp_wifi_set_ps(WIFI_PS_NONE));
	MDF_ERROR_ASSERT(esp_mesh_set_6m_rate(false));
	MDF_ERROR_ASSERT(esp_wifi_start());

	return MDF_OK;
}

/**
 * @brief All module events will be sent to this task in esp-mdf
 *
 * @Note:
 *     1. Do not block or lengthy operations in the callback function.
 *     2. Do not consume a lot of memory in the callback function.
 *        The task memory of the callback function is only 4KB.
 */
static mdf_err_t event_loop_cb(mdf_event_loop_t event, void *ctx)
{
	MDF_LOGI("event_loop_cb, event: %d", event);

	switch (event) {
	case MDF_EVENT_MWIFI_STARTED:
		MDF_LOGW("MESH is started");
		break;

	case MDF_EVENT_MWIFI_PARENT_CONNECTED:
		MDF_LOGW("Parent is connected on station interface");
		break;

	case MDF_EVENT_MWIFI_PARENT_DISCONNECTED:
		MDF_LOGW("Parent is disconnected on station interface");
		break;

	default:
		break;
	}

	return MDF_OK;
}

void app_main()
{
	//network_status_led();

	mwifi_init_config_t cfg = MWIFI_INIT_CONFIG_DEFAULT();
	mwifi_config_t config   = {
		.channel   = CONFIG_MESH_CHANNEL,
		.mesh_id   = CONFIG_MESH_ID,
		.mesh_type = CONFIG_DEVICE_TYPE,
	};

	/**
	 * @brief Set the log level for serial port printing.
	 */
	esp_log_level_set("*", ESP_LOG_INFO);
	esp_log_level_set(TAG, ESP_LOG_DEBUG);

	/**
	 * @brief Initialize wifi mesh.
	 */
	MDF_ERROR_ASSERT(mdf_event_loop_init(event_loop_cb));
	MDF_ERROR_ASSERT(wifi_init());
	MDF_ERROR_ASSERT(mwifi_init(&cfg));
	MDF_ERROR_ASSERT(mwifi_set_config(&config));
	MDF_ERROR_ASSERT(mwifi_start());

	/**
	 * @brief Data transfer between wifi mesh devices
	 */
	if (config.mesh_type == MESH_ROOT) {
		xTaskCreate(root_task, "root_task", 4 * 1024,
					NULL, CONFIG_MDF_TASK_DEFAULT_PRIOTY, NULL);

		xTaskCreate(ssd1306_display_task, "ssd1306_display_task", 8 * 1024,
					NULL, CONFIG_MDF_TASK_DEFAULT_PRIOTY, NULL);

	} else {
		/* Join info group for receiving list of available nodes. */
		MDF_ERROR_ASSERT(esp_mesh_set_group_id(&node_info_broadcast_group, 1));

		xTaskCreate(node_write_task, "node_write_task", 4 * 1024,
					NULL, CONFIG_MDF_TASK_DEFAULT_PRIOTY, NULL);
		xTaskCreate(node_read_task, "node_read_task", 4 * 1024,
					NULL, CONFIG_MDF_TASK_DEFAULT_PRIOTY, NULL);
	}

	TimerHandle_t timer = xTimerCreate("print_system_info", 10000 / portTICK_RATE_MS,
									   true, NULL, print_system_info_timercb);
	xTimerStart(timer, 0);
}
