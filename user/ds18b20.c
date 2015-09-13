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
#include "report.h"

#include "ds18b20.h"

extern MQTT_Client mqttClient;

#define MEASUREMENT_US 750000    // max time from datasheet for 12 bits

static os_timer_t read_timer;
static uint32 measurement_start_time;

static uint32_t reportPID = 0;
static uint32_t myid = 0;
static report_t *myReport;

/*
 * report ds18b20 reading
 *
 */
report_t* ICACHE_FLASH_ATTR
ds18B20_report(void)
{
    uint16_t tb;
    int16_t  temperature;
    int16_t mantissa;

    myReport = newReport(10);

    // Read measurement
    ds_reset();
    ds_write(0xcc);   // Skip ROM (address all devices)
    ds_write(0xbe);   // CMD = Read scratch pad

    tb = (uint16_t)ds_read();
    temperature = (int16_t)(tb + ((uint16_t)ds_read() * 256));
    mantissa = temperature % 16;
    temperature /= 16;   // Scale to degC
    if ((temperature == 0) && (mantissa < 0))
    {
        os_sprintf(myReport->buffer, "-%d.%03d",
                temperature, abs((mantissa * 625)/10));
    }
    else
    {
        os_sprintf(myReport->buffer, "%d.%03d",
                temperature, abs((mantissa * 625)/10));
    }
    myReport->len = strlen(myReport->buffer);

    return(myReport);

} //end ds18B20_report(void)


/*
 * ds18B20_init- tell DS18B20 to start measurement
 *
 * The system time is recorded so that we can later assure that enough
 * time has elapsed for the measurement to complete.
 */
void ICACHE_FLASH_ATTR
ds18B20_init(uint32_t pid, uint32_t id)
{
    reportPID = pid;
    myid = id;

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
    os_timer_setfn(&read_timer, (os_timer_func_t *)ds18B20_start, NULL);

    return;
} // end ds18B20_init()


/*
 *  Function name is a misnomer because the measurement was started in
 *  ds18B20_init(). This really checks for the measurement to be
 *  complete.
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
        system_os_post(reportPID, myid, 0);
    }

} //end ds18B20_start()


void ICACHE_FLASH_ATTR
ds18B20_shutdown(void)
{
    INFO("ds18B20_is_shutdown()\r\n");
    // make sure we are not sinking or sourcing power to parasitic
    // one-wire devices (the DS18B20 in this case).
    GPIO_DIS_OUTPUT(ONEWIRE_PIN);
    GPIO_OUTPUT_SET(ONEWIRE_PIN, 0);
    freeReport(myReport);

    return;
}  //end ds18B20_shutdown()
