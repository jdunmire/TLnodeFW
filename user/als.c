/*
 *  Ambient light sensor support routines
 *
 *  The light sensor is the ISL29035. Reported values are in 1/64 lux.
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
//#define INFO os_printf  // override debug.h
#include "debug.h"
#include "report.h"

#include "als.h"

/*
 * Datasheet gives 105ms for 16-bit integration time. Since the ADC is
 * dual-slope, the total measurement time could be double that for full
 * scale.
 */
#define MEASUREMENT_US (105000 * 2)
                                    

static uint32_t reportPID = 0;
static uint32_t myid = 0;
static report_t *myReport;

static os_timer_t read_timer;
static uint32_t measurement_start_time;

static uint16_t Lux = 0;

/*
 * read light level from isl92035
 *
 */
static uint16_t ICACHE_FLASH_ATTR
readData16(void)
{
    uint16_t lux = 0;
    uint16_t lux_check = 1;
    int maxloops = 5;

    lux = isl_read_word(ISL_DATA_REG);
    lux_check = isl_read_word(ISL_DATA_REG);

    // make sure the reading didn't change between LSB/MSB reads
    while ((lux != lux_check) && (maxloops >= 0)) {
        lux = lux_check;
        maxloops -= 1;
        lux_check = isl_read_word(ISL_DATA_REG);
    }
    return (lux);

} //end readData16();


/*
 * read light levels
 *    - a state machine to read ambient level in full
 *      dynamic range.
 *    - measurements will be reported in 1/64 lux per bit.
 *    - called from a timer that allows time for each read.
 */
enum alsState_t {
    als_ranging,
    als_ready,
};

static enum alsState_t alsState = als_ranging;
static uint8_t Range = ISL_RANGE_64K;

/*
 * Gains from ISL2905gainAnalysis.ods, Theoretical (K13-K16)
 */
static uint8_t Gain[4] = {1, 4, 15, 61};

static void ICACHE_FLASH_ATTR
readLightLevels(void)
{
    uint32_t mtime = MEASUREMENT_US / 1000;

    Lux = readData16();
    switch(alsState) {
        case als_ranging:
            INFO("Ranging starting at %d, lux = %d\r\n", Range, Lux);
            // check range and re-measure if necessary
            // cut-off points are based on ISL29035gainAnalysis.ods
            // and allow for gain variation between range settings.
            alsState = als_ready;
            if (Lux < 14000) {
                Range = ISL_RANGE_16K;
                if (Lux < 880) {
                    Range = ISL_RANGE_1K;
                } else if (Lux < 3500) {
                    Range = ISL_RANGE_4K;
                }
                INFO("Change range to %d\r\n", Range);
                isl_write_byte(ISL_CMD2_REG, (Range | ISL_ADC_16_BIT));
                isl_write_byte(ISL_CMD1_REG, ISL_MODE_ALS_CONT);
                os_timer_arm(&read_timer, mtime, 0);
                break;
            } else {
                // range is OK, no need to re-read.
                // Fall through to the als_ready state.
            }
            // Conditional breaks above.
            // Fall through otherwise.

        case als_ready:
            INFO("ALS ready ...\r\n");
            system_os_post(reportPID, myid, 0);
            break;

        default:
            os_printf("State error in readLightLevels()\r\n");
            break;

    } // end switch(alsState)

} // end readLightLevels()


/*
 * startTempMeasurement - tell isl29035 to start measurement
 *
 * The system time is recorded so that we can later assure that enough
 * time has elapsed for the measurement to complete.
 */
void ICACHE_FLASH_ATTR
als_init(uint32_t pid, uint32_t id)
{
    reportPID = pid;
    myid = id;

    INFO("als_init()\r\n");
    i2c_init();
    // Start the light sensor measurements.
    Range = ISL_RANGE_64K;
    isl_write_byte(ISL_CMD2_REG, (Range | ISL_ADC_16_BIT));
    isl_write_byte(ISL_CMD1_REG, ISL_MODE_ALS_CONT);

    measurement_start_time = system_get_time();

    // Setup up the timer, but don't start it
    os_timer_disarm(&read_timer);
    os_timer_setfn(&read_timer, (os_timer_func_t *)readLightLevels, NULL);

    return;
} // end als_init()


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
    uint32_t elapsed_us = system_get_time() - measurement_start_time;
    if (elapsed_us < MEASUREMENT_US) {
        INFO("Delaying %d us\r\n", MEASUREMENT_US - elapsed_us);
        os_timer_arm(&read_timer, (MEASUREMENT_US - elapsed_us) / 1000 , 0);
    }
    else
    {
        INFO("Skipping delay for als\r\n");
        readLightLevels();
    }

} //end als_start()


/*
 * format light level for reporting process
 *
 */
report_t* ICACHE_FLASH_ATTR
als_report(void)
{
    myReport = newReport(12);
    os_sprintf(myReport->buffer, "%d", Lux);
    myReport->len = strlen(myReport->buffer);

    return(myReport);

} //end als_report(void)


void ICACHE_FLASH_ATTR
als_shutdown(void)
{
    INFO("als_shutdown()\r\n");
    // power down the light sensor
    isl_write_byte(ISL_CMD1_REG, ISL_MODE_PD);
    freeReport(myReport);

    return;
}  //end als_shutdown()
