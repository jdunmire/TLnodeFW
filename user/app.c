/*
 * app.c - toplevel for SensorNode
 *
 * Reads sensors and reports using MQTT
 *
 *  Copyright (C) 2015 Jerry Dunmire
 *  This file is part of TLnodeFW
 *
 *  TLnodeFW is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  TLnodeFW is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with TLnodeFW.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

//#define INFO os_printf  // over-ride debug setting
#include "debug.h"

#include <ets_sys.h>
#include <driver/uart.h>
#include <osapi.h>
#include <os_type.h>
#include <gpio.h>
#include <mem.h>
#include "mqtt.h"
#include "config.h"
#include "als.h"
#include "report.h"
#include "user_config.h"
#include "ds18b20.h"
#include "battery.h"

// Device ID = 1, Application version = 1
#define ID_VERSION_STR  "1,1"

#define DEEP_SLEEP_SECONDS 300
#define US_PER_SEC 1000000

static os_timer_t shutdown_timer;
static os_timer_t watchdog_timer;

#define REPORTER_PID    1   // 0-2, low to high, 0 used by MQTT
#define REPORTER_QLEN   3   // allow space for all drivers to report
#define DRIVER_1        0x01
#define DRIVER_2        0x02
#define DRIVER_3        0x04
// system_os_post(REPORTER_PID, driverID, driverStatus );

static os_event_t       reporter_queue[REPORTER_QLEN];
static uint8_t          driverStatusMask = 0;

MQTT_Client mqttClient;


/*
 * This gets called when the connection to the WiFi AP
 * changes.
 */
void ICACHE_FLASH_ATTR
wifiConnectCb(uint8_t status)
{
    INFO("wifiConnectCb\r\n");
    ip_addr_t *addr = (ip_addr_t *)os_zalloc(sizeof(ip_addr_t));
    if(status == STATION_GOT_IP){
        MQTT_Connect(&mqttClient);
        // The INFO message will be 'TCP: Connect to...'
    }
    else
    {
        MQTT_Disconnect(&mqttClient);
        // The INFO message will be 'Free memory'
    }
    os_free(addr);
    INFO("wifiConnectCb exit\r\n");
}


/*
 * handle MQTT disconnect
 *
 */
void ICACHE_FLASH_ATTR
mqttDisconnectedCb(uint32_t *args)
{
    MQTT_Client* client = (MQTT_Client*)args;
    INFO("MQTT: Disconnected\r\n");
}

/*
 * handle MQTT published event
 *
 */
void ICACHE_FLASH_ATTR
mqttPublishedCb(uint32_t *args)
{
    MQTT_Client* client = (MQTT_Client*)args;
    INFO("MQTT: Report published\r\n");

    // shutdown in 1 milli-second
    os_timer_arm(&shutdown_timer, 1, 0);
}


/*
 * handle MQTT connection
 *
 */
void ICACHE_FLASH_ATTR
mqttConnectedCb(uint32_t *args)
{
    MQTT_Client* client = (MQTT_Client*)args;
    INFO(" MQTT: Connected\r\n");
    
    ds18B20_start();
    als_start();
    battery_start();

} //end mqttConnectedCb()


/*
 * handle data received by MQTT
 *
 */
void ICACHE_FLASH_ATTR
mqttDataCb(
        uint32_t *args,
        const char* topic,
        uint32_t topic_len,
        const char *data,
        uint32_t data_len
        )
{
    char *topicBuf = (char*)os_zalloc(topic_len+1);
    char *dataBuf = (char*)os_zalloc(data_len+1);

    MQTT_Client* client = (MQTT_Client*)args;

    os_memcpy(topicBuf, topic, topic_len);
    topicBuf[topic_len] = 0;
    INFO("Received topic %s\r\n", topicBuf);

    os_memcpy(dataBuf, data, data_len);
    dataBuf[data_len] = 0;
    INFO("   date=%s\r\n", topicBuf);

    os_free(topicBuf);
    os_free(dataBuf);

} //end mqttDataCb()


/*
 * user_deep_sleep - timer callback to t clean up and rigger deep sleep
 */
static void ICACHE_FLASH_ATTR
user_deep_sleep(void)
{
    INFO("user_deep_sleep()\r\n");
    ds18B20_shutdown();
    als_shutdown();
    battery_shutdown();

    uint32_t usec = system_get_time();
    INFO("elapsed: %d.%03d\r\n", usec/1000000, usec % 1000000);

    system_deep_sleep(DEEP_SLEEP_SECONDS * US_PER_SEC);
}


/*
 * sys_init_complete - callback for sys_init_done
 *
 * The RF calibration is complete, we can proceed with functions that
 * need WiFi access.
 */
static void ICACHE_FLASH_ATTR
sys_init_complete(void)
{
    INFO("sys_init_complete\r\n");
    // Setup Wifi connection and MQTT
    MQTT_InitConnection(&mqttClient, sysCfg.mqtt_host, sysCfg.mqtt_port,
           // sysCfg.security
           0 // 1 = SSL
            );

    INFO("Got here\r\n");
    MQTT_OnConnected(&mqttClient, mqttConnectedCb);
    MQTT_OnDisconnected(&mqttClient, mqttDisconnectedCb);
    MQTT_OnPublished(&mqttClient, mqttPublishedCb);
    MQTT_OnData(&mqttClient, mqttDataCb);

    INFO("Got here 2\r\n");
    // Setup MQTT handling process. The process will be signalled when
    // the WIFI connection is established.
    MQTT_InitClient(
            &mqttClient,
            sysCfg.device_id,
            sysCfg.mqtt_user,
            sysCfg.mqtt_pass,
            sysCfg.mqtt_keepalive,
            1  // 1 = clean session
            );

    INFO("Got here 3\r\n");
    // TODO: the LWT and status don't work the way I think, so 
    //       they are not persistant at this time. I'll fix them
    //       when I know more.
    //os_sprintf(tBuf, "%s/status", sysCfg.device_id);
    //MQTT_InitLWT(&mqttClient, tBuf, "offline",
    //            0,  // QOS
    //            0   // 1 = retain  TODO: set to 1
    //        );
    //MQTT_Publish(
    //            &mqttClient,
    //            tBuf,
    //            "online", 6,  // msg and length
    //            0, // QOS
    //            0  // 1 = retain  TODO: set to 1
    //        );

    WIFI_Connect(sysCfg.sta_ssid, sysCfg.sta_pwd, wifiConnectCb);
    INFO("Got here 4\r\n");

    //dumpInfo();

}  //end of sys_init_complete()


/*
 * initSysCfg
 *
 * This replaces CFG_load() from the mqttAsLib module. I don't
 * understand why the values are written to SPI flash and I'm not clear
 * on what else might be there, so for now
 * hard code the initialization.
 */
SYSCFG sysCfg;
static void ICACHE_FLASH_ATTR
initSysCfg()
{
    sysCfg.cfg_holder = CFG_HOLDER;

    os_sprintf(sysCfg.sta_ssid, "%s", STA_SSID);
    os_sprintf(sysCfg.sta_pwd, "%s", STA_PASS);
    sysCfg.sta_type = STA_TYPE;

    os_sprintf(sysCfg.device_id, MQTT_CLIENT_ID, system_get_chip_id());
    os_sprintf(sysCfg.mqtt_host, "%s", MQTT_HOST);
    sysCfg.mqtt_port = MQTT_PORT;
    os_sprintf(sysCfg.mqtt_user, "%s", MQTT_USER);
    os_sprintf(sysCfg.mqtt_pass, "%s", MQTT_PASS);

    sysCfg.security = DEFAULT_SECURITY;     /* default non ssl */

    sysCfg.mqtt_keepalive = MQTT_KEEPALIVE;

    INFO(" default configuration\r\n");
}  //end of initSysCfg()


/*
 * reporter -  process to collect and send driver results
 *
 * Collect measurements from drivers and when all ready, send them to
 * MQTT broker. Report message has the form:
 *    deviceType,report_version,temperature,lightlevel,voltage,elapsedTime
 *    deviceType = 1 (sensorNode with ds18b20 and isl29035)
 *    version_version = 1
 *       temperature: float, degC
 *       lightlevel:  integer, 1/64 lux per count
 *       voltage: float, volts
 *       elapsedTime: float, seconds
 */
void ICACHE_FLASH_ATTR
reporter(os_event_t *event) {
    driverStatusMask |= (event->sig & 0xff);

    INFO("reporter status: %x, %x\r\n", driverStatusMask, (DRIVER_1 | DRIVER_2 | DRIVER_3));
    if (driverStatusMask == (DRIVER_1 | DRIVER_2 | DRIVER_3)) {
        INFO("Reporting...\r\n");
        // measurements complete, report
        uint32 usec = system_get_time();
        char *timeBuf = (char*)os_zalloc(16);

        // Collect reports
        report_t *driver1 = ds18B20_report();
        report_t *driver2 = als_report();
        report_t *driver3 = battery_report();

        // allocate space for the summary report
        /*
        uint8_t buflen = driver1->len
            + driver2->len
            + driver3->len
            + 2 + SPACE4LOCAL_REPORTS;
        */
        uint8_t buflen = 255;
        char *mBuf = (char*)os_zalloc(buflen);
        char *tBuf = (char *)os_zalloc(strlen(sysCfg.device_id) + 40);

        // fill out the report
        (void)strcpy(mBuf, ID_VERSION_STR);  // deviceID and report version
        (void)strcat(mBuf,",");
        (void)strcat(mBuf,driver1->buffer);  // temperature
        (void)strcat(mBuf,",");
        (void)strcat(mBuf,driver2->buffer);  // ambient light
        (void)strcat(mBuf,",");
        (void)strcat(mBuf,driver3->buffer);  // voltage
        (void)strcat(mBuf,",");

        // elapsed time
        usec = system_get_time();
        os_sprintf(timeBuf, "%d.%3d", usec/1000000, usec % 1000000);
        (void)strcat(mBuf,timeBuf);  // Elapsed Time


        INFO("Used mBuf = %d\r\n", strlen(mBuf));
        // publish the report
        os_sprintf(tBuf, "%s/report", sysCfg.device_id);
        MQTT_Publish(&mqttClient, tBuf, mBuf, os_strlen(mBuf), 0, 1);
        INFO("%s:%s\r\n", tBuf, mBuf);
        /*
        INFO("Got here\r\n");
        */

        // cleanup
        os_free(timeBuf);
        os_free(mBuf);
        os_free(tBuf);
    }
    INFO("Waiting...\r\n");
    return;
} // end reporter()


/*
 * user_init - starting point for user code
 *
 * Initialize modules and callback. Use the sys_init_done callback
 * is the starting point for functionality
 */
void ICACHE_FLASH_ATTR
user_init()
{
    uart_init(BIT_RATE_115200);

    // Setup mqtt configuration, this is a local alternative
    // to the flash based CFG_load/save function in the MQTT library.
    initSysCfg();

    INFO("%s\r\n", sysCfg.device_id);

    // Initialize drivers
    ds18B20_init(REPORTER_PID, DRIVER_1);
    als_init(REPORTER_PID, DRIVER_2);
    battery_init(REPORTER_PID, DRIVER_3);

    // setup timers and processes
    os_timer_disarm(&shutdown_timer);
    os_timer_setfn(&shutdown_timer, (os_timer_func_t *)user_deep_sleep, NULL);


    // watchdog shutdown in 10 seconds (10000 milli-seconds)
    // This limits battery drain if the network or the MQTT server is
    // not available.
    os_timer_disarm(&watchdog_timer);
    os_timer_setfn(&watchdog_timer, (os_timer_func_t *)user_deep_sleep, NULL);
    os_timer_arm(&watchdog_timer, 10000, 0);

    system_os_task(reporter, REPORTER_PID, reporter_queue, REPORTER_QLEN);

    // Set callback to detect system initialization
    system_init_done_cb(sys_init_complete);

} // end of user_init()
