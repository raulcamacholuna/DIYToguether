/*
  Fichero: Z:/DIYMON/VSCODE/DIYToguether/components/diymon_bsp/WS1.9TS/bsp_sdcard.c
  Fecha: 12/08/2025 - 05:45 pm
  Último cambio: Añadido reset manual de la línea CS de la SD para mejorar la fiabilidad.
  Descripción: Se ha añadido un pequeño retardo y un ciclo de la línea Chip Select (CS) de la tarjeta SD antes de intentar el montaje. Esto ayuda a resetear el estado interno de la tarjeta, solucionando problemas de inicialización cuando el bus SPI es compartido.
*/
#include "bsp_api.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "bsp_sdcard.h" 

static const char *TAG = "bsp_sdcard";

// --- CONFIGURACIÓN PRIVADA ---
#define PIN_NUM_MISO  (GPIO_NUM_19)
#define PIN_NUM_MOSI  (GPIO_NUM_4)
#define PIN_NUM_CLK   (GPIO_NUM_5)
#define PIN_NUM_CS    (GPIO_NUM_20)
#define MOUNT_POINT   "/sdcard"

static sdmmc_card_t *g_card = NULL;
static sdmmc_host_t g_host = SDSPI_HOST_DEFAULT();

// --- IMPLEMENTACIÓN DE LA FUNCIÓN PÚBLICA ---
esp_err_t bsp_sdcard_init(void)
{
    ESP_LOGI(TAG, "Initializing SD card...");
    esp_err_t ret;

    // Workaround: Resetear la tarjeta SD manualmente con la línea CS
    // Esto es útil en buses compartidos donde la tarjeta puede estar en un estado desconocido.
    gpio_set_direction(PIN_NUM_CS, GPIO_MODE_OUTPUT);
    gpio_set_level(PIN_NUM_CS, 1);
    vTaskDelay(pdMS_TO_TICKS(100)); // Pequeña espera
    gpio_set_level(PIN_NUM_CS, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(PIN_NUM_CS, 1);
    vTaskDelay(pdMS_TO_TICKS(100));
    ESP_LOGW(TAG, "Línea CS de la tarjeta SD reseteada manualmente.");


    // Configura el sistema de ficheros FAT, habilitando el formateo si el montaje falla.
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = true,
        .max_files = 10,
        .allocation_unit_size = 16 * 1024
    };

    g_host.slot = SPI2_HOST;

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = PIN_NUM_CS;
    slot_config.host_id = SPI2_HOST;

    ret = esp_vfs_fat_sdspi_mount(MOUNT_POINT, &g_host, &slot_config, &mount_config, &g_card);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount filesystem. Formatting may be required.");
        } else {
            ESP_LOGE(TAG, "Failed to initialize the card (%s). Make sure SD card is formatted as FAT32.", esp_err_to_name(ret));
        }
        return ret;
    }
    
    sdmmc_card_print_info(stdout, g_card);
    ESP_LOGI(TAG, "SD card initialized successfully!");
    
    return ESP_OK;
}
