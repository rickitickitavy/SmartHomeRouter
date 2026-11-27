#include "HostSlaveReset.h"

#if defined(BOARD_ROLE_HOST)

#include "pins.h"

#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *kLogTag = "host_rst";
static constexpr int kRstPulseMs = 15;

void hostResetSlaveSynchronous(void) {
    gpio_config_t config = {};
    config.pin_bit_mask = 1ULL << PIN_SLAVE_RST;
    config.mode = GPIO_MODE_OUTPUT;
    config.pull_up_en = GPIO_PULLUP_DISABLE;
    config.pull_down_en = GPIO_PULLDOWN_DISABLE;
    config.intr_type = GPIO_INTR_DISABLE;
    ESP_ERROR_CHECK(gpio_config(&config));

    ESP_ERROR_CHECK(gpio_set_level(static_cast<gpio_num_t>(PIN_SLAVE_RST), 0));
    vTaskDelay(pdMS_TO_TICKS(kRstPulseMs));
    ESP_ERROR_CHECK(gpio_set_level(static_cast<gpio_num_t>(PIN_SLAVE_RST), 1));
    ESP_LOGI(kLogTag, "Slave reset released (sync), waiting SLAVE_READY");
}

#endif
