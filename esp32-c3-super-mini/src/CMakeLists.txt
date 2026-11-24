idf_component_register(
    SRCS "main.c" "awr1843_drv.c" "bsp.c" "espnow_ll.c" "supervisor.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES esp_driver_uart esp_driver_gpio esp_pm
)
