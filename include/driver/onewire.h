/*
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
 */
#ifndef ONEWIRE_H
#define ONEWIRE_H

#define ONEWIRE_PARASITIC_PWR 1
#define ONEWIRE_WIRED_PWR 0

void ICACHE_FLASH_ATTR ds_init(int power);
void ICACHE_FLASH_ATTR ds_reset(void);
void ICACHE_FLASH_ATTR ds_write(uint8_t cmd);
uint8_t ICACHE_FLASH_ATTR ds_read(void);

#endif

