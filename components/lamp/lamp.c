#include <stdio.h>
#include <esp_log.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_system.h"
#include "driver/gpio.h"
#include "driver/adc.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "lamp.h"


#define LAMP_MAX_EVENTS	10

#define GPIO_LAMP_ON	0
#define GPIO_LAMP_OFF	1


static volatile int32_t	lamp_mode;
static QueueHandle_t	lamp_event_queue;

struct lamp_event_s {
	struct lamp_event_command_s {
		unsigned int		save:1;
		unsigned int		set_mode:1;
	} command;
	enum lamp_mode_e	mode;
};

static void lamp_set_indicator(unsigned char onoff) {
	gpio_set_level(CONFIG_INDICATOR_PIN, onoff);
}

static void lamp_setup_pio() {
	ESP_LOGI("LAMP", "GPIO controls : %i, %i", CONFIG_CH1_RELAY, CONFIG_CH2_RELAY);

	
	gpio_pad_select_gpio(CONFIG_CH1_RELAY);
	gpio_pad_select_gpio(CONFIG_CH2_RELAY);
	gpio_pad_select_gpio(CONFIG_INDICATOR_PIN);


	gpio_set_level(CONFIG_CH1_RELAY, GPIO_LAMP_OFF);
	gpio_set_level(CONFIG_CH2_RELAY, GPIO_LAMP_OFF);


	gpio_set_direction(CONFIG_CH1_RELAY, GPIO_MODE_OUTPUT);
	gpio_set_direction(CONFIG_CH2_RELAY, GPIO_MODE_OUTPUT);
	gpio_set_direction(CONFIG_INDICATOR_PIN, GPIO_MODE_OUTPUT);

	// ON-board LEDs
	//gpio_pad_select_gpio(18);
	//gpio_pad_select_gpio(19);
	//gpio_set_direction(18, GPIO_MODE_OUTPUT);
	//gpio_set_direction(19, GPIO_MODE_OUTPUT);
	//gpio_set_level(18, 1);
	//gpio_set_level(19, 1);
}


static void lamp_set_relays(unsigned char mode) {
	unsigned char	lamp1, lamp2;

	lamp1 = lamp2 = GPIO_LAMP_OFF;

	if (mode == LAMP_MODE_R1 || mode == LAMP_MODE_R12)
		lamp1 = GPIO_LAMP_ON;

	if (mode == LAMP_MODE_R2 || mode == LAMP_MODE_R12)
		lamp2 = GPIO_LAMP_ON;

	gpio_set_level(CONFIG_CH1_RELAY, lamp1);
	gpio_set_level(CONFIG_CH2_RELAY, lamp2);

	printf("Lamp Mode %i\n", mode);
}


static void lamp_setup_mode() {
    nvs_handle_t	handle;
	int32_t			mode;

	nvs_flash_init();
	nvs_open("storage", NVS_READONLY, &handle);
	nvs_get_i32(handle, "lamp_mode", &mode);
	nvs_close(handle);

	lamp_mode = mode % LAMP_MODES_COUNT;

#ifdef  CONFIG_KITCHEN_MODE
	if (lamp_mode < LAMP_MODE_R1 || lamp_mode > LAMP_MODE_R12)
		lamp_mode = LAMP_MODE_R1;
#endif


	lamp_set_relays(lamp_mode);
}


void lamp_save_mode() {
    nvs_handle_t	handle;
	esp_err_t		err;

	err = nvs_open("storage", NVS_READWRITE, &handle);
	if (err != ESP_OK) {
        ESP_LOGI("LAMP_SAVE", "Open error %i", err);
		return;
	}
	err = nvs_set_i32(handle, "lamp_mode", lamp_mode);
	if (err != ESP_OK) {
        ESP_LOGI("LAMP_SAVE", "Set error %i", err);
		return;
	}
	nvs_commit(handle);
	if (err != ESP_OK) {
        ESP_LOGI("LAMP_SAVE", "Commit error %i", err);
		return;
	}
	nvs_close(handle);

}



static void lamp_flash_thread(void * args) {
	
	while (1) {
		int8_t	divider;

		switch (lamp_mode) {
			case LAMP_MODE_R1:
				divider = 2;
				break;
			case LAMP_MODE_R2:
				divider = 3;
				break;
			case LAMP_MODE_R12:
				divider = 5;
				break;
			default:
				divider = 0;
		}

		lamp_set_indicator(0);

		if (divider) {
			int delay_interval = 600 / divider;

			for (int n=0; n<divider; n++) {
				int onoff = !(n % 2);
				lamp_set_indicator(onoff);
				vTaskDelay(pdMS_TO_TICKS(delay_interval));
			}
		}

		lamp_set_indicator(0);

		vTaskDelay(pdMS_TO_TICKS(500));
	}
}


static void lamp_control_thread(void * args) {
	lamp_event_queue = xQueueCreate(LAMP_MAX_EVENTS, sizeof(struct lamp_event_s));
	ESP_LOGI("LAMP_EVENT", "Created queue %p", lamp_event_queue);
	
	while (1) {
		struct lamp_event_s	event;

		xQueueReceive(lamp_event_queue, &event, portMAX_DELAY);

		if (event.command.set_mode) {
			ESP_LOGI("LAMP_EVENT", "New mode %i", event.mode);
			lamp_set_relays(event.mode);
			lamp_mode = event.mode;
		}

		if (event.command.save) {
			ESP_LOGI("LAMP_EVENT", "Save event");
			lamp_save_mode();
		}

	}
}


////////////////////////////////////////////////////////////////////////////////
//
//
//	API: Get lamp state
//
//
////////////////////////////////////////////////////////////////////////////////
enum lamp_mode_e lamp_get_mode() {
	return lamp_mode;
}


////////////////////////////////////////////////////////////////////////////////
//
//
//	API: Set lamp state
//
//
////////////////////////////////////////////////////////////////////////////////
void lamp_set_mode(enum lamp_mode_e	mode) {
	struct lamp_event_s	event = { 
		.command = {
			.save = 1,	
			.set_mode = 1
		},
		.mode = mode 
	};

	ESP_LOGI("LAMP_EVENT", "Sending %i to queue %p", mode, lamp_event_queue);
	xQueueSend(lamp_event_queue, &event, 0);
}

////////////////////////////////////////////////////////////////////////////////
//
//
//	API: Set next mode
//
//
////////////////////////////////////////////////////////////////////////////////
void lamp_next_visible_mode() {
	enum lamp_mode_e	next_mode = (lamp_mode + 1) % (LAMP_MODE_R12 + 1);

	struct lamp_event_s	event = { 
		.command = {
			.save = 0,	
			.set_mode = 1
		},
		.mode = next_mode 
	};

	ESP_LOGI("LAMP_EVENT", "Sending %i to queue %p", next_mode, lamp_event_queue);
	xQueueSend(lamp_event_queue, &event, 0);
}


////////////////////////////////////////////////////////////////////////////////
//
//
//	API: Lamp init
//
//
////////////////////////////////////////////////////////////////////////////////
void lamp_init() {
	lamp_setup_pio();
	lamp_setup_mode();

	xTaskCreate(lamp_control_thread, "Lamp events", 5 * 1024, NULL, tskIDLE_PRIORITY, NULL);
	xTaskCreate(lamp_flash_thread, "Lamp flashing", 5 * 1024, NULL, tskIDLE_PRIORITY, NULL);
}

