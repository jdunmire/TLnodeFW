/*
 *  ds18b20 support routines for sensor Node 
 *
 *  Copyright (C) 2015 Jerry Dunmire
 *  This file is part of sensorNode
 *
 *  sensorNode is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  sensorNode is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with sensorNode.  If not, see <http://www.gnu.org/licenses/>.
 *
 */
#include <ets_sys.h>
#include <driver/uart.h>
#include <osapi.h>
#include <os_type.h>
#include <gpio.h>
#include "mem.h"
#include "mqtt.h"
#include "config.h"
#include "user_config.h"
#include "debug.h"
#include "driver/onewire.h"

#include "ds18b20.h"

extern MQTT_Client mqttClient;

#define MEASUREMENT_US 750000    // max time from datasheet for 12 bits

static os_timer_t read_timer;
static uint32 measurement_start_time;
static bool status_cplt = false;

/*
 * read temperature from DS18S20
 *
 */
static void ICACHE_FLASH_ATTR
readTemp(void)
{
    uint16_t tb;
    int16_t  temperature;
    int16_t mantissa;
    char *mBuf = (char*)os_zalloc(20);
    char *tBuf = (char *)os_zalloc(strlen(sysCfg.device_id) + 40);

    // Read measurement
    ds_reset();
    ds_write(0xcc);   // Skip ROM (address all devices)
    ds_write(0xbe);   // CMD = Read scratch pad

    tb = (uint16_t)ds_read();
    temperature = (int16_t)(tb + ((uint16_t)ds_read() * 256));
    mantissa = temperature % 16;
    temperature /= 16;   // Scale to degC
    os_sprintf(tBuf, "%s/degC", sysCfg.device_id);
    if ((temperature == 0) && (mantissa < 0))
    {
        os_sprintf(mBuf, "-%d.%03d", temperature, abs((mantissa * 625)/10));
    }
    else
    {
        os_sprintf(mBuf, "%d.%03d", temperature, abs((mantissa * 625)/10));
    }
    MQTT_Publish(&mqttClient, tBuf, mBuf, strlen(mBuf), 0, 1);

    INFO("\r\n");
    INFO(mBuf);
    INFO("\r\n");

    os_free(mBuf);
    os_free(tBuf);
    status_cplt = true;

} //end readTemp(void)


/*
 * startTempMeasurement - tell DS18B20 to start measurement
 *
 * The system time is recorded so that we can later assure that enough
 * time has elapsed for the measurement to complete.
 */
void ICACHE_FLASH_ATTR
ds18B20_init(void)
{
    INFO("ds18B20_init()\r\n");
    // I thought the board was designed for WIRED power, but there is a
    // design error on the board or in the onewire code. 2015/08/18
    // 2015/09/05 - looks like I might have ordered parasitic part.
    //ds_init(ONEWIRE_WIRED_PWR);
    ds_init(ONEWIRE_PARASITIC_PWR);

    // Start temperature measurement
    // TODO: be specific about the number of bits for the measurment
    ds_reset();
    ds_write(0xcc);   // Skip ROM (address all devices)
    ds_write(0x44);   // CMD = Start conversion
    measurement_start_time = system_get_time();

    // Setup up the timer, but don't start it
    os_timer_disarm(&read_timer);
    os_timer_setfn(&read_timer, (os_timer_func_t *)readTemp, NULL);

    return;
} // end ds_init()


/*
 * handle MQTT connection
 *
 */
void ICACHE_FLASH_ATTR
ds18B20_start()
{
    INFO("ds18B20_start()\r\n");
    
    // Read and report the measured temperature.
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
        readTemp();
    }

} //end ds18B20_start()


bool ICACHE_FLASH_ATTR
ds18B20_is_complete()
{
    INFO("ds18B20_is_complete()\r\n");
    return(status_cplt);
} // end ds18B20_is_complete()


void ICACHE_FLASH_ATTR
ds18B20_shutdown(void)
{
    INFO("ds18B20_is_shutdown()\r\n");
    // make sure we are not sinking or sourcing power to parasitic
    // one-wire devices (the DS18B20 in this case).
    GPIO_DIS_OUTPUT(ONEWIRE_PIN);
    GPIO_OUTPUT_SET(ONEWIRE_PIN, 0);

    return;
}
