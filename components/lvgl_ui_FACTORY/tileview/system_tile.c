#include "system_tile.h"

#include "bsp_sdcard.h"
#include "bsp_display.h"
#include "bsp_battery.h"

#include "esp_flash.h"
// #include "esp_psram.h"
#include "driver/temperature_sensor.h"
#include "esp_private/esp_clk.h"

static lv_obj_t *label_brightness;
static lv_obj_t *label_chip_temp;
static lv_obj_t *label_battery;

temperature_sensor_handle_t temp_sensor = NULL;

static void slider_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_VALUE_CHANGED)
    {
        lv_obj_t *slider = lv_event_get_target(e);
        int value = lv_slider_get_value(slider);
        // printf("Slider value: %d\n", value);

        lv_label_set_text_fmt(label_brightness, "%d%%", value);
        bsp_display_set_brightness(value);
        // bsp_display_handle_t display = bsp_display_get_handle();
        // display->set_brightness(display, value);
        lv_event_stop_bubbling(e);
    }
}

static void system_time_cb(lv_timer_t *timer)
{
    char str[20];
    float tsens_out;
    float bat_voltage;
    bsp_battery_get_voltage(&bat_voltage, NULL);
    sprintf(str, "%.2f V", bat_voltage);
    lv_label_set_text(label_battery, str);

    temperature_sensor_get_celsius(temp_sensor, &tsens_out);
    sprintf(str, "%d C", (int)tsens_out);
    lv_label_set_text(label_chip_temp, str);
}


void system_tile_init(lv_obj_t *parent)
{
    uint32_t flash_size;
    uint64_t sdcard_size;
    uint32_t cpu_freq;
    uint8_t brightness;

    brightness = bsp_display_get_brightness();
    /*Create a list*/
    lv_obj_t *list = lv_list_create(parent);
    lv_obj_t *lable =  lv_label_create(parent);
    lv_obj_set_style_text_font(lable, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_label_set_text(lable, "System");
    lv_obj_align(lable, LV_ALIGN_TOP_MID, 0, 3);


    lv_obj_set_size(list, lv_pct(100), lv_pct(75));
    // lv_obj_center(list);
    lv_obj_align(list, LV_ALIGN_TOP_MID, 0, 30);

    // lv_obj_t *button = lv_btn_create(parent);
    // lable = lv_label_create(parent);
    // lv_label_set_text(lable, "ES8311\nTest");
    // lv_obj_align(lable, LV_ALIGN_BOTTOM_MID, -125, -15);

    // lv_obj_t *sw = lv_switch_create(parent);
    // lv_obj_align(sw, LV_ALIGN_BOTTOM_MID, -75, -15);

    lv_obj_t *slider = lv_slider_create(parent);
    lv_slider_set_range(slider, 1, 100);    
    lv_slider_set_value(slider, brightness, LV_ANIM_OFF); 

    lv_obj_set_size(slider, lv_pct(70), lv_pct(5));     
    lv_obj_align(slider, LV_ALIGN_BOTTOM_MID, 0, -18); 
    lv_obj_add_event_cb(slider, slider_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_t *list_item;
    list_item = lv_list_add_btn(list, NULL, "Chip");
    lv_obj_t *label_chip = lv_label_create(list_item);
    lv_label_set_text(label_chip, "ESP32-C6");

    list_item = lv_list_add_btn(list, NULL, "Temp");
    label_chip_temp = lv_label_create(list_item);
    lv_label_set_text(label_chip_temp, "--- C");

    list_item = lv_list_add_btn(list, NULL, "Freq");
    lv_obj_t *label_chip_freq = lv_label_create(list_item);
    cpu_freq = esp_clk_cpu_freq();
    lv_label_set_text_fmt(label_chip_freq, "%d MHz", (int)(cpu_freq / 1000 / 1000));

    list_item = lv_list_add_btn(list, NULL, "Brightness");
    label_brightness = lv_label_create(list_item);
    lv_label_set_text_fmt(label_brightness, "%d%%", brightness);

    list_item = lv_list_add_btn(list, NULL, "RAM");
    lv_obj_t *label_ram = lv_label_create(list_item);
    lv_label_set_text(label_ram, "512 KB");

    list_item = lv_list_add_btn(list, NULL, "Flash");
    lv_obj_t *label_flash = lv_label_create(list_item);
    esp_flash_get_size(NULL, &flash_size);
    lv_label_set_text_fmt(label_flash, "%d MB", (int)(flash_size / 1024 / 1024));

    list_item = lv_list_add_btn(list, NULL, "SDCard");
    lv_obj_t *label_sd = lv_label_create(list_item);
    sdcard_size = bsp_sdcard_get_size();
    lv_label_set_text_fmt(label_sd, "%d MB", (int)(sdcard_size / 1024 / 1024));
    
    list_item = lv_list_add_btn(list, NULL, "Battery");
    label_battery = lv_label_create(list_item);
    lv_label_set_text(label_battery, "--- V");

    temperature_sensor_config_t temp_sensor_config = TEMPERATURE_SENSOR_CONFIG_DEFAULT(10, 80);
    temperature_sensor_install(&temp_sensor_config, &temp_sensor);
    temperature_sensor_enable(temp_sensor);
    
    lv_timer_create(system_time_cb, 1000, NULL);
}