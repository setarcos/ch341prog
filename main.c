/*
 * This file is part of the ch341prog project.
 *
 * Copyright (C) 2014 Pluto Yang (yangyj.ee@gmail.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */

#include <libusb-1.0/libusb.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "ch341a.h"

int main(int argc, char* argv[])
{
    struct libusb_device_handle *devHandle = NULL;
    uint32_t ret;
    uint8_t buf[8888];

    devHandle = ch341Configure(CH341A_USB_VENDOR, CH341A_USB_PRODUCT);
    if (devHandle == NULL)
        return -1;
    ret = ch341SetStream(devHandle, 1);
    if (ret < 0) goto out;
    //ch341SpiCapacity(devHandle);
    //ret = ch341EraseChip(devHandle);
    //for (int i = 0; i < 256; ++i) buf[i] = i;
    //ret = ch341SpiWrite(devHandle, buf, 0, 512);
    memset(buf, 1, 1024);
    ret = ch341SpiRead(devHandle, buf, 0, 1024);
    for (int i = 0; i < 1024; ++i) {
        if (i % 32 == 0) printf("\n");
        printf("%02x ", buf[i]);
    }
    printf("\n");
out:
    ch341Release(devHandle);
    return 0;
}
