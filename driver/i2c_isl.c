/*
 *  I2C driver for the ISL29035 Integrated Digital Light Sensor
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
#include "driver/i2c.h"
#include "driver/i2c_isl.h"

void ICACHE_FLASH_ATTR
isl_write_byte(uint8 addr, uint8 data) {
    i2c_start();
    i2c_writeByte(ISL_WRITE_ADDR);
    (void)i2c_check_ack();
    i2c_writeByte(addr);
    (void)i2c_check_ack();
    i2c_writeByte(data);
    (void)i2c_check_ack();
    i2c_stop();
} //end isl_write_byte()


uint8 ICACHE_FLASH_ATTR
isl_read_byte(uint8 addr) {
    uint8 data;
    i2c_start();

    i2c_writeByte(ISL_WRITE_ADDR);
    (void)i2c_check_ack();

    i2c_writeByte(addr);
    (void)i2c_check_ack();

    i2c_start();
    i2c_writeByte(ISL_READ_ADDR);
    (void)i2c_check_ack();

    data = i2c_readByte();
    i2c_stop();

    return(data);

} //end isl_read_byte();


uint16 ICACHE_FLASH_ATTR
isl_read_word(uint8 addr) {
    uint8 lsb;
    uint8 msb;
    uint16 data;
    i2c_start();

    i2c_writeByte(ISL_WRITE_ADDR);
    (void)i2c_check_ack();

    i2c_writeByte(addr);
    (void)i2c_check_ack();

    i2c_start();
    i2c_writeByte(ISL_READ_ADDR);
    (void)i2c_check_ack();

    lsb = i2c_readByte();
    i2c_send_ack(1);

    msb = i2c_readByte();

    i2c_stop();
    //os_printf("msb = %0x, lsb = %0x, data = %0x\n\r", msb, lsb, data);
    data = (msb << 8) | lsb;

    return(data);

} //end isl_read_word()


