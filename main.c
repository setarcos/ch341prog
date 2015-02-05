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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <getopt.h>
#include "ch341a.h"

int main(int argc, char* argv[])
{
    int32_t ret;
    uint8_t *buf;
    FILE *fp;
    char *filename;
    int cap;
    int length = 0;
    char op = 0;
    int8_t c;

    const char usage[] =
        "\nUsage:\n"\
        " -h, --help             display this message\n"\
        " -i, --info             read the chip ID info\n"\
        " -e, --erase            erase the entire chip\n"\
        " -l, --length <bytes>   manualy set length\n"\
        " -w, --write <filename> write chip with data from filename\n"\
        " -r, --read <filename>  read chip and save data to filename\n";
    const struct option options[] = {
        {"help",    no_argument,        0, 'h'},
        {"erase",   no_argument,        0, 'e'},
        {"write",   required_argument,  0, 'w'},
        {"length",   required_argument,  0, 'l'},
        {"read",    required_argument,  0, 'r'},
        {0, 0, 0, 0}};

        int32_t optidx = 0;

        while ((c = getopt_long(argc, argv, "hiew:r:l:", options, &optidx)) != -1){
            switch (c) {
                case 'i':
                case 'e':
                    if (!op)
                        op = c;
                    else
                        op = 'x';
                    break;
                case 'w':
                case 'r':
                    if (!op) {
                        op = c;
                        filename = (char*) malloc(strlen(optarg) + 1);
                        strcpy(filename, optarg);
                    } else
                        op = 'x';
                    break;
		case 'l':
		    length = atoi(optarg);
		    break;
                default:
                    printf("%s\n", usage);
                    return 0;
            }
    }
    if (op == 0) {
        fprintf(stderr, "%s\n", usage);
        return 0;
    }
    if (op == 'x') {
        fprintf(stderr, "Conflicting options, only one option a time.\n");
        return -1;
    }
    ret = ch341Configure(CH341A_USB_VENDOR, CH341A_USB_PRODUCT);
    if (ret < 0)
        return -1;
    ret = ch341SetStream(CH341A_STM_I2C_100K);
    if (ret < 0) goto out;
    ret = ch341SpiCapacity();
    if (ret < 0) goto out;
    cap = 1 << ret;
    printf("Chip capacity is %d\n", cap);

    if (length != 0){
	cap = length;
    }
    if (op == 'i') goto out;
    if (op == 'e') {
        uint8_t timeout = 0;
        ret = ch341EraseChip();
        if (ret < 0) goto out;
        do {
            sleep(1);
            ret = ch341ReadStatus();
            if (ret < 0) goto out;
            printf(".");
            fflush(stdout);
            timeout++;
            if (timeout == 100) break;
        } while(ret != 0);
        if (timeout == 100)
            fprintf(stderr, "Chip erase timeout.\n");
        else
            printf("Chip erase done!\n");
    }
    if ((op == 'r') || (op == 'w')) {
        buf = (uint8_t *)malloc(cap);
        if (!buf) {
            fprintf(stderr, "Malloc failed for read buffer.\n");
            goto out;
        }
    }
    if (op == 'r') {
        ret = ch341SpiRead(buf, 0, cap);
        if (ret < 0)
            goto out;
        fp = fopen(filename, "wb");
        if (!fp) {
            fprintf(stderr, "Couldn't open file %s for writing.\n", filename);
            goto out;
        }
        fwrite(buf, 1, cap, fp);
        if (ferror(fp))
            fprintf(stderr, "Error writing file [%s]\n", filename);
        fclose(fp);
    }
    if (op == 'w') {
        fp = fopen(filename, "rb");
        if (!fp) {
            fprintf(stderr, "Couldn't open file %s for reading.\n", filename);
            goto out;
        }
        ret = fread(buf, 1, cap, fp);
        if (ferror(fp)) {
            fprintf(stderr, "Error reading file [%s]\n", filename);
            if (fp)
                fclose(fp);
            goto out;
        }
        fclose(fp);
        ret = ch341SpiWrite(buf, 0, cap);
    }
out:
    ch341Release();
    return 0;
}
