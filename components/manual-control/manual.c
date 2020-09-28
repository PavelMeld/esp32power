#include <esp_log.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "lamp.h"

#define NO_ISR_FLAGS			0
#define DEBOUNCE_TIMEOUT		100

static uint32_t		evt_manual_triggered = 1;
static xQueueHandle	manual_event_queue;
static char * TAG = "MANUAL";

static enum lamp_mode_e	on_value = 
#ifdef CONFIG_GARAGE_LAMP1
	LAMP_MODE_R1
#endif
#ifdef CONFIG_GARAGE_LAMP2
	LAMP_MODE_R2
#endif
#ifdef CONFIG_GARAGE_BOTH_LAMPS
	LAMP_MODE_R12
#endif;
;


static void IRAM_ATTR flip_flop_isr_handler(void* arg) {
	gpio_isr_handler_remove(CONFIG_MANUAL_CONTROL_PIN);
    xQueueSendFromISR(manual_event_queue, &evt_manual_triggered, NULL);
}


static void manual_setup_isr() {
    gpio_install_isr_service(NO_ISR_FLAGS);
	gpio_pad_select_gpio(CONFIG_MANUAL_CONTROL_PIN);
	gpio_set_intr_type(CONFIG_MANUAL_CONTROL_PIN, GPIO_INTR_ANYEDGE);
	gpio_set_direction(CONFIG_MANUAL_CONTROL_PIN, GPIO_MODE_INPUT);
}


static void manual_control_task(void *arg)
{

	manual_event_queue = xQueueCreate(10, sizeof(uint32_t));

	manual_setup_isr();

    while (1) {
		uint32_t	evt;

		gpio_isr_handler_add(CONFIG_MANUAL_CONTROL_PIN, flip_flop_isr_handler, NULL);
        xQueueReceive(manual_event_queue, &evt, portMAX_DELAY);

		vTaskDelay(pdMS_TO_TICKS(DEBOUNCE_TIMEOUT));


		ESP_LOGI(TAG, "Manual switch event, level = %i", gpio_get_level(CONFIG_MANUAL_CONTROL_PIN));

		if (gpio_get_level(CONFIG_MANUAL_CONTROL_PIN))
			lamp_set_mode(on_value);
		else
			lamp_set_mode(LAMP_MODE_OFF);
	}
}


void manual_init() {
	xTaskCreate(manual_control_task, "Manual control", 2048, NULL, tskIDLE_PRIORITY, NULL);
}
