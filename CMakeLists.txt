# The following lines of boilerplate have to be in your project's CMakeLists
# in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

set(EXTRA_COMPONENT_DIRS $ENV{PWD}/components/html_pages)
set(EXTRA_COMPONENT_DIRS $ENV{PWD}/components/lamp)
set(EXTRA_COMPONENT_DIRS $ENV{PWD}/components/manual-control)
set(EXTRA_COMPONENT_DIRS $ENV{PWD}/components/power-sensor)
set(EXTRA_COMPONENT_DIRS $ENV{PWD}/components/bluetooth)
set(EXTRA_COMPONENT_DIRS $ENV{PWD}/components/web-wifi)
set(EXTRA_COMPONENT_DIRS $ENV{IDF_PATH}/components/mbedtls)
set(EXTRA_COMPONENT_DIRS $ENV{IDF_PATH}/components/nvs_flash)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(simple-web-server)
