/*
 *  battery.c - measure battery voltage
 *
 *  Uses ESP-8266 internal measurment.
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
#include <osapi.h>
#include <os_type.h>
#include <user_interface.h>
//#define INFO os_printf  // override debug.h
#include "debug.h"

#include "battery.h"

static uint32_t reportPID = 0;
static uint32_t myid = 0;
static report_t *myReport;

void ICACHE_FLASH_ATTR
battery_init(uint32_t pid, uint32_t id)
{
    INFO("battery_init()\r\n");
    reportPID = pid;
    myid = id;
    return;
} // end ds_init()


void ICACHE_FLASH_ATTR
battery_start()
{
    INFO("battery_start()\r\n");
    // nothing to do, so report we are done.
    system_os_post(reportPID, myid, 0);
} //end battery_start()


report_t* ICACHE_FLASH_ATTR
battery_report(void)
{
    uint32 voltageRaw = system_get_vdd33();
    myReport = newReport(10);
    os_sprintf(myReport->buffer, "%d.%03d",
            voltageRaw / 1024,
            (voltageRaw % 1024) * 1000 / 1024);
    myReport->len = strlen(myReport->buffer);

    return(myReport);
} // end of battery_report()


void ICACHE_FLASH_ATTR
battery_shutdown(void)
{
    INFO("battery_shutdown()\r\n");
    freeReport(myReport);
    return;

}  //end battery_shutdown()
