/*
 * This is minimalist implementation of a 1-wire master on an ESP8266
 *
 * Only a single slave is supported and enumeration is not implemented.
 *
 * This is a cleaned up version of the code published in Peter Scargill's
 * blog: http://tech.scargill.net/esp8266-and-the-dallas-ds18b20-and-ds18b20p/
 *
 * Which is itself an adaptation of Paul Stoffregen’s One wire library
 * for the Arduino: http://www.pjrc.com/teensy/td_libs_OneWire.html
 * 
 * Copyright 2015 Jerry Dunmire
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of version 3 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Copyright and license information for earlier versions is at the end
 * of this file.
 *
 */
#include <ets_sys.h>
#include <gpio.h>
#include <osapi.h>
#include "driver/onewire.h"

// If this define is changed, then associated *_GPIO5* macros below must
// also be changed.
// NOTE: GPIO5 is labeled as GPIO4 on the ESP-12 module
#define ONEWIRE_PIN 5

static int DS_Power = ONEWIRE_PARASITIC_PWR;

/*
 * Initialize GPIO for one-wire use
 *
 * This must be called during initialization and before any other ds_*
 * functions.
 *
 * The writing code uses the active drivers to raise the
 * pin high, if you need power after the write (e.g. DS18S20 in
 * parasite power mode) then set ‘power’ to ONEWIRE_PARASITIC_PWR,
 * otherwise the pin will go tri-state at the end of the write to avoid
 * heating in a short or other mishap.
 *
 */
void ICACHE_FLASH_ATTR ds_init(int power)
{
    // Record how the device is powered
    DS_Power = power;

    //disable pulldown
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO5_U, FUNC_GPIO5);

    //enable pull up R - PIN_PULLDWN_DIS dropped in SDK v1.3.0
    //PIN_PULLDWN_DIS(PERIPHS_IO_MUX_GPIO5_U);

    // Configure the GPIO with internal pull-up
    PIN_PULLUP_EN(PERIPHS_IO_MUX_GPIO5_U);
    
    // tri-state the pin
    GPIO_DIS_OUTPUT(ONEWIRE_PIN);

} // end ds_init()


/*
 * one-wire reset function
 *
 * Perform the onewire reset function. We will wait up to 250uS for
 * the bus to come high, if it doesn’t then it is broken or shorted
 * and we return;
 */
void ICACHE_FLASH_ATTR ds_reset(void)
{
    uint8_t retries = 125;
    GPIO_DIS_OUTPUT(ONEWIRE_PIN);
    // wait until the wire is high… just in case
    do {
        if (--retries == 0)
        {
            return;
        }
        os_delay_us(2);
    } while (!GPIO_INPUT_GET(ONEWIRE_PIN));

    GPIO_OUTPUT_SET(ONEWIRE_PIN, 0);
    os_delay_us(480);
    GPIO_DIS_OUTPUT(ONEWIRE_PIN);
    os_delay_us(480);

} // end ds_reset(void)


/*
 * Write a bit.
 *
 * Inlined for more certain timing. Interrupts remain enabled so that
 * os_delay_us() can be used for timing.
 */
static inline void write_bit( int bit )
{
    GPIO_OUTPUT_SET(ONEWIRE_PIN, 0);
    if( bit )
    {
        os_delay_us(10);
        GPIO_OUTPUT_SET(ONEWIRE_PIN, 1);
        os_delay_us(55);
    }
    else
    {
        os_delay_us(65);
        GPIO_OUTPUT_SET(ONEWIRE_PIN, 1);
        os_delay_us(5);
    }

} // end write_bit()


/*
 * Write a byte.
 *
 */
void ICACHE_FLASH_ATTR ds_write(uint8_t cmd)
{
    uint8_t bitMask;
    for (bitMask = 0x01; bitMask; bitMask <<= 1)
    {
        write_bit((bitMask & cmd) ? 1 : 0);
    }

    if (DS_Power != ONEWIRE_PARASITIC_PWR)
    {
        GPIO_DIS_OUTPUT( ONEWIRE_PIN );
        GPIO_OUTPUT_SET( ONEWIRE_PIN, 0 );
    }

} // end ds_write()


/*
 * Read a bit.
 *
 * Inlined for more certain timing. Interrupts remain enabled so that
 * os_delay_us() can be used for timing.
 */
static inline int read_bit(void)
{
    int r;
    GPIO_OUTPUT_SET(ONEWIRE_PIN, 0);
    os_delay_us(3);
    GPIO_DIS_OUTPUT(ONEWIRE_PIN);
    os_delay_us(10);
    r = GPIO_INPUT_GET(ONEWIRE_PIN);
    os_delay_us(53);

    return r;
} // read_bit(void)


/*
 * Read a byte
 */
uint8_t ICACHE_FLASH_ATTR ds_read(void)
{
    uint8_t bitMask;
    uint8_t r = 0;
    for (bitMask = 0x01; bitMask; bitMask <<= 1)
    {
        if (read_bit())
        {
            r |= bitMask;
        }
    }
    return r;

} // end ds_read()


/*
    Related copyright and license notices
    -------------------------------------
    Copyright (c) 2007, Jim Studt  (original old version - many contributors since)

    The latest version of this library may be found at:
      http://www.pjrc.com/teensy/td_libs_OneWire.html

    Jim Studt's original library was modified by Josh Larios.

    Tom Pollard, pollard@alum.mit.edu, contributed around May 20, 2008

    Permission is hereby granted, free of charge, to any person obtaining
    a copy of this software and associated documentation files (the
    "Software"), to deal in the Software without restriction, including
    without limitation the rights to use, copy, modify, merge, publish,
    distribute, sublicense, and/or sell copies of the Software, and to
    permit persons to whom the Software is furnished to do so, subject to
    the following conditions:

    The above copyright notice and this permission notice shall be
    included in all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
    EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
    MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
    NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
    LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
    OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
    WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

    Much of the code was inspired by Derek Yerger's code, though I don't
    think much of that remains.  In any event that was..
        (copyleft) 2006 by Derek Yerger - Free to distribute freely.

    The CRC code was excerpted and inspired by the Dallas Semiconductor
    sample code bearing this copyright.
    //---------------------------------------------------------------------------
    // Copyright (C) 2000 Dallas Semiconductor Corporation, All Rights Reserved.
    //
    // Permission is hereby granted, free of charge, to any person obtaining a
    // copy of this software and associated documentation files (the "Software"),
    // to deal in the Software without restriction, including without limitation
    // the rights to use, copy, modify, merge, publish, distribute, sublicense,
    // and/or sell copies of the Software, and to permit persons to whom the
    // Software is furnished to do so, subject to the following conditions:
    //
    // The above copyright notice and this permission notice shall be included
    // in all copies or substantial portions of the Software.
    //
    // THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
    // OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
    // MERCHANTABILITY,  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
    // IN NO EVENT SHALL DALLAS SEMICONDUCTOR BE LIABLE FOR ANY CLAIM, DAMAGES
    // OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
    // ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
    // OTHER DEALINGS IN THE SOFTWARE.
    //
    // Except as contained in this notice, the name of Dallas Semiconductor
    // shall not be used except as stated in the Dallas Semiconductor
    // Branding Policy.
    //--------------------------------------------------------------------------
*/
