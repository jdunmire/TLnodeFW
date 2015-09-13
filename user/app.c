/*
 * app.c - toplevel for SensorNode
 *
 * Reads sensors and reports using MQTT
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
//#ifndef INFO
//#endif
// Disable debugging messages
//#define INFO //os_printf
#include "debug.h"

#include <ets_sys.h>
#include <driver/uart.h>
#include <osapi.h>
#include <os_type.h>
#include <gpio.h>
#include <mem.h>
#include "mqtt.h"
#include "config.h"
#include "user_config.h"
#include "ds18b20.h"
#include "als.h"
#include "report.h"

// Device ID = 1, Application version = 1
#define ID_VERSION_STR  "1,1"

#define DEEP_SLEEP_SECONDS 300
#define US_PER_SEC 1000000

static os_timer_t shutdown_timer;

#define REPORTER_PID    0   // 0-2, low to high
#define REPORTER_QLEN   2   // allow space for both drivers to report
#define DRIVER_1        0x01
#define DRIVER_2        0x02
// system_os_post(REPORTER_PID, driverID, driverStatus );

static os_event_t       reporter_queue[user_procTaskQueueLen];
static uint8_t          driverStatusMask = 0;

MQTT_Client mqttClient;


/*
 * This gets called when the connection to the WiFi AP
 * changes.
 *
 */
void ICACHE_FLASH_ATTR
wifiConnectCb(uint8_t status)
{
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

static int done = 0;

/*
 * handle MQTT published event
 *
 */
void ICACHE_FLASH_ATTR
mqttPublishedCb(uint32_t *args)
{
    MQTT_Client* client = (MQTT_Client*)args;
    char *mBuf = (char*)os_zalloc(20);
    char *tBuf = (char *)os_zalloc(strlen(sysCfg.device_id) + 40);
    uint32 usec = system_get_time();

    INFO("ds= %d, als=%d\r\n",
            ds18B20_is_complete(), als_is_complete());

    if ((ds18B20_is_complete() == true)
            && (als_is_complete() == true)
       )
    {
        INFO("MQTT: Measurements published\r\n");
       // INFO("app: queue fill_cnt= %d\r\n", client->msgQueue.rb.fill_cnt);
       // INFO("app: queue size= %d\r\n", client->msgQueue.rb.size);
       // INFO("app: time elapsed = %d.%d s\r\n", usec / 1000000, usec % 1000000);
        if (client->msgQueue.rb.fill_cnt == 0) {
            if (done == 0) {
                done = 1;
                os_sprintf(mBuf, "%d.%03d", usec/1000000, usec % 1000000);
                os_sprintf(tBuf, "%s/elapsedTime", sysCfg.device_id);

                MQTT_Publish(&mqttClient, tBuf, mBuf, os_strlen(mBuf), 0, 1);
                INFO("MQTT: publishing timestamp\r\n");
            }
            else
            { 
                os_timer_arm(&shutdown_timer, 1, 0);
            }
        }
        else
        {
            INFO("Waiting for queue to drain...\r\n");
        }
    }
    os_free(mBuf);
    os_free(tBuf);
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
 * user_deep_sleep - timer callback to trigger deep sleep
 */
static void ICACHE_FLASH_ATTR
user_deep_sleep(void)
{
    ds18B20_shutdown();
    als_shutdown();

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
    char *tBuf = (char *)os_zalloc(strlen(sysCfg.device_id) + 40);
    char *mBuf = (char*)os_zalloc(20);

    uint32 voltageRaw = system_get_vdd33();

    // Setup Wifi connection and MQTT
    MQTT_InitConnection(&mqttClient, sysCfg.mqtt_host, sysCfg.mqtt_port,
           // sysCfg.security
           0 // 1 = SSL
            );

    MQTT_OnConnected(&mqttClient, mqttConnectedCb);
    MQTT_OnDisconnected(&mqttClient, mqttDisconnectedCb);
    MQTT_OnPublished(&mqttClient, mqttPublishedCb);
    MQTT_OnData(&mqttClient, mqttDataCb);

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

    os_sprintf(tBuf, "%s/voltage", sysCfg.device_id);
    os_sprintf(mBuf, "%d.%03d",
            voltageRaw / 1024,
            (voltageRaw % 1024) * 1000 / 1024);
    MQTT_Publish(
            &mqttClient,
            tBuf,
            mBuf, os_strlen(mBuf),  // msg and length
            0, // QOS
            1  // 1 = retain
            );
    os_free(tBuf);

    WIFI_Connect(sysCfg.sta_ssid, sysCfg.sta_pwd, wifiConnectCb);

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

    if (driverStatusMask == (DRIVER_1 & DRIVER_2)) {
        // measurements complete, report
        report_t *driver1 = ds18b20_report();
        report_t *driver2 = als_report();
        uint8_t buflen = driver1->len + driver2->len + 1 + SPACE4LOCAL_REPORTS;
        char *mBuf = (char*)os_zalloc(bufLen);

        // DeviceID and application version
        (void)strcpy(mBuf, ID_VERSION_STR);
        (void)strcat(mBuf,",");
        (void)strcat(mBuf,driver1->buffer);
        (void)strcat(mBuf,",");
        (void)strcat(mBuf,driver2->buffer);
        (void)strcat(mBuf,",");
        // voltage
        (void)strcat(mBuf,",");
        // elapsed time
    }
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
    ds18B20_init(DRIVER_1);
    als_init(DRIVER_2);

    // setup timers and processes
    os_timer_disarm(&shutdown_timer);
    os_timer_setfn(&shutdown_timer, (os_timer_func_t *)user_deep_sleep, NULL);

    system_os_task(reporter, REPORTER_PID, reporter_queue, REPORTER_QLEN);

    // Set callback to detect system initialization
    system_init_done_cb(sys_init_complete);

} // end of user_init()
