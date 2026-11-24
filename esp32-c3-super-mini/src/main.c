void app_main(void)
{
    ESP_ERROR_CHECK(bsp_power_init());

    ESP_ERROR_CHECK(bsp_power_gpio_init());

    ESP_ERROR_CHECK(bsp_led_init());

    ESP_ERROR_CHECK(bsp_sched_init());
    ESP_ERROR_CHECK(bsp_sched_start());

{