/*
 *  Report object
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
#include <os_type.h>
#include <mem.h>

#include "report.h"


/*
 * newReport - allocate an empty report structure
 *
 */
report_t * ICACHE_FLASH_ATTR
newReport(uint8_t bsize) {
    report_t *report = (report_t *)os_zalloc(sizeof(report_t));
    report->buffer = (char *)os_zalloc(bsize);
    report->buffer[0] = '\0';  // null terminator
    report->len = 0;
    report->bsize = bsize;

    return(report);
} // end of newReport()


/*
 * freeReport - free the allocated report space
 *
 */
void ICACHE_FLASH_ATTR
freeReport(report_t * report) {
    if (report != NULL)
    {
        os_free(report->buffer);
        os_free(report);
    }
    return;
} // end of freeReport()
