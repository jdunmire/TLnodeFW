/*
 * app.c - toplevel for SensorNode
 *
 * Reads sensors and reports using MQTT
 *
 */
#ifndef INFO
#define INFO os_printf
#endif

#include <ets_sys.h>
#include <driver/uart.h>
#include <osapi.h>
#include <os_type.h>
#include "mem.h"
#include "mqtt.h"
#include "config.h"
#include "user_config.h"
#include "driver/onewire.h"

// Enable debugging messages

#define MEASUREMENT_US 750000    // max time from datasheet for 12 bits
#define DEEP_SLEEP_SECONDS 10
#define US_PER_SEC 1000000

static void ICACHE_FLASH_ATTR measurementReady(void);

static uint32 measurement_start_time;
static os_timer_t read_timer;
static os_timer_t shutdown_timer;

MQTT_Client mqttClient;


/*
 * This gets called when the connection to the WiFi AP
 * changes.
 *
 */
void ICACHE_FLASH_ATTR
wifiConnectCb(uint8_t status)
{
    if(status == STATION_GOT_IP){
        MQTT_Connect(&mqttClient);
        // The INFO message will be 'TCP: Connect to...'
    } else {
        MQTT_Disconnect(&mqttClient);
        // The INFO message will be 'Free memory'
    }
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
    INFO("MQTT: Published\r\n");
    INFO("app: queue fill_cnt= %d\r\n", client->msgQueue.rb.fill_cnt);
    INFO("app: queue size= %d\r\n", client->msgQueue.rb.size);
    
    uint32 usec = system_get_time();
    INFO("app: time elapsed = %d.%d s\r\n", usec / 1000000, usec % 1000000);

    if (client->msgQueue.rb.fill_cnt == 0) {
        if (done == 0) {
            done = 1;
            os_sprintf(mBuf, "%d.%03d", usec/1000000, usec % 1000000);
            os_sprintf(tBuf, "%s/elapsedTime", sysCfg.device_id);

            MQTT_Publish(&mqttClient, tBuf, mBuf, strlen(mBuf), 0, 0);
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
    INFO("JERRY MQTT: Connected\r\n");
    
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
        measurementReady();
    }

    //TODO: add other measurements

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
 * read temperature from DS18S20
 *
 * Reads last measurement from the scratch pad, reports
 * it to MQTT and via a debug (INFO) message.
 */
static void ICACHE_FLASH_ATTR
readTemp(void)
{
    uint16_t tb;
    int16_t  temperature;
    int16_t mantissa;
    char *mBuf = (char*)os_zalloc(20);
    char *tBuf = (char *)os_zalloc(strlen(sysCfg.device_id) + 40);

    // Read previous measurement
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

    INFO("\r\n");
    INFO(mBuf);
    INFO("\r\n");
    MQTT_Publish(&mqttClient, tBuf, mBuf, strlen(mBuf), 0, 0);

    os_sprintf(tBuf, "%s/degF", sysCfg.device_id);
    if ((temperature == 0) && (mantissa < 0))
    {
        os_sprintf(mBuf, "-%d", (((temperature * 9)/5) + 32));
    }
    else
    {
        os_sprintf(mBuf, "%d", (((temperature * 9)/5) + 32));
    }

    INFO("\r\n");
    INFO(mBuf);
    INFO("\r\n");
    MQTT_Publish(&mqttClient, tBuf, mBuf, strlen(mBuf), 0, 0);


    os_free(mBuf);
    os_free(tBuf);

} //end readTemp(void)


/*
 * startTempMeasurement - tell DS18B20 to start measurement
 *
 * The system time is recorded so that we can later assure that enough
 * time has elapsed for the measurement to complete.
 */
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


/*
 * user_deep_sleep - timer callback to trigger deep sleep
 */
static void ICACHE_FLASH_ATTR
user_deep_sleep(void)
{
    system_deep_sleep(DEEP_SLEEP_SECONDS * US_PER_SEC);
}

/*
 * measurementReady - timer call back to read the temperature
 *
 * This may be called directly or as a timer callback to assure that
 * enough time has elapsed for the measurement to be complete.
 */
static void ICACHE_FLASH_ATTR
measurementReady(void)
{
    readTemp();
    // TODO: report the temperature to MQTT
    //MQTT_Subscribe(client, "temperature/report", 1);
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
    // buffer for status and LWT message
    char *tBuf = (char *)os_zalloc(strlen(sysCfg.device_id) + 40);

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
    os_sprintf(tBuf, "%s/status", sysCfg.device_id);
    MQTT_InitLWT(&mqttClient, tBuf, "offline",
                0,  // QOS
                0   // 1 = retain  TODO: set to 1
            );
    MQTT_Publish(
                &mqttClient,
                tBuf,
                "online", 6,  // msg and length
                0, // QOS
                0  // 1 = retain  TODO: set to 1
            );
    os_free(tBuf);

    WIFI_Connect(sysCfg.sta_ssid, sysCfg.sta_pwd, wifiConnectCb);

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
 * user_init - starting point for user code
 *
 * Initialize modules and callback. Use the sys_init_done callback
 * is the starting point for functionality
 */
void ICACHE_FLASH_ATTR
user_init()
{
    uart_init(BIT_RATE_115200);

    // I thought the board was designed for WIRED power, but there is a
    // design error on the board or in the onewire code. 2015/08/18
    //ds_init(ONEWIRE_WIRED_PWR);
    ds_init(ONEWIRE_PARASITIC_PWR);

    // Setup mqtt configuration, this is a local alternative
    // to the flash based CFG_load/save function in the MQTT library.
    initSysCfg();

    os_timer_disarm(&shutdown_timer);
    os_timer_setfn(&shutdown_timer, (os_timer_func_t *)user_deep_sleep, NULL);

    os_timer_disarm(&read_timer);
    os_timer_setfn(&read_timer, (os_timer_func_t *)measurementReady, NULL);
    startTempMeasurement();

    system_init_done_cb(sys_init_complete);

} // end of user_init()
