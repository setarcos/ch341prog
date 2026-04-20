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
#include <time.h>
#include <stdio.h>

int verbose;

void v_print(int mode, int len) { // mode: begin=0, progress = 1
    if (!verbose) return ;

    static unsigned int size = 0;
    static time_t started,reported;
    unsigned int dur,done;
    time_t now;
    time(&now);

    switch (mode) {
        case 0: // setup
            size = len;
            started = reported = now;
            break;
        case 1: // progress
            if (now == started ) return ;

            dur = now - started;
            done = size-len;
            if (done > 0 && reported != now) {
                printf("Bytes: %d (%d%c),  Time: %d, ETA: %d   \r",done,
                        (done * 100) / size, '%', dur, (int) ((1.0 * dur * size) / done-dur));
                fflush(stdout);
                reported = now;
            }
            break;
        case 2: // done
            dur = now - started; if (dur<1) dur=1;
            printf("Total:  %d sec,  average speed  %d  bytes per second.\n", dur, size / dur);
            break;
        default:
            break;
    }
}

int main(int argc, char* argv[])
{
    int32_t ret;
    int exitcode = 0;
    uint8_t *buf;
    FILE *fp;
    char *filename;
    int cap;
    int length = 0;
    char op = 0;
    uint32_t speed = CH341A_STM_I2C_20K;
    int8_t c;
    int offset = 0;
    int sec_page = -1;
    char sec_op = 0;

    const char usage[] =
        "\nUsage:\n"\
        " -h, --help             display this message\n"\
        " -i, --info             read the chip ID info\n"\
        " -u, --unlock           unlock block protection\n"\
        " -e, --erase            erase the entire chip\n"\
        " -v, --verbose          print verbose info\n"\
        " -l, --length <bytes>   manually set length\n"\
        " -w, --write <filename> write chip with data from filename\n"\
        " -o, --offset <bytes>   write data starting from specific offset\n"\
        " -r, --read <filename>  read chip and save data to filename\n"\
        " -t, --turbo            increase the i2c bus speed (-tt to use much faster speed)\n"\
        " -d, --double           double the spi bus speed\n"\
        "\nSecurity Register commands:\n"\
        " -S, --read-secreg <page>   read security register page (0-3)\n"\
        " -W, --write-secreg <page>  write file to security register page (1-3)\n"\
        " -E, --erase-secreg <page>  erase security register page (1-3)\n"\
        " -L, --lock-secreg <page>   OTP-lock security register page (1-3) IRREVERSIBLE!\n"\
        " -D, --dump-secreg          dump all security register pages with lock status\n";
    const struct option options[] = {
        {"help",    no_argument,        0, 'h'},
        {"info",    no_argument,        0, 'i'},
        {"erase",   no_argument,        0, 'e'},
        {"write",   required_argument,  0, 'w'},
        {"length",  required_argument,  0, 'l'},
        {"verbose", no_argument,        0, 'v'},
        {"write",   required_argument,  0, 'w'},
        {"offset",  required_argument,  0, 'o'},
        {"read",    required_argument,  0, 'r'},
        {"turbo",   no_argument,        0, 't'},
        {"double",  no_argument,        0, 'd'},
        {"unlock",  no_argument,        0, 'u'},
        {"read-secreg",  required_argument, 0, 'S'},
        {"write-secreg", required_argument, 0, 'W'},
        {"erase-secreg", required_argument, 0, 'E'},
        {"lock-secreg",  required_argument, 0, 'L'},
        {"dump-secreg",  no_argument,       0, 'D'},
        {0, 0, 0, 0}};

        int32_t optidx = 0;

        while ((c = getopt_long(argc, argv, "uhiew:r:l:tdvo:S:W:E:L:D", options, &optidx)) != -1){
            switch (c) {
                case 'i':
                case 'e':
                    if (!op)
                        op = c;
                    else
                        op = 'x';
                    break;
                case 'v':
                    verbose = 1;
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
                case 't':
                    if ((speed & 3) < 3) {
                        speed++;
                    }
                    break;
                case 'd':
                    speed |= CH341A_STM_SPI_DBL;
                    break;
                case 'o':
                    offset = atoi(optarg);
                    break;
                case 'u':
                    op='u';
                    break;
                case 'S':
                    sec_op = 'R';
                    sec_page = atoi(optarg);
                    if (!op) op = 'S';
                    break;
                case 'W':
                    sec_op = 'W';
                    sec_page = atoi(optarg);
                    if (!op) {
                        op = 'S';
                        if (optind < argc && argv[optind][0] != '-') {
                            filename = (char*) malloc(strlen(argv[optind]) + 1);
                            strcpy(filename, argv[optind]);
                            optind++;
                        }
                    }
                    break;
                case 'E':
                    sec_op = 'E';
                    sec_page = atoi(optarg);
                    if (!op) op = 'S';
                    break;
                case 'L':
                    sec_op = 'L';
                    sec_page = atoi(optarg);
                    if (!op) op = 'S';
                    break;
                case 'D':
                    sec_op = 'D';
                    if (!op) op = 'S';
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
        fprintf(stderr, "Conflicting options, only one option at a time.\n");
        return -1;
    }
    ret = ch341Configure(CH341A_USB_VENDOR, CH341A_USB_PRODUCT);
    if (ret < 0)
        return -1;
    ret = ch341SetStream(speed);
    if (ret < 0) goto fail;
    ret = ch341SpiCapacity();
    if (ret < 0) goto fail;
    cap = 1 << ret;
    printf("Chip capacity is %d bytes\n", cap);

    if (length != 0){
        cap = length;
    }
    if (op == 'i') goto out;
    if (op == 'S') {
        uint8_t secbuf[256];
        if (sec_op == 'D') {
            for (int p = 0; p <= 3; p++) {
                ret = ch341ReadSecReg(p, secbuf);
                if (ret < 0) {
                    fprintf(stderr, "Failed to read security register page %d\n", p);
                    goto fail;
                }
                printf("=== Security Register Page %d ===\n", p);
                for (int j = 0; j < 256; j += 16) {
                    printf("%02x: ", j);
                    for (int k = 0; k < 16; k++)
                        printf("%02x ", secbuf[j + k]);
                    printf(" |");
                    for (int k = 0; k < 16; k++)
                        printf("%c", (secbuf[j + k] >= 0x20 && secbuf[j + k] < 0x7f) ? secbuf[j + k] : '.');
                    printf("|\n");
                }
            }
            ret = ch341ReadStatus2();
            if (ret >= 0) {
                printf("\nStatus Register 2: 0x%02x\n", ret);
                printf("  LB1 (page 1 lock): %s\n", (ret & 0x08) ? "LOCKED (OTP)" : "unlocked");
                printf("  LB2 (page 2 lock): %s\n", (ret & 0x10) ? "LOCKED (OTP)" : "unlocked");
                printf("  LB3 (page 3 lock): %s\n", (ret & 0x20) ? "LOCKED (OTP)" : "unlocked");
            }
            goto out;
        }
        if (sec_page < 0 || sec_page > 3) {
            fprintf(stderr, "Security register page must be 0-3\n");
            goto fail;
        }
        if (sec_op == 'R') {
            ret = ch341ReadSecReg(sec_page, secbuf);
            if (ret < 0) {
                fprintf(stderr, "Failed to read security register\n");
                goto fail;
            }
            printf("Security Register Page %d:\n", sec_page);
            for (int j = 0; j < 256; j += 16) {
                printf("%02x: ", j);
                for (int k = 0; k < 16; k++)
                    printf("%02x ", secbuf[j + k]);
                printf(" |");
                for (int k = 0; k < 16; k++)
                    printf("%c", (secbuf[j + k] >= 0x20 && secbuf[j + k] < 0x7f) ? secbuf[j + k] : '.');
                printf("|\n");
            }
            goto out;
        }
        if (sec_op == 'E') {
            if (sec_page == 0) {
                fprintf(stderr, "Cannot erase manufacturer page 0\n");
                goto fail;
            }
            printf("Erasing security register page %d...\n", sec_page);
            ret = ch341EraseSecReg(sec_page);
            if (ret < 0) {
                fprintf(stderr, "Erase failed\n");
                goto fail;
            }
            printf("Erase done!\n");
            goto out;
        }
        if (sec_op == 'W') {
            if (sec_page == 0) {
                fprintf(stderr, "Cannot write manufacturer page 0\n");
                goto fail;
            }
            if (filename == NULL) {
                fprintf(stderr, "No filename specified. Usage: -W <page> <filename>\n");
                goto fail;
            }
            fp = fopen(filename, "rb");
            if (!fp) {
                fprintf(stderr, "Cannot open %s\n", filename);
                goto fail;
            }
            memset(secbuf, 0xff, 256);
            ret = fread(secbuf, 1, 256, fp);
            fclose(fp);
            if (ret <= 0) {
                fprintf(stderr, "Empty file\n");
                goto fail;
            }
            printf("Writing %d bytes to security register page %d...\n", ret, sec_page);
            int wret = ch341WriteSecReg(sec_page, secbuf, ret);
            if (wret < 0) {
                fprintf(stderr, "Write failed\n");
                goto fail;
            }
            printf("Write done! Verifying...\n");
            uint8_t vbuf[256];
            wret = ch341ReadSecReg(sec_page, vbuf);
            if (wret < 0) {
                fprintf(stderr, "Verify read failed\n");
                goto fail;
            }
            if (memcmp(secbuf, vbuf, ret) == 0)
                printf("Verify OK!\n");
            else {
                fprintf(stderr, "Verify FAILED! Data mismatch.\n");
                goto fail;
            }
            goto out;
        }
        if (sec_op == 'L') {
            if (sec_page < 1 || sec_page > 3) {
                fprintf(stderr, "Can only lock pages 1-3\n");
                goto fail;
            }
            ret = ch341ReadStatus2();
            if (ret < 0) {
                fprintf(stderr, "Failed to read status register 2\n");
                goto fail;
            }
            uint8_t sr2 = ret;
            uint8_t lb_bit = 1 << (sec_page + 2);
            if (sr2 & lb_bit) {
                printf("Security register page %d is already locked.\n", sec_page);
                goto out;
            }
            printf("WARNING: This will PERMANENTLY lock security register page %d!\n", sec_page);
            printf("This operation is IRREVERSIBLE. Type 'YES' to confirm: ");
            fflush(stdout);
            char confirm[16];
            if (fgets(confirm, sizeof(confirm), stdin) == NULL || strncmp(confirm, "YES", 3) != 0) {
                printf("Aborted.\n");
                goto out;
            }
            sr2 |= lb_bit;
            ret = ch341WriteStatus2(sr2);
            if (ret < 0) {
                fprintf(stderr, "Failed to write status register 2\n");
                goto fail;
            }
            printf("Security register page %d is now PERMANENTLY locked.\n", sec_page);
            goto out;
        }
    }
    if (op == 'u') {
        ret = ch341WriteStatus(0);
        if (ret < 0) goto fail;
        printf("Chip status %04x\n",ret);
    }
    if (op == 'e') {
        uint8_t timeout = 0;
        ret = ch341EraseChip();
        if (ret < 0) goto fail;
        do {
            sleep(1);
            ret = ch341ReadStatus();
            if (ret < 0) goto fail;
            printf(".");
            fflush(stdout);
            timeout++;
            if (timeout == 100) break;
        } while(ret != 0);
        if (timeout == 100)
        {
            fprintf(stderr, "Chip erase timeout.\n");
            goto fail;
        }
        else
            printf("Chip erase done!\n");
    }
    if ((op == 'r') || (op == 'w')) {
        buf = (uint8_t *)malloc(cap);
        if (!buf) {
            fprintf(stderr, "Malloc failed for read buffer.\n");
            goto fail;
        }
    }
    if (op == 'r') {
        ret = ch341SpiRead(buf, offset, cap);
        if (ret < 0)
        {
            goto fail;
        }
        fp = fopen(filename, "wb");
        if (!fp) {
            fprintf(stderr, "Couldn't open file %s for writing.\n", filename);
            goto fail;
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
            goto fail;
        }
        ret = fread(buf, 1, cap, fp);
        if (ferror(fp)) {
            fprintf(stderr, "Error reading file [%s]\n", filename);
            if (fp)
                fclose(fp);
            goto fail;
        }
        cap = ret;
        fprintf(stderr, "File Size is [%d]\n", ret);
        ret = ch341SpiWrite(buf, offset, ret);
        if (ret == 0) {
            printf("\nWrite ok! Try to verify... ");
            FILE *test_file;
            char *test_filename;
            test_filename = (char*) malloc(strlen("./test-firmware.bin") + 1);
            strcpy(test_filename, "./test-firmware.bin");

            ret = ch341SpiRead(buf, offset, cap);
            test_file = fopen(test_filename, "w+b");

            if (!test_file) {
                fprintf(stderr, "Couldn't open file %s for writing.\n", test_filename);
                goto fail;
            }
            fwrite(buf, 1, cap, test_file);

            if (ferror(test_file))
                fprintf(stderr, "Error writing file [%s]\n", test_filename);

            fseek(fp, 0, SEEK_SET);
            fseek(test_file, 0, SEEK_SET);

            int ch1, ch2;
            int checked_count = 0;
            ch1 = getc(fp);
            ch2 = getc(test_file);

            while ((ch1 != EOF) && (ch2 != EOF) && (ch1 == ch2)) {
                ch1 = getc(fp);
                ch2 = getc(test_file);
                checked_count++;
            }

            if (ch1 == ch2 || (checked_count == cap))
                printf("\nWrite completed successfully. \n");
            else
            {
                fprintf(stderr, "\nError while writing. Check your device. Maybe it needs to be erased.\n");
                goto fail;
            }

            if (remove(test_filename) == 0)
                printf("\nAll done. \n");
            else
                printf("\nTemp file could not be deleted");
        }
        fclose(fp);
    }
    goto out;
fail:
    exitcode = 1;
out:
    ch341Release();
    return exitcode;
}
