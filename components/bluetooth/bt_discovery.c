/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

/*
listSET_LIST_ITEM_OWNER
listGET_LIST_ITEM_OWNER
*/



/****************************************************************************
*
* This file is for Classic Bluetooth device and service discovery Demo.
*
****************************************************************************/

#include <stdint.h>
#include <string.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "esp_gap_bt_api.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "freertos/queue.h"
#include "freertos/list.h"
#include "pages.h"
#include "wifi.h"

#include "bt_discovery.h"

#define GAP_TAG          		"GAP"
#define MAX_WEB_INACTIVITY		10
#define DISCOVERY_INTERVAL		30

static List_t	discovered_devices;


typedef enum {
    APP_GAP_STATE_IDLE = 0,
    APP_GAP_STATE_DEVICE_DISCOVERING,
    APP_GAP_STATE_DEVICE_DISCOVER_COMPLETE,
    APP_GAP_STATE_SERVICE_DISCOVERING,
    APP_GAP_STATE_SERVICE_DISCOVER_COMPLETE,
} app_gap_state_t;

typedef struct {
    bool dev_found;
    uint8_t bdname_len;
    uint8_t eir_len;
    uint8_t rssi;
    uint32_t cod;
    uint8_t eir[ESP_BT_GAP_EIR_DATA_LEN];
    uint8_t bdname[ESP_BT_GAP_MAX_BDNAME_LEN + 1];
    esp_bd_addr_t bda;
    app_gap_state_t state;
} app_gap_cb_t;

static app_gap_cb_t		m_dev_info;
static QueueHandle_t	discovery_event_queue;
static int				discovery_started = 0;


void clear_discovered_devices() {

	while (listCURRENT_LIST_LENGTH(&discovered_devices)) {
		ListItem_t		* head = listGET_HEAD_ENTRY(&discovered_devices);
		device_info_t	* device_info = listGET_LIST_ITEM_OWNER(head);

		free(device_info->mac_str);
		free(device_info->name);
		free(device_info);

		uxListRemove(head);
		free(head);
	}

	vListInitialise(&discovered_devices);
}


ListItem_t * discovered_search(char * mac) {
    ListItem_t      * entry;
    int length = listCURRENT_LIST_LENGTH(&discovered_devices);

	ESP_LOGI(GAP_TAG, "Current device list size = %X", length);

	if (!length) {
        return NULL;
    }

    entry = listGET_HEAD_ENTRY(&discovered_devices);

    for (int n = 0; n < length && entry; n++) {
        device_info_t	* device_info = listGET_LIST_ITEM_OWNER(entry);
        
        if (!memcmp(device_info->mac_str, mac, 6))
            return entry;

        entry = listGET_NEXT(entry);
    }

    return NULL;
}


void add_discovered_device(char * mac, uint8_t * name) {
	ListItem_t		* item;
	device_info_t	* device_info;

    item = discovered_search(mac);

	ESP_LOGI(GAP_TAG, "Looking in Previous devices = %p\n", item);

    if (item) {
        device_info = listGET_LIST_ITEM_OWNER(item);
        if (device_info->name == NULL && strlen((char *)name)) {
            ESP_LOGI(GAP_TAG, "Added device name %x:%x:%x:%x:%x:%x %s\n", 
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], (char *)name);
            device_info->name = strdup((char*)name);
        }
        return;
    }

    item = calloc(1, sizeof(*item));
    device_info = calloc(1, sizeof(*device_info));

	device_info->mac_str = strdup(mac);
    device_info->name = strlen((char*)name) ? strdup((char*)name) : NULL;
    ESP_LOGI(GAP_TAG, "Added new device %x:%x:%x:%x:%x:%x %s\n", 
        mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], (char *)name);

	vListInitialiseItem(item);
	listSET_LIST_ITEM_OWNER(item, device_info);
	vListInsert(&discovered_devices, item);
}


void start_discovery() {
    /* inititialize device information and status */
    app_gap_cb_t *p_dev = &m_dev_info;
    memset(p_dev, 0, sizeof(app_gap_cb_t));

    /* start to discover nearby Bluetooth devices */
	discovery_started = 1;
    p_dev->state = APP_GAP_STATE_DEVICE_DISCOVERING;
    esp_bt_gap_start_discovery(ESP_BT_INQ_MODE_GENERAL_INQUIRY, 30, 0);
}


static char *bda2str(esp_bd_addr_t bda, char *str, size_t size)
{
    if (bda == NULL || str == NULL || size < 18) {
        return NULL;
    }

    uint8_t *p = bda;
    sprintf(str, "%02x:%02x:%02x:%02x:%02x:%02x",
            p[0], p[1], p[2], p[3], p[4], p[5]);
    return str;
}

static char *uuid2str(esp_bt_uuid_t *uuid, char *str, size_t size)
{
    if (uuid == NULL || str == NULL) {
        return NULL;
    }

    if (uuid->len == 2 && size >= 5) {
        sprintf(str, "%04x", uuid->uuid.uuid16);
    } else if (uuid->len == 4 && size >= 9) {
        sprintf(str, "%08x", uuid->uuid.uuid32);
    } else if (uuid->len == 16 && size >= 37) {
        uint8_t *p = uuid->uuid.uuid128;
        sprintf(str, "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
                p[15], p[14], p[13], p[12], p[11], p[10], p[9], p[8],
                p[7], p[6], p[5], p[4], p[3], p[2], p[1], p[0]);
    } else {
        return NULL;
    }

    return str;
}

static bool get_name_from_eir(uint8_t *eir, uint8_t *bdname, uint8_t *bdname_len)
{
    uint8_t *rmt_bdname = NULL;
    uint8_t rmt_bdname_len = 0;

    if (!eir) {
        return false;
    }

    rmt_bdname = esp_bt_gap_resolve_eir_data(eir, ESP_BT_EIR_TYPE_CMPL_LOCAL_NAME, &rmt_bdname_len);
    if (!rmt_bdname) {
        rmt_bdname = esp_bt_gap_resolve_eir_data(eir, ESP_BT_EIR_TYPE_SHORT_LOCAL_NAME, &rmt_bdname_len);
    }

    if (rmt_bdname) {
        if (rmt_bdname_len > ESP_BT_GAP_MAX_BDNAME_LEN) {
            rmt_bdname_len = ESP_BT_GAP_MAX_BDNAME_LEN;
        }

        if (bdname) {
            memcpy(bdname, rmt_bdname, rmt_bdname_len);
            bdname[rmt_bdname_len] = '\0';
        }
        if (bdname_len) {
            *bdname_len = rmt_bdname_len;
        }
        return true;
    }

    return false;
}

static void update_device_info(esp_bt_gap_cb_param_t *param)
{
    char bda_str[18];
    uint32_t cod = 0;
    int32_t rssi = -129; /* invalid value */
    esp_bt_gap_dev_prop_t *p;
    uint8_t bdname[ESP_BT_GAP_MAX_BDNAME_LEN + 1];
	uint8_t bdname_len = 0;
    uint8_t eir[ESP_BT_GAP_EIR_DATA_LEN];
    uint8_t eir_len = 0;

    ESP_LOGI(GAP_TAG, "Device found: %s", bda2str(param->disc_res.bda, bda_str, sizeof(bda_str)));
    for (int i = 0; i < param->disc_res.num_prop; i++) {
        p = param->disc_res.prop + i;
        switch (p->type) {
        case ESP_BT_GAP_DEV_PROP_COD:
            cod = *(uint32_t *)(p->val);
            break;
        case ESP_BT_GAP_DEV_PROP_RSSI:
            rssi = *(int8_t *)(p->val);
            break;
        case ESP_BT_GAP_DEV_PROP_BDNAME: {
            uint8_t len = (p->len > ESP_BT_GAP_MAX_BDNAME_LEN) ? ESP_BT_GAP_MAX_BDNAME_LEN :
                          (uint8_t)p->len;
            memcpy(bdname, (uint8_t *)(p->val), len);
            bdname[len] = '\0';
            bdname_len = len;
			break;
		}
        case ESP_BT_GAP_DEV_PROP_EIR:
            memcpy(eir, (uint8_t *)(p->val), p->len);
            eir_len = p->len;
            break;
        default:
            break;
        }
    }


    if (eir_len && !bdname_len) {
        if (!get_name_from_eir(eir, bdname, &bdname_len))
			strcpy((char *)bdname, "<undefined>");
    }

	ESP_LOGI(GAP_TAG, "Adding : address %s, name %s", bda_str, bdname);
	add_discovered_device(bda_str, bdname);
}

void bt_app_gap_init(void)
{
}

void bt_app_gap_cb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param)
{
    app_gap_cb_t *p_dev = &m_dev_info;
    char bda_str[18];
    char uuid_str[37];

    switch (event) {
    case ESP_BT_GAP_DISC_RES_EVT: {
        update_device_info(param);
        break;
    }
    case ESP_BT_GAP_DISC_STATE_CHANGED_EVT: {
        if (param->disc_st_chg.state == ESP_BT_GAP_DISCOVERY_STOPPED && discovery_started) {
			uint32_t	event = 1;
			discovery_started = 0;
            ESP_LOGI(GAP_TAG, "Device discovery stopped.");
            esp_bt_gap_cancel_discovery();
            esp_bt_controller_mem_release(ESP_BT_MODE_BTDM);
			xQueueSend(discovery_event_queue, &event, 0);
        } else if (param->disc_st_chg.state == ESP_BT_GAP_DISCOVERY_STARTED) {
            ESP_LOGI(GAP_TAG, "Discovery started.");
        }
        break;
    }
    case ESP_BT_GAP_RMT_SRVCS_EVT: {
        if (memcmp(param->rmt_srvcs.bda, p_dev->bda, ESP_BD_ADDR_LEN) == 0 &&
                p_dev->state == APP_GAP_STATE_SERVICE_DISCOVERING) {
            p_dev->state = APP_GAP_STATE_SERVICE_DISCOVER_COMPLETE;
            if (param->rmt_srvcs.stat == ESP_BT_STATUS_SUCCESS) {
                ESP_LOGI(GAP_TAG, "Services for device %s found",  bda2str(p_dev->bda, bda_str, 18));
                for (int i = 0; i < param->rmt_srvcs.num_uuids; i++) {
                    esp_bt_uuid_t *u = param->rmt_srvcs.uuid_list + i;
                    ESP_LOGI(GAP_TAG, "--%s", uuid2str(u, uuid_str, 37));
                    // ESP_LOGI(GAP_TAG, "--%d", u->len);
                }
            } else {
                ESP_LOGI(GAP_TAG, "Services for device %s not found",  bda2str(p_dev->bda, bda_str, 18));
            }
        }
        break;
    }
    case ESP_BT_GAP_RMT_SRVC_REC_EVT:
    default: {
        ESP_LOGI(GAP_TAG, "event: %d", event);
        break;
    }
    }
    return;
}



void bt_discovery_task()
{
    /* Initialize NVS — it is used to store PHY calibration data */
    esp_err_t ret;
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK( ret );

    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_BLE));

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    if ((ret = esp_bt_controller_init(&bt_cfg)) != ESP_OK) {
        ESP_LOGE(GAP_TAG, "%s initialize controller failed: %s\n", __func__, esp_err_to_name(ret));
        return;
    }

    if ((ret = esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT)) != ESP_OK) {
        ESP_LOGE(GAP_TAG, "%s enable controller failed: %s\n", __func__, esp_err_to_name(ret));
        return;
    }

    if ((ret = esp_bluedroid_init()) != ESP_OK) {
        ESP_LOGE(GAP_TAG, "%s initialize bluedroid failed: %s\n", __func__, esp_err_to_name(ret));
        return;
    }

    if ((ret = esp_bluedroid_enable()) != ESP_OK) {
        ESP_LOGE(GAP_TAG, "%s enable bluedroid failed: %s\n", __func__, esp_err_to_name(ret));
        return;
    }

    char *dev_name = "ESP_GAP_INQRUIY";
    esp_bt_dev_set_device_name(dev_name);

    /* set discoverable and connectable mode, wait to be connected */
    esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);

    /* register GAP callback function */
    esp_bt_gap_register_callback(bt_app_gap_cb);

	// Discovery loop
    clear_discovered_devices();
	discovery_event_queue = xQueueCreate(10, sizeof(uint32_t));
	while (1) {
		uint32_t	evt;
		time_t		cur_time = time(NULL);
		time_t		last_web_event = get_last_web_action_time();
		time_t		last_wifi_event = get_last_wifi_join_time();
		time_t		last_event_time = last_web_event < last_wifi_event ? last_wifi_event : last_web_event;

		if (cur_time < last_event_time  + MAX_WEB_INACTIVITY) {
			time_t	sleep_time= last_event_time + MAX_WEB_INACTIVITY - cur_time;

			ESP_LOGI(GAP_TAG, "Sleeping %lu seconds", sleep_time);
			vTaskDelay(pdMS_TO_TICKS(sleep_time * 1000));
			continue;
		}

        if (0) {
            clear_discovered_devices();
        }

		ESP_LOGI(GAP_TAG, "Starting discovery");

		start_discovery();
        BaseType_t ret = xQueueReceive(discovery_event_queue , &evt, portMAX_DELAY);
		vTaskDelay(pdMS_TO_TICKS(DISCOVERY_INTERVAL * 1000));
	}
}


List_t * bt_device_list() {
	return &discovered_devices;
}


void bt_init() {
	xTaskCreate(bt_discovery_task, "Bluetooth poller", 2048, NULL, tskIDLE_PRIORITY, NULL);
}
