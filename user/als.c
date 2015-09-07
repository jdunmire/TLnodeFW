/*
 *  Ambient light sensor support routines
 *
 *  The light sensor is the ISL29035
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
#include "driver/i2c.h"
#include "driver/i2c_isl.h"
#include "debug.h"

#include "als.h"

extern MQTT_Client mqttClient;

#define MEASUREMENT_US 105000    // max time from datasheet for 16 bits

static os_timer_t read_timer;
static uint32 measurement_start_time;
static bool status_cplt = false;

/*
 * read light level from isl92035
 *
 */
static void ICACHE_FLASH_ATTR
readLight(void)
{
    uint16_t lux = 0;
    uint16_t lux_check = 1;
    int maxloops = 5;
    char *mBuf = (char*)os_zalloc(20);
    char *tBuf = (char *)os_zalloc(strlen(sysCfg.device_id) + 40);

    // TODO: need to scale based on resolution
    lux = isl_read_word(ISL_DATA_REG);
    lux_check = isl_read_word(ISL_DATA_REG);

    while ((lux != lux_check) && (maxloops >= 0)) {
        lux = lux_check;
        maxloops -= 1;
        lux_check = isl_read_word(ISL_DATA_REG);
    }

    os_sprintf(tBuf, "%s/ambient_lux", sysCfg.device_id);
    os_sprintf(mBuf, "%d", lux);
    MQTT_Publish(&mqttClient, tBuf, mBuf, strlen(mBuf), 0, 1);

    INFO("\r\n");
    INFO(mBuf);
    INFO("\r\n");

    os_free(mBuf);
    os_free(tBuf);
    status_cplt = true;

} //end readLight(void)



/*
 * startTempMeasurement - tell DS18B20 to start measurement
 *
 * The system time is recorded so that we can later assure that enough
 * time has elapsed for the measurement to complete.
 */
void ICACHE_FLASH_ATTR
als_init(void)
{
    INFO("als_init()\r\n");
    i2c_init();
    // Start the light sensor measurements.
    isl_write_byte(ISL_CMD1_REG, ISL_MODE_ALS_CONT);
    isl_write_byte(ISL_CMD2_REG, (ISL_RANGE_16K | ISL_ADC_16_BIT));

    measurement_start_time = system_get_time();

    // Setup up the timer, but don't start it
    os_timer_disarm(&read_timer);
    os_timer_setfn(&read_timer, (os_timer_func_t *)readLight, NULL);

    return;
} // end ds_init()


/*
 * handle MQTT connection
 *
 */
void ICACHE_FLASH_ATTR
als_start()
{
    INFO("als_start()\r\n");
    
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
        INFO("Skipping delay for als\r\n");
        readLight();
    }

} //end als_start()


bool ICACHE_FLASH_ATTR
als_is_complete()
{
    INFO("als_is_complete()\r\n");
    return(status_cplt);
} // end als_is_complete()


void ICACHE_FLASH_ATTR
als_shutdown(void)
{
    INFO("als_shutdown()\r\n");
    // power down the light sensor
    isl_write_byte(ISL_CMD1_REG, ISL_MODE_PD);

    return;
}  //end als_shutdown()
