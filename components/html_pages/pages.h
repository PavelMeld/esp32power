#ifndef __HTTP_PAGES
#define __HTTP_PAGES
#include <esp_http_server.h>
#include "pages.h"

void pages_register(httpd_handle_t server);
time_t	get_last_web_action_time();
void update_last_web_action_time();

#endif
