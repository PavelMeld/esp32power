#include <esp_log.h>
#include <sys/param.h>
#include "pages.h"
#include "lamp.h"
#include <time.h>

static const char *TAG = "PAGES";

esp_err_t serve_page_webs_INDEX_CSP(httpd_req_t *  request);
esp_err_t serve_page_webs_STATUS_CSP(httpd_req_t *  request);
static time_t last_action_time = 0;


time_t	get_last_web_action_time() {
	return last_action_time;
}


/*
	Called once index page is loaded or user manually selected lamp mode
*/
void update_last_web_action_time() {
	last_action_time = time(NULL);
}

/*
 *
 */
static esp_err_t set_status(httpd_req_t *  request) {
    char buf[100];
    int ret, remaining = request->content_len;
	int buf_free = sizeof(buf)-1;

	update_last_web_action_time();


    while (remaining > 0 && buf_free) {
        /* Read the data for the request */
        if ((ret = httpd_req_recv(request, buf + sizeof(buf) - buf_free - 1,
                        MIN(remaining, buf_free))) <= 0) {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
                /* Retry receiving if timeout occurred */
                continue;
            }
            return ESP_FAIL;
        }

        remaining -= ret;
		buf_free -= ret;

        /* Log data received */
        ESP_LOGI(TAG, "%.*s", ret, buf);
    }

	buf[sizeof(buf) - buf_free - 1]  = 0;

	char * token, *saveptr;

	token = strtok_r(buf, "&", &saveptr);

	while (token) {
		char * lval;
		char * rval;
		char part[100];
		char * lptrsave;

		ESP_LOGI(TAG, "Processing %s", token);

		strcpy(part, token);
		lval = strtok_r(part, "=", &lptrsave);
		rval = strtok_r(NULL, "=", &lptrsave);
		ESP_LOGI(TAG, "%s<-->%s", lval, rval);

		token = strtok_r(NULL, "&", &saveptr);

		// Set lamp mode
		if (!strcmp(lval, "l")) {
			char	*err;
			int		rval_int = strtol(rval, &err, 10);
			if (*err == 0 && rval_int < LAMP_MODES_COUNT) {
				ESP_LOGI(TAG, "Setting lamp mode %i", rval_int);
				lamp_set_mode((enum lamp_mode_e) rval_int);
			} else {
				if (*err) {
					ESP_LOGI(TAG, "Error mode syntax %s", err);
				}
				if (rval_int >= LAMP_MODES_COUNT) {
					ESP_LOGI(TAG, "Error mode, too big %i", rval_int);
				}
			}
		}
	}

	httpd_resp_sendstr(request, "OK");
	
	return ESP_OK;
}


static const httpd_uri_t  index_page = {
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = serve_page_webs_INDEX_CSP,
    .user_ctx  = NULL
};

static const httpd_uri_t  set_lamp_api = {
    .uri       = "/api/set",
    .method    = HTTP_POST,
    .handler   = set_status,
    .user_ctx  = NULL
};

static const httpd_uri_t  status_api = {
    .uri       = "/api/status",
    .method    = HTTP_GET,
    .handler   = serve_page_webs_STATUS_CSP,
    .user_ctx  = NULL
};


void pages_register(httpd_handle_t server) {
	httpd_register_uri_handler(server, &index_page);
	httpd_register_uri_handler(server, &status_api);
	httpd_register_uri_handler(server, &set_lamp_api);
}


#if 0

    char*  buf;
    size_t buf_len;

    /* Get header value string length and allocate memory for length + 1,
     * extra byte for null termination */
    buf_len = httpd_req_get_hdr_value_len(req, "Host") + 1;
    if (buf_len > 1) {
        buf = malloc(buf_len);
        /* Copy null terminated value string into buffer */
        if (httpd_req_get_hdr_value_str(req, "Host", buf, buf_len) == ESP_OK) {
            ESP_LOGI(TAG, "Found header => Host: %s", buf);
        }
        free(buf);
    }

    buf_len = httpd_req_get_hdr_value_len(req, "Test-Header-2") + 1;
    if (buf_len > 1) {
        buf = malloc(buf_len);
        if (httpd_req_get_hdr_value_str(req, "Test-Header-2", buf, buf_len) == ESP_OK) {
            ESP_LOGI(TAG, "Found header => Test-Header-2: %s", buf);
        }
        free(buf);
    }

    buf_len = httpd_req_get_hdr_value_len(req, "Test-Header-1") + 1;
    if (buf_len > 1) {
        buf = malloc(buf_len);
        if (httpd_req_get_hdr_value_str(req, "Test-Header-1", buf, buf_len) == ESP_OK) {
            ESP_LOGI(TAG, "Found header => Test-Header-1: %s", buf);
        }
        free(buf);
    }

    /* Read URL query string length and allocate memory for length + 1,
     * extra byte for null termination */
    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1) {
        buf = malloc(buf_len);
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
            ESP_LOGI(TAG, "Found URL query => %s", buf);
            char param[32];
            /* Get value of expected key from query string */
            if (httpd_query_key_value(buf, "query1", param, sizeof(param)) == ESP_OK) {
                ESP_LOGI(TAG, "Found URL query parameter => query1=%s", param);
            }
            if (httpd_query_key_value(buf, "query3", param, sizeof(param)) == ESP_OK) {
                ESP_LOGI(TAG, "Found URL query parameter => query3=%s", param);
            }
            if (httpd_query_key_value(buf, "query2", param, sizeof(param)) == ESP_OK) {
                ESP_LOGI(TAG, "Found URL query parameter => query2=%s", param);
            }
        }
        free(buf);
    }

    /* Set some custom headers */
    httpd_resp_set_hdr(req, "Custom-Header-1", "Custom-Value-1");
    httpd_resp_set_hdr(req, "Custom-Header-2", "Custom-Value-2");

    /* Send response with custom headers and body set as the
     * string passed in user context*/
    const char* resp_str = (const char*) req->user_ctx;
    httpd_resp_send(req, resp_str, strlen(resp_str));

    /* After sending the HTTP response the old HTTP request
     * headers are lost. Check if HTTP request headers can be read now. */
    if (httpd_req_get_hdr_value_len(req, "Host") == 0) {
        ESP_LOGI(TAG, "Request headers lost");
    }
    return ESP_OK;
#endif
