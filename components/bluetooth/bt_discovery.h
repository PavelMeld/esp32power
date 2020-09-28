#ifndef __BT_DISCOVERY
#define __BT_DISCOVERY

#include "freertos/list.h"

typedef struct device_info_s {
	char	* mac_str;
	char	* name;
} device_info_t;

void		bt_init();
List_t	*	bt_device_list();


#endif
