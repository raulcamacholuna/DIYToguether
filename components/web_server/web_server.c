/*
  Fichero: ./components/web_server/web_server.c
  Fecha: 12/08/2025 - 04:55 pm
  Último cambio: Corregida la ruta base para servir archivos desde la raíz de la SD, eliminando el subdirectorio '/web'.
  Descripción: Se ha eliminado el subdirectorio "/web" de la ruta de búsqueda
               de archivos. El servidor ahora buscará los ficheros (index.html, etc.)
               directamente en la raíz del punto de montaje de la tarjeta SD,
               coincidiendo con la estructura de archivos del usuario.
*/
#include "web_server.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_vfs.h"
#include "bsp_api.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "WEB_SERVER";
#define WEB_MOUNT_POINT "/sdcard"
#define FILE_UPLOAD_BUFFER_SIZE 2048

struct file_upload_ctx {
    FILE *fd;
    char filepath[256];
};

static esp_err_t upload_post_handler(httpd_req_t *req) {
    char buf[FILE_UPLOAD_BUFFER_SIZE];
    int received;
    struct file_upload_ctx *ctx = (struct file_upload_ctx *)req->user_ctx;

    if (ctx == NULL) {
        ctx = malloc(sizeof(struct file_upload_ctx));
        if (!ctx) return ESP_ERR_NO_MEM;
        req->user_ctx = ctx;

        char header_val[128];
        if (httpd_req_get_hdr_value_str(req, "Content-Disposition", header_val, sizeof(header_val)) == ESP_OK) {
            char *filename_ptr = strstr(header_val, "filename=\"");
            if (filename_ptr) {
                filename_ptr += strlen("filename=\"");
                char *end_ptr = strchr(filename_ptr, '\"');
                if (end_ptr) {
                    *end_ptr = '\0';
                    snprintf(ctx->filepath, sizeof(ctx->filepath), "%s/%s", WEB_MOUNT_POINT, filename_ptr);
                    ESP_LOGI(TAG, "Iniciando subida de archivo: %s", ctx->filepath);
                    ctx->fd = fopen(ctx->filepath, "wb");
                    if (!ctx->fd) {
                        ESP_LOGE(TAG, "No se pudo abrir el archivo para escritura");
                        httpd_resp_send_500(req);
                        return ESP_FAIL;
                    }
                }
            }
        } else {
             ESP_LOGE(TAG, "Falta la cabecera 'Content-Disposition'");
             httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Falta cabecera 'Content-Disposition'.");
             return ESP_FAIL;
        }
    }

    received = httpd_req_recv(req, buf, MIN(req->content_len, sizeof(buf)));
    if (received <= 0) {
        if (received == HTTPD_SOCK_ERR_TIMEOUT) {
            httpd_resp_send_408(req);
        }
        return ESP_FAIL;
    }

    if (fwrite(buf, 1, received, ctx->fd) != received) {
        ESP_LOGE(TAG, "Error al escribir en el archivo");
        fclose(ctx->fd);
        unlink(ctx->filepath);
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    if (received < sizeof(buf)) {
        fclose(ctx->fd);
        ESP_LOGI(TAG, "Archivo subido correctamente a %s", ctx->filepath);
        httpd_resp_send(req, "Archivo subido con exito!", HTTPD_RESP_USE_STRLEN);
        free(ctx);
        req->user_ctx = NULL;
    }

    return ESP_OK;
}

static esp_err_t file_get_handler(httpd_req_t *req) {
    char filepath[256];
    const char *uri = req->uri;
    
    if (strcmp(uri, "/") == 0) {
        uri = "/index.html";
    }

    // Se construye la ruta base directamente desde el punto de montaje de la SD.
    int len = snprintf(filepath, sizeof(filepath), "%s", WEB_MOUNT_POINT);
    if (len < 0 || len >= sizeof(filepath)) {
        ESP_LOGE(TAG, "Error al construir la ruta base del archivo.");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    if (len + strlen(uri) >= sizeof(filepath)) {
        ESP_LOGE(TAG, "URI demasiado larga: %s", uri);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "URI too long");
        return ESP_FAIL;
    }
    strcat(filepath, uri);

    ESP_LOGI(TAG, "Sirviendo archivo: %s", filepath);

    FILE *f = fopen(filepath, "r");
    if (f == NULL) {
        ESP_LOGE(TAG, "No se pudo encontrar el archivo: %s", filepath);
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }

    if (strstr(filepath, ".css")) {
        httpd_resp_set_type(req, "text/css");
    } else if (strstr(filepath, ".js")) {
        httpd_resp_set_type(req, "application/javascript");
    } else {
        httpd_resp_set_type(req, "text/html");
    }

    char chunk[512];
    size_t chunksize;
    do {
        chunksize = fread(chunk, 1, sizeof(chunk), f);
        if (chunksize > 0) {
            if (httpd_resp_send_chunk(req, chunk, chunksize) != ESP_OK) {
                fclose(f);
                ESP_LOGE(TAG, "Error al enviar chunk de archivo");
                return ESP_FAIL;
            }
        }
    } while (chunksize != 0);

    fclose(f);
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}


static httpd_handle_t start_webserver(void) {
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.uri_match_fn = httpd_uri_match_wildcard;
    config.max_uri_handlers = 10;

    ESP_LOGI(TAG, "Iniciando servidor httpd");
    if (httpd_start(&server, &config) == ESP_OK) {
        
        httpd_uri_t upload_uri = {
            .uri       = "/upload",
            .method    = HTTP_POST,
            .handler   = upload_post_handler,
            .user_ctx  = NULL
        };
        httpd_register_uri_handler(server, &upload_uri);
        
        httpd_uri_t file_get_uri = {
            .uri       = "/*",
            .method    = HTTP_GET,
            .handler   = file_get_handler,
            .user_ctx  = NULL
        };
        httpd_register_uri_handler(server, &file_get_uri);
        
    }
    return server;
}

void web_server_start(void) {
    ESP_LOGI(TAG, "Iniciando servidor web de configuracion.");
    
    start_webserver();

    while(1) {
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}
