void app_main(void)
{
    bsp_led_init();

    // Power management: 160 MHz maximum, 10 MHz minimum
    bsp_power_init(160, 10, true);

    bsp_sched_init();
    bsp_sched_start();

    bsp_mem_stats_t stats;
    bsp_mem_get_stats(&stats);

    printf("Total heap:       %u bytes\n", (unsigned)stats.total);
    printf("Free heap:        %u bytes\n", (unsigned)stats.free);
    printf("Minimum free:     %u bytes\n", (unsigned)stats.minimum_free);
    printf("Largest block:    %u bytes\n", (unsigned)stats.largest_block);
}