/*
  Fichero: ./components/diymon_bsp/WS1.9TS/bsp.c
  Fecha: 12/08/2025 - 05:25 pm
  Último cambio: Restaurado el orden de inicialización original para corregir fallo de montaje de SD.
  Descripción: Orquestador del BSP. Se revierte el orden de inicialización para que el display se configure antes que la tarjeta SD, resolviendo un conflicto en el bus SPI compartido que impedía el montaje de la SD.
*/
#include "bsp_api.h"
#include "esp_err.h"
#include "esp_log.h" 
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "bsp";

// Inicialización completa para la aplicación principal
esp_err_t bsp_init(void) {
    ESP_LOGI(TAG, "Inicializando TODO el hardware para la aplicación principal...");
    ESP_ERROR_CHECK(bsp_i2c_init());
    ESP_ERROR_CHECK(bsp_spi_init());
    ESP_ERROR_CHECK(bsp_display_init());
    ESP_ERROR_CHECK(bsp_touch_init());
    ESP_ERROR_CHECK(bsp_imu_init());
    ESP_ERROR_CHECK(bsp_sdcard_init());
    return ESP_OK;
}

// Inicialización para modos de servicio que necesitan mostrar una imagen
esp_err_t bsp_init_service_mode(void) {
    ESP_LOGI(TAG, "Inicializando hardware para modo de servicio con pantalla...");
    ESP_ERROR_CHECK(bsp_spi_init());
    ESP_ERROR_CHECK(bsp_sdcard_init());
    ESP_ERROR_CHECK(bsp_display_init());
    bsp_display_set_brightness(100);
    return ESP_OK;
}

// Inicialización mínima para modos de servicio que no usan pantalla
esp_err_t bsp_init_minimal_headless(void) {
    ESP_LOGI(TAG, "Inicializando hardware MÍNIMO para modo headless (SPI + SD)...");
    ESP_ERROR_CHECK(bsp_spi_init());
    ESP_ERROR_CHECK(bsp_sdcard_init());
    return ESP_OK;
}
