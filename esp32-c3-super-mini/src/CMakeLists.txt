
idf_component_register(
    SRCS
        "main.c"
        "bsp.c"
        "awr1843_drv.c"
        "espnow_ll.c"
        "supervisor.c"

    INCLUDE_DIRS
        "."

    REQUIRES
        freertos
        esp_driver_gpio
        esp_driver_uart
        esp_pm
        esp_wifi
        esp_event
        esp_netif
        esp_timer
        esp_common
        esp_sleep
        heap
        log
        nvs_flash
)
