#include <gpiod.h>
#include <stdio.h>
#include <assert.h>
#include "led.h"

#define LED_OFFSET 2

static struct gpiod_chip *chip = NULL;
static struct gpiod_line_config *config = NULL;
static struct gpiod_line_request *request = NULL;

int led_begin()
{
    chip = gpiod_chip_open("/dev/gpiochip1"); 
    if (!chip)
    {
        perror("Failed to open GPIO chip");
        return -1;
    }

    config = gpiod_line_config_new();
    if (!config)
    {
        perror("Failed to allocate GPIO line config");
        return -1;
    }

    struct gpiod_line_settings *settings = gpiod_line_settings_new();
    if (!settings)
    {
        perror("Failed to allocate line settings");
        return -1;
    }

    gpiod_line_settings_set_direction(settings, GPIOD_LINE_DIRECTION_OUTPUT);
    gpiod_line_settings_set_drive(settings, GPIOD_LINE_DRIVE_PUSH_PULL);

    unsigned int offset = LED_OFFSET;
    int ret = gpiod_line_config_add_line_settings(config, &offset, 1, settings);
    if (ret < 0)
    {
        perror("Failed to set line settings");
        return -1;
    }

    gpiod_line_settings_free(settings);
    settings = NULL;

    const enum gpiod_line_value value = GPIOD_LINE_VALUE_INACTIVE;
    ret = gpiod_line_config_set_output_values(config, &value, 1);
    if (ret < 0)
    {
        perror("Failed to set line value");
        return -1;
    }

    struct gpiod_request_config *req_config = gpiod_request_config_new();
    if (!req_config)
    {
        perror("Failed to allocate request config");
        return -1;
    }

    gpiod_request_config_set_consumer(req_config, "ledd");

    request = gpiod_chip_request_lines(chip, req_config, config);
    if (!request)
    {
        perror("Failed to request line");
        return -1;
    }

    return 0;
}

int led_write(Value v)
{
    assert(v == 1 || v == 0);
    assert(request);

    const enum gpiod_line_value value = v == 1 ? GPIOD_LINE_VALUE_ACTIVE : GPIOD_LINE_VALUE_INACTIVE;
    if (gpiod_line_request_set_value(request, LED_OFFSET, value) < 0)
    {
        perror("Failed to write line");
        return -1;
    }
    return 0;
}

void led_end()
{
    assert(chip);
    assert(config);

    gpiod_line_request_release(request);
    gpiod_line_config_free(config);
    gpiod_chip_close(chip);
}
