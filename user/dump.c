/*
 * dump.c - dump interesting system information
 *
 */
#include <ets_sys.h>
#include <driver/uart.h>
#include <osapi.h>
#include <os_type.h>
#include "user_interface.h"

// Enable debugging messages
#ifndef INFO
#define INFO os_printf
#endif

void ICACHE_FLASH_ATTR
dumpInfo(void) {
    uint32 voltageRaw = system_get_vdd33();
    uint32 ticks;
    enum flash_size_map fmap;
    enum rst_reason reason;
    struct rst_info *rstInfo;

    os_printf("\r\n");
    os_printf("SDK version : %s\r\n", system_get_sdk_version());
    os_printf("Chip ID : %x\r\n", system_get_chip_id());
    os_printf("Voltage : %d.%03d (%d)\r\n",
            voltageRaw / 1024,
            (voltageRaw % 1024) * 1000 / 1024,
            voltageRaw);
    os_printf("meminfo :\r\n");
    system_print_meminfo();
    os_printf("\r\n");
    os_printf("Free Heap : %x (%d)\r\n",
            system_get_free_heap_size(),
            system_get_free_heap_size());

    os_printf("Time(us) : %d\r\n", system_get_time());
    os_printf("RTC us/tic : %d\r\n", system_rtc_clock_cali_proc());
    ticks = system_get_rtc_time();
    os_printf("RTC ticks : %d\r\n", ticks);
    // TODO: Calculate rtc in us
    os_printf("Boot Version : %d\r\n", system_get_boot_version());
    os_printf("Userbin Addr : %x\r\n", system_get_userbin_addr());
    os_printf("Boot Mode : %d\r\n", system_get_boot_mode());
    os_printf("CPU Freq : %d \r\n", system_get_cpu_freq());
    fmap = system_get_flash_size_map();
    os_printf("Flash size-map : %d ", fmap);
    switch (fmap) {
        case FLASH_SIZE_4M_MAP_256_256:
            os_printf("(4M MAP 256 256)");
            break;
        case FLASH_SIZE_2M:
            os_printf("(2M)");
            break;
        case FLASH_SIZE_8M_MAP_512_512:
            os_printf("(8M MAP 512 512)");
            break;
        case FLASH_SIZE_16M_MAP_512_512:
            os_printf("(16M MAP 512 512)");
            break;
        case FLASH_SIZE_32M_MAP_512_512:
            os_printf("(32M MAP 512 512)");
            break;
        case FLASH_SIZE_16M_MAP_1024_1024:
            os_printf("(16M MAP 1024 1024)");
            break;
        case FLASH_SIZE_32M_MAP_1024_1024:
            os_printf("(32M MAP 1024 1024)");
            break;
        default:
            os_printf("(Unknown)");
            break;
    }
    os_printf("\r\n");

    rstInfo = system_get_rst_info();
    os_printf("Reset Reason : ");
    switch ((enum rst_reason)rstInfo->reason) {
        case REASON_DEFAULT_RST:
            os_printf("Normal startup");
            break;
        case REASON_WDT_RST:
            os_printf("HW watchdog reset");
            break;
        case REASON_EXCEPTION_RST:
            os_printf("Exception reset");
            break;
        case REASON_SOFT_WDT_RST:
            os_printf("SW watchdog reset");
            break;
        case REASON_SOFT_RESTART:
            os_printf("Soft restart");
            break;
        case REASON_DEEP_SLEEP_AWAKE:
            os_printf("Deep Sleep wake up");
            break;
        case REASON_EXT_SYS_RST:
            os_printf("External reset");
            break;
        default:
            os_printf("(Unknown)");
            break;
    }
    os_printf("\r\n");

    os_printf("SPI Flash ID : %x \r\n", spi_flash_get_id());

} // end of dumpInfo()



