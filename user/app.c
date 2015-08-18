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
readTemp(void *arg)
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

    INFO(mBuf);
    INFO("\r\n");

    // Start next temperature measurement
    ds_reset();
    ds_write(0xcc);   // Skip ROM (address all devices)
    ds_write(0x44);   // CMD = Start conversion

    os_free(mBuf);

} //end readTemp(void)


static os_timer_t read_timer;

//Init function 
void ICACHE_FLASH_ATTR
user_init()
{
    uart_init(BIT_RATE_115200);

    os_timer_disarm(&read_timer);
    os_timer_setfn(&read_timer, (os_timer_func_t *)readTemp, (void *)0);
    os_timer_arm(&read_timer, 5000, 1);
}
