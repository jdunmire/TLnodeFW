/*
 *  Report structures
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
#ifndef REPORT_H
#define REPORT_H

typedef struct report_s {
    uint8_t len;    // length of null terminated string in buffer
    uint8_t bsize;  // buffer size
    char *  buffer; // buffer
} report_t;

report_t *newReport(uint8_t bsize);
void freeReport(report_t *report);

#endif
