#include <esp_log.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "lamp.h"

#define NO_ISR_FLAGS			0
#define POWER_CHECK_INTERVAL	50

static uint32_t		evt_sensor_triggered = 1;
static xQueueHandle	sensor_event_queue;
static char * TAG = "POWER";


static void IRAM_ATTR power_sensor_isr_handler(void* arg)
{
	gpio_isr_handler_remove(CONFIG_POWER_PIN);
    xQueueSendFromISR(sensor_event_queue, &evt_sensor_triggered, NULL);
}


static void power_setup_isr() {
    gpio_install_isr_service(NO_ISR_FLAGS);
	gpio_pad_select_gpio(CONFIG_POWER_PIN);
	gpio_set_intr_type(CONFIG_POWER_PIN, GPIO_INTR_ANYEDGE);
}


static void power_sensor_task(void *arg)
{

	sensor_event_queue = xQueueCreate(10, sizeof(uint32_t));

	power_setup_isr();

    while (1) {
		uint32_t	evt;

		gpio_isr_handler_add(CONFIG_POWER_PIN, power_sensor_isr_handler, NULL);
        BaseType_t ret = xQueueReceive(sensor_event_queue, &evt, pdMS_TO_TICKS(POWER_CHECK_INTERVAL));

		if (ret) {
			//ESP_LOGI(TAG, "Power detected");
			vTaskDelay(pdMS_TO_TICKS(POWER_CHECK_INTERVAL));
			continue;
		}

		ESP_LOGI(TAG, "Power GAP");

		lamp_next_visible_mode();
		// Power GAP --> waiting while power resumed
		gpio_isr_handler_add(CONFIG_POWER_PIN, power_sensor_isr_handler, NULL);
        xQueueReceive(sensor_event_queue, &evt, portMAX_DELAY);

		lamp_save_mode();

		ESP_LOGI(TAG, "Power Resumed");
	}
}


void power_init() {
	xTaskCreate(power_sensor_task, "Power sensor", 2048, NULL, tskIDLE_PRIORITY, NULL);
}
