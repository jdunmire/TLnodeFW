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
#ifndef DS18B20_H
#define DS18B20_H
#include <report.h>

void ds18B20_init(uint32_t);
void ds18B20_start(void);
bool ds18B20_is_complete(void);  // obsolete
report_t* ds18B20_report(void);
void ds18B20_shutdown(void);

#endif
