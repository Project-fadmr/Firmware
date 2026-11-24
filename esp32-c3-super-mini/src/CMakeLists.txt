idf_component_register(
    SRCS
        "main.c"
        "bsp.c"
        "awr1843_drv.c"
        "espnow_ll.c"

    INCLUDE_DIRS
        "."

    REQUIRES
        esp_driver_gpio
        esp_driver_uart
        esp_pm
        esp_timer
        esp_wifi
        esp_event
        nvs_flash
)