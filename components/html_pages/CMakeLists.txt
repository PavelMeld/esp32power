execute_process(
	COMMAND ./csp index.csp 
	OUTPUT_FILE index.c
	WORKING_DIRECTORY ${COMPONENT_DIR})
execute_process(
	COMMAND ./csp status.csp 
	OUTPUT_FILE status.c
	WORKING_DIRECTORY ${COMPONENT_DIR})
idf_component_register(
	SRCS "pages.c" "index.c" "status.c"
	INCLUDE_DIRS "." 
	REQUIRES esp_http_server lamp bluetooth)
