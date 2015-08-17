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

#define user_procTaskPrio        0
#define user_procTaskQueueLen    1
os_event_t    user_procTaskQueue[user_procTaskQueueLen];
static void loop(os_event_t *events);

/*
 * read temperature from DS18S20
 *
 * Reads last measurement from the scratch pad, reports
 * it via a debug (INFO) message and starts the next measurement.
 */
void ICACHE_FLASH_ATTR
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

    INFO(mBuf);
    INFO("\r\n");

    // Start next temperature measurement
    ds_reset();
    ds_write(0xcc);   // Skip ROM (address all devices)
    ds_write(0x44);   // CMD = Start conversion

    os_free(mBuf);

} //end readTemp(void)


//Main code function
static void ICACHE_FLASH_ATTR
loop(os_event_t *events)
{
    readTemp();
    // If the delay is too long, there will be a watchdog (wdt) reset
    os_delay_us(1000000);
    system_os_post(user_procTaskPrio, 0, 0 );
}

//Init function 
void ICACHE_FLASH_ATTR
user_init()
{
    uart_init(BIT_RATE_115200);

    //Start os task
    system_os_task(loop, user_procTaskPrio,user_procTaskQueue, user_procTaskQueueLen);

    system_os_post(user_procTaskPrio, 0, 0 );
}
