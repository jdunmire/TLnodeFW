#ifndef __I2C_ISL_H__
#define __I2C_ISL_H__

/*
    I2C driver for the ISL29035 Integrated Digital Light Sensor

    Copyright (C) 2015 Jerry Dunmire
    This file is part of sensorNode

    sensorNode is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    sensorNode is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with sensorNode.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "ets_sys.h"
#include "osapi.h"
#include "gpio.h"

/*
 * Device address
 */
#define ISL_WRITE_ADDR 0b10001000
#define ISL_READ_ADDR  0b10001001

/*
 * Register addresses
 */
#define ISL_CMD1_REG        0x00
#define ISL_CMD2_REG        0x01
#define ISL_DATA_REG        0x02
#define ISL_DATA2_REG       0x03
#define ISL_INT_LT_LSB_REG  0x04
#define ISL_INT_LT_MSB_REG  0x05
#define ISL_INT_HT_LSB_REG  0x06
#define ISL_INT_HT_MSB_REG  0x07
#define ISL_ID_REG          0x0f

/*
 * ISL_CMD1_REG values
 */
#define ISL_MODE_PD         0x00
#define ISL_MODE_ALS_ONCE   0x20
#define ISL_MODE_IR_ONCE    0x40
#define ISL_MODE_ALS_CONT   0xa0
#define ISL_MODE_IR_CONT    0xc0

#define ISL_INTG_CYCLES_1   0x00
#define ISL_INTG_CYCLES_4   0x01
#define ISL_INTG_CYCLES_8   0x02
#define ISL_INTG_CYCLES_16  0x03

#define ISL_INTR_MASK       0x04

/*
 * ISL_CMD2_REG values
 */
#define ISL_RANGE_1K        0x00
#define ISL_RANGE_4K        0x01
#define ISL_RANGE_16K       0x02
#define ISL_RANGE_65K       0x03

#define ISL_ADC_16_BIT      0x00
#define ISL_ADC_12_BIT      0x04
#define ISL_ADC_8_BIT       0x08
#define ISL_ADC_4_BIT       0x0c

void isl_write_byte(uint8 addr, uint8 data);
uint8 isl_read_byte(uint8 addr);
uint16 isl_read_word(uint8 addr);

#endif
