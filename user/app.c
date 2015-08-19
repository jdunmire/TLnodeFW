/*
 * app.c - example application, Hello World
 *
 * Minimal application that prints "Hello, World".
 *
 */
#include <ets_sys.h>
#include <driver/uart.h>
#include <osapi.h>
#include <os_type.h>
#include "mem.h"

// Enable debugging messages
#ifndef INFO
#define INFO os_printf
#endif

/*
 * read temperature from DS18S20
 *
 * Reads last measurement from the scratch pad, reports
 * it via a debug (INFO) message and starts the next measurement.
 */
static void ICACHE_FLASH_ATTR
readTemp(void)
{
    uint16_t tb;
    int16_t  temperature;
    int16_t mantissa;
    char *mBuf = (char*)os_zalloc(20);

    // Read previous measurement
    ds_reset();
    ds_write(0xcc);   // Skip ROM (address all devices)
    ds_write(0xbe);   // CMD = Read scratch pad

    tb = (uint16_t)ds_read();
    temperature = (int16_t)(tb + ((uint16_t)ds_read() * 256));
    mantissa = temperature % 16;
    temperature /= 16;   // Scale to degC
    if ((temperature == 0) && (mantissa < 0))
    {
        os_sprintf(mBuf, "-%d.%03d degC", temperature, abs((mantissa * 625)/10));
    }
    else
    {
        os_sprintf(mBuf, "%d.%03d degC", temperature, abs((mantissa * 625)/10));
    }

    INFO("\r\n");
    INFO(mBuf);
    INFO("\r\n");

    os_free(mBuf);

} //end readTemp(void)


// this will be in micro-seconds
LOCAL uint32 measurement_start_time;
#define MEASUREMENT_US 750000    // max time from datasheet for 12 bits

static void ICACHE_FLASH_ATTR
startTempMeasurement(void)
{
    // Start temperature measurement
    // TODO: be specific about the number of bits for the measurment
    ds_reset();
    ds_write(0xcc);   // Skip ROM (address all devices)
    ds_write(0x44);   // CMD = Start conversion
    measurement_start_time = system_get_time();
    return;
} // end startTempMeasurment()

static os_timer_t read_timer;
static os_timer_t shutdown_timer;

static void ICACHE_FLASH_ATTR
user_deep_sleep(void)
{
    system_deep_sleep(5000000);
}

static void ICACHE_FLASH_ATTR
measurementReady(void)
{
    readTemp();
    // Allow time for the print buffers to flush
    os_timer_arm(&shutdown_timer, 5, 0);
}

static void ICACHE_FLASH_ATTR
sys_init_complete(void)
{
    // No need to worry about overflow or 32-bit wrap because
    // system_get_time() always starts from zero.
    uint32 elapsed_us = system_get_time() - measurement_start_time;

    if (elapsed_us < MEASUREMENT_US) {
        INFO("Delaying %d us\r\n", MEASUREMENT_US - elapsed_us);
        os_timer_arm(&read_timer, (MEASUREMENT_US - elapsed_us) / 1000 , 0);
    }
    else
    {
        INFO("Skipping delay\r\n");
        measurementReady();
    }
}

//Init function 
void ICACHE_FLASH_ATTR
user_init()
{
    uart_init(BIT_RATE_115200);

    startTempMeasurement();

    os_timer_disarm(&read_timer);
    os_timer_setfn(&read_timer, (os_timer_func_t *)measurementReady, NULL);

    os_timer_disarm(&shutdown_timer);
    os_timer_setfn(&shutdown_timer, (os_timer_func_t *)user_deep_sleep, NULL);

    system_init_done_cb(sys_init_complete);

}
