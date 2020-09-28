idf_component_register(
	SRCS "bt_discovery.c"
	INCLUDE_DIRS "."
	REQUIRES bt nvs_flash html_pages web-wifi
)
