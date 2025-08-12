/*
  Fichero: ./components/service_screen/service_screen.c
  Fecha: 12/08/2025 - 05:15 pm
  Último cambio: Eliminada la inicialización de hardware redundante para evitar crasheos.
  Descripción: Se elimina la llamada a bsp_init_service_mode() para que la inicialización del hardware sea gestionada únicamente por las funciones de modo en main.c, evitando dobles inicializaciones y el crash del bus SPI.
*/
#include "service_screen.h"
#include "bsp_api.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_lcd_panel_ops.h"
#include <stdlib.h>
#include <stdio.h>

static const char *TAG = "SERVICE_SCREEN";

esp_err_t service_screen_show(const char* image_path) {
    ESP_LOGI(TAG, "Mostrando pantalla de servicio NATIVA desde: %s", image_path);
    
    // Se asume que el hardware ya ha sido inicializado por el llamante (main.c)
    // Se elimina la llamada redundante a sp_init_service_mode() que causaba el crash.

    // 1. Obtener información y manejador del display
    esp_lcd_panel_handle_t panel_handle = bsp_get_display_handle();
    if (!panel_handle) {
        ESP_LOGE(TAG, "Error: el manejador del panel no es válido. ¿Se inicializó el BSP?");
        return ESP_ERR_INVALID_STATE;
    }
    const int h_res = bsp_get_display_hres();
    const int v_res = bsp_get_display_vres();
    const size_t buf_size = h_res * v_res * 2; // 2 bytes por píxel (RGB565)

    // 2. Reservar memoria para la imagen
    uint8_t *img_buf = (uint8_t*)malloc(buf_size);
    if (!img_buf) {
        ESP_LOGE(TAG, "Fallo al reservar %d bytes para la imagen de servicio.", (int)buf_size);
        return ESP_ERR_NO_MEM;
    }

    // 3. Leer el fichero binario desde la SD
    FILE* f = fopen(image_path, "rb");
    if (!f) {
        ESP_LOGE(TAG, "No se pudo abrir la imagen: %s", image_path);
        free(img_buf);
        return ESP_FAIL;
    }
    size_t bytes_read = fread(img_buf, 1, buf_size, f);
    fclose(f);

    if (bytes_read != buf_size) {
        ESP_LOGW(TAG, "El tamaño del fichero de imagen (%d) no coincide con el esperado (%d).", (int)bytes_read, (int)buf_size);
    }

    // 4. Volcar el búfer directamente al display
    ESP_LOGI(TAG, "Renderizando imagen nativa en el display...");
    esp_lcd_panel_draw_bitmap(panel_handle, 0, 0, h_res, v_res, img_buf);

    // 5. Liberar la memoria
    free(img_buf);
    
    ESP_LOGI(TAG, "Pantalla de servicio mostrada con éxito.");
    return ESP_OK;
}
