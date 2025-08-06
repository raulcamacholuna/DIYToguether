#include <stdio.h>

#include "esp_err.h"
#include "esp_log.h"
#include "esp_check.h"

#include "nvs_flash.h"

#include "esp_lvgl_port.h"

#include "bsp_display.h"
#include "bsp_touch.h"
#include "bsp_i2c.h"
#include "bsp_spi.h"
#include "bsp_wifi.h"
#include "bsp_sdcard.h"
#include "bsp_battery.h"
#include "bsp_qmi8658.h"

#include "iot_button.h"
#include "button_gpio.h"

#include "demos/lv_demos.h"


#include "lvgl_ui.h"

#define EXAMPLE_DISPLAY_ROTATION 0

#if EXAMPLE_DISPLAY_ROTATION == 90 || EXAMPLE_DISPLAY_ROTATION == 270
#define EXAMPLE_LCD_H_RES (320)
#define EXAMPLE_LCD_V_RES (172)
#else
#define EXAMPLE_LCD_H_RES (172)
#define EXAMPLE_LCD_V_RES (320)
#endif

#define EXAMPLE_LCD_DRAW_BUFF_HEIGHT (50)
#define EXAMPLE_LCD_DRAW_BUFF_DOUBLE (1)


static char *TAG = "factory";

/* LCD IO and panel */
static esp_lcd_panel_io_handle_t io_handle = NULL;
static esp_lcd_panel_handle_t panel_handle = NULL;
static esp_lcd_touch_handle_t touch_handle = NULL;

/* LVGL display and touch */
static lv_display_t *lvgl_disp = NULL;
static lv_indev_t *lvgl_touch_indev = NULL;


void lv_fs_fatfs_init(void);
static esp_err_t app_lvgl_init(void);
static void button_init(void);
static void touch_test(void);

void app_main(void)
{
    i2c_master_bus_handle_t i2c_bus_handle;
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    i2c_bus_handle = bsp_i2c_init();
    bsp_spi_init();

    bsp_battery_init();
    bsp_qmi8658_init(i2c_bus_handle);
    bsp_wifi_init("WSTEST", "waveshare0755");
    bsp_display_init(&io_handle, &panel_handle, EXAMPLE_LCD_H_RES * EXAMPLE_LCD_DRAW_BUFF_HEIGHT);
    bsp_touch_init(&touch_handle, i2c_bus_handle, EXAMPLE_LCD_H_RES, EXAMPLE_LCD_V_RES, EXAMPLE_DISPLAY_ROTATION);
    bsp_sdcard_init();
    ESP_ERROR_CHECK(app_lvgl_init());
    lv_fs_fatfs_init();


    bsp_display_brightness_init();
    bsp_display_set_brightness(100);

    button_init();
    touch_test();


    if (lvgl_port_lock(0))
    {
        // lv_demo_benchmark();
        // lv_demo_music();
        // lv_demo_widgets();
        lvgl_ui_init();
        lvgl_port_unlock();
    }
}

static esp_err_t app_lvgl_init(void)
{
    /* Initialize LVGL */
    const lvgl_port_cfg_t lvgl_cfg = {
        .task_priority = 4,       /* LVGL task priority */
        .task_stack = 1024 * 10,  /* LVGL task stack size */
        .task_affinity = -1,      /* LVGL task pinned to core (-1 is no affinity) */
        .task_max_sleep_ms = 500, /* Maximum sleep in LVGL task */
        .timer_period_ms = 5      /* LVGL timer tick period in ms */
    };
    ESP_RETURN_ON_ERROR(lvgl_port_init(&lvgl_cfg), TAG, "LVGL port initialization failed");

    /* Add LCD screen */
    ESP_LOGD(TAG, "Add LCD screen");
    lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = io_handle,
        .panel_handle = panel_handle,
        .buffer_size = EXAMPLE_LCD_H_RES * EXAMPLE_LCD_DRAW_BUFF_HEIGHT,
        .double_buffer = EXAMPLE_LCD_DRAW_BUFF_DOUBLE,
        .hres = EXAMPLE_LCD_H_RES,
        .vres = EXAMPLE_LCD_V_RES,
        .monochrome = false,
        /* Rotation values must be same as used in esp_lcd for initial settings of the screen */
        .rotation = {
            .swap_xy = false,
            .mirror_x = false,
            .mirror_y = false,
        },
        .flags = {
            .buff_dma = true,
        }};
#if EXAMPLE_DISPLAY_ROTATION == 90
    disp_cfg.rotation.swap_xy = true;
    disp_cfg.rotation.mirror_x = true;
    disp_cfg.rotation.mirror_y = false;
    ESP_ERROR_CHECK(esp_lcd_panel_set_gap(panel_handle, 0, 34));
#elif EXAMPLE_DISPLAY_ROTATION == 180
    disp_cfg.rotation.swap_xy = false;
    disp_cfg.rotation.mirror_x = true;
    disp_cfg.rotation.mirror_y = true;
    ESP_ERROR_CHECK(esp_lcd_panel_set_gap(panel_handle, 34, 0));
#elif EXAMPLE_DISPLAY_ROTATION == 270
    disp_cfg.rotation.swap_xy = true;
    disp_cfg.rotation.mirror_x = false;
    disp_cfg.rotation.mirror_y = true;
    ESP_ERROR_CHECK(esp_lcd_panel_set_gap(panel_handle, 0, 34));
#else
    ESP_ERROR_CHECK(esp_lcd_panel_set_gap(panel_handle, 34, 0));
#endif
    lvgl_disp = lvgl_port_add_disp(&disp_cfg);

    /* Add touch input (for selected screen) */
    const lvgl_port_touch_cfg_t touch_cfg = {
        .disp = lvgl_disp,
        .handle = touch_handle,
    };
    lvgl_touch_indev = lvgl_port_add_touch(&touch_cfg);

    return ESP_OK;
}



static bool touch_test_done = false;


static void button_event_cb(void *arg, void *data)
{
    button_event_t event = iot_button_get_event((button_handle_t)arg);
    ESP_LOGI(TAG, "%s", iot_button_get_event_str(event));
    touch_test_done = true;
}

static void button_init(void)
{
    button_config_t btn_cfg = {};
    button_gpio_config_t btn_gpio_cfg = {};
    btn_gpio_cfg.gpio_num = GPIO_NUM_9;
    btn_gpio_cfg.active_level = 0;
    static button_handle_t btn = NULL;
    ESP_ERROR_CHECK(iot_button_new_gpio_device(&btn_cfg, &btn_gpio_cfg, &btn));
    iot_button_register_cb(btn, BUTTON_SINGLE_CLICK, NULL, button_event_cb, NULL);
    // iot_button_register_cb(btn, BUTTON_LONG_PRESS_START, NULL, button_event_cb, NULL);
    // iot_button_register_cb(btn, BUTTON_LONG_PRESS_HOLD, NULL, button_event_cb, NULL);
    // iot_button_register_cb(btn, BUTTON_LONG_PRESS_UP, NULL, button_event_cb, NULL);
    // iot_button_register_cb(btn, BUTTON_PRESS_END, NULL, button_event_cb, NULL);
}

static void touch_test(void)
{
    uint16_t touchpad_x[1] = {0};
    uint16_t touchpad_y[1] = {0};
    uint8_t touchpad_cnt = 0;
    uint16_t color_arr[16] = {0};
    lv_obj_t *lable = NULL;

    for (int i = 0; i < 16; i++)
    {
        color_arr[i] = 0xf800;
    }
    if (lvgl_port_lock(0))
    {
        lable = lv_label_create(lv_scr_act());
        lv_label_set_text(lable, "Touch testing mode \nExit with BOOT button");
        lv_obj_center(lable);
        lvgl_port_unlock();
    }
    vTaskDelay(pdMS_TO_TICKS(500));
    if (lvgl_port_lock(0))
    {
        while (!touch_test_done)
        {
            /* Read data from touch controller into memory */
            esp_lcd_touch_read_data(touch_handle);

            /* Read data from touch controller */
            bool touchpad_pressed = esp_lcd_touch_get_coordinates(touch_handle, touchpad_x, touchpad_y, NULL, &touchpad_cnt, 1);
            if (touchpad_pressed && touchpad_cnt > 0)
            {
                // touchpad_x[0] = EXAMPLE_LCD_H_RES - 1 - touchpad_x[0];

                if (touchpad_x[0] < 2)
                    touchpad_x[0] = 2;
                else if (touchpad_x[0] > EXAMPLE_LCD_H_RES - 2 - 1)
                    touchpad_x[0] = EXAMPLE_LCD_H_RES - 2 - 1;

                if (touchpad_y[0] < 2)
                    touchpad_y[0] = 2;
                else if (touchpad_y[0] > EXAMPLE_LCD_V_RES - 2 - 1)
                    touchpad_y[0] = EXAMPLE_LCD_V_RES - 2 - 1;

                esp_lcd_panel_draw_bitmap(panel_handle, touchpad_x[0] - 2, touchpad_y[0] - 2, touchpad_x[0] + 2, touchpad_y[0] + 2, color_arr);
            }
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        lv_obj_del(lable);
        lvgl_port_unlock();
    }
    
}
