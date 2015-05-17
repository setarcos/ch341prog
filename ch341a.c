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
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <signal.h>
#include "ch341a.h"

int32_t bulkin_count;
struct libusb_device_handle *devHandle = NULL;
struct sigaction saold;
int force_stop = 0;

/* SIGINT handler */
void sig_int(int signo)
{
    force_stop = 1;
}

/* Configure CH341A, find the device and set the default interface. */
int32_t ch341Configure(uint16_t vid, uint16_t pid)
{ 
    struct libusb_device *dev;
    int32_t ret;
    struct sigaction sa;

    uint8_t  desc[0x12];

    if (devHandle != NULL) {
        fprintf(stderr, "Call ch341Release before re-configure\n");
        return -1;
    }
    ret = libusb_init(NULL);
    if(ret < 0) {
        fprintf(stderr, "Couldn't initialise libusb\n");
        return -1;
    }

    libusb_set_debug(NULL, 3);
    
    if(!(devHandle = libusb_open_device_with_vid_pid(NULL, vid, pid))) {
        fprintf(stderr, "Couldn't open device [%04x:%04x].\n", vid, pid);
        return -1;
    }
 
    if(!(dev = libusb_get_device(devHandle))) {
        fprintf(stderr, "Couldn't get bus number and address.\n");
        goto close_handle;
    }

    if(libusb_kernel_driver_active(devHandle, 0)) {
        ret = libusb_detach_kernel_driver(devHandle, 0);
        if(ret) {
            fprintf(stderr, "Failed to detach kernel driver: '%s'\n", strerror(-ret));
            goto close_handle;
        }
    }
    
    ret = libusb_claim_interface(devHandle, 0);

    if(ret) {
        fprintf(stderr, "Failed to claim interface 0: '%s'\n", strerror(-ret));
        goto close_handle;
    }
    
    ret = libusb_get_descriptor(devHandle, LIBUSB_DT_DEVICE, 0x00, desc, 0x12);

    if(ret < 0) {
        fprintf(stderr, "Failed to get device descriptor: '%s'\n", strerror(-ret));
        goto release_interface;
    }
    
    printf("Device reported its revision [%d.%02d]\n", desc[12], desc[13]);
    sa.sa_handler = &sig_int;
    sa.sa_flags = SA_RESTART;
    sigfillset(&sa.sa_mask);
    if (sigaction(SIGINT, &sa, &saold) == -1) {
        perror("Error: cannot handle SIGINT"); // Should not happen
    }
    return 0;
release_interface:
    libusb_release_interface(devHandle, 0);
close_handle:
    libusb_close(devHandle);
    devHandle = NULL;
    return -1;
}

/* release libusb structure and ready to exit */
int32_t ch341Release(void)
{
    if (devHandle == NULL) return -1;
    libusb_release_interface(devHandle, 0);
    libusb_close(devHandle);
    libusb_exit(NULL);
    devHandle = NULL;
    sigaction(SIGINT, &saold, NULL);
    return 0;
}

/* Helper function for libusb_bulk_transfer, display error message with the caller name */
int32_t usbTransfer(const char * func, uint8_t type, uint8_t* buf, int len)
{
    int32_t ret;
    int transfered;
    if (devHandle == NULL) return -1;
    ret = libusb_bulk_transfer(devHandle, type, buf, len, &transfered, DEFAULT_TIMEOUT);
    if (ret < 0) {
        fprintf(stderr, "%s: Failed to %s %d bytes '%s'\n", func,
                (type == BULK_WRITE_ENDPOINT) ? "write" : "read", len, strerror(-ret));
        return -1;
    }
    return transfered;
}

/*   set the i2c bus speed (speed(b1b0): 0 = 20kHz; 1 = 100kHz, 2 = 400kHz, 3 = 750kHz)
 *   set the spi bus data width(speed(b2): 0 = Single, 1 = Double)  */
int32_t ch341SetStream(uint32_t speed) {
    uint8_t buf[3];

    if (devHandle == NULL) return -1;
    buf[0] = CH341A_CMD_I2C_STREAM;
    buf[1] = CH341A_CMD_I2C_STM_SET | (speed & 0x7);
    buf[2] = CH341A_CMD_I2C_STM_END;

    return usbTransfer(__func__, BULK_WRITE_ENDPOINT, buf, 3);
}

/* ch341 requres LSB first, swap the bit order before send and after receive  */
uint8_t swapByte(uint8_t c)
{
    uint8_t result=0;
    for (int i = 0; i < 8; ++i)
    {
        result = result << 1;
        result |= (c & 1);
        c = c >> 1;
    }
    return result;
}

/* assert or deassert the chip-select pin of the spi device */
void ch341SpiCs(uint8_t *ptr, bool selected)
{
    *ptr++ = CH341A_CMD_UIO_STREAM;
    *ptr++ = CH341A_CMD_UIO_STM_OUT | (selected ? 0x36 : 0x37);
    if (selected)
        *ptr++ = CH341A_CMD_UIO_STM_DIR | 0x3F; // pin direction
    *ptr++ = CH341A_CMD_UIO_STM_END;
}

/* transfer len bytes of data to the spi device */
int32_t ch341SpiStream(uint8_t *out, uint8_t *in, uint32_t len)
{
    uint8_t inBuf[CH341_PACKET_LENGTH], outBuf[CH341_PACKET_LENGTH], *inPtr, *outPtr;
    int32_t ret, packetLen;
    bool done;

    if (devHandle == NULL) return -1;

    ch341SpiCs(outBuf, true);
    ret = usbTransfer(__func__, BULK_WRITE_ENDPOINT, outBuf, 4);
    if (ret < 0) return -1;

    inPtr = in;

    do {
        done=true;
        packetLen=len+1;    // STREAM COMMAND + data length
        if (packetLen>CH341_PACKET_LENGTH) {
            packetLen=CH341_PACKET_LENGTH;
            done=false;
        }
        outPtr = outBuf;
        *outPtr++ = CH341A_CMD_SPI_STREAM;
        for (int i = 0; i < packetLen-1; ++i)
            *outPtr++ = swapByte(*out++);
        ret = usbTransfer(__func__, BULK_WRITE_ENDPOINT, outBuf, packetLen);
        if (ret < 0) return -1;
        ret = usbTransfer(__func__, BULK_READ_ENDPOINT, inBuf, packetLen-1);
        if (ret < 0) return -1;
        len -= ret;

        for (int i = 0; i < ret; ++i) // swap the buffer
            *inPtr++ = swapByte(inBuf[i]);
    } while (!done);

    ch341SpiCs(outBuf, false);
    ret = usbTransfer(__func__, BULK_WRITE_ENDPOINT, outBuf, 3);
    if (ret < 0) return -1;
    return 0;
}

#define JEDEC_ID_LEN 0x52    // additional byte due to SPI shift
/* read the JEDEC ID of the SPI Flash */
int32_t ch341SpiCapacity(void)
{
    uint8_t out[JEDEC_ID_LEN];
    uint8_t in[JEDEC_ID_LEN], *ptr, cap;
    int32_t ret;

    if (devHandle == NULL) return -1;
    ptr = out;
    *ptr++ = 0x9F; // Read JEDEC ID
    for (int i = 0; i < JEDEC_ID_LEN-1; ++i)
        *ptr++ = 0x00;

    ret = ch341SpiStream(out, in, JEDEC_ID_LEN);
    if (ret < 0) return ret;
    printf("Manufacturer ID: %02x\n", in[1]);
    printf("Memory Type: %02x%02x\n", in[2], in[3]);

    if(in[0x11]=='Q' && in[0x12]=='R' && in[0x13]=='Y') {
        cap = in[0x28];
        printf("Reading device capacity from CFI structure\n");
    } else {
        cap = in[3];
        printf("No CFI structure found, trying to get capacity from device ID. Set manually if detection fails.\n");
    }

    printf("Capacity: %02x\n", cap);

    return cap;
}

/* read status register */
int32_t ch341ReadStatus(void)
{
    uint8_t out[2];
    uint8_t in[2];
    int32_t ret;

    if (devHandle == NULL) return -1;
    out[0] = 0x05; // Read status
    ret = ch341SpiStream(out, in, 2);
    if (ret < 0) return ret;
    return (in[1]);
}

/* write status register */
int32_t ch341WriteStatus(uint8_t status)
{
    uint8_t out[2];
    uint8_t in[2];
    int32_t ret;

    if (devHandle == NULL) return -1;
    out[0] = 0x06; // Write enable
    ret = ch341SpiStream(out, in, 1);
    if (ret < 0) return ret;
    out[0] = 0x01; // Write status
    out[1] = status;
    ret = ch341SpiStream(out, in, 2);
    if (ret < 0) return ret;
    out[0] = 0x04; // Write disable
    ret = ch341SpiStream(out, in, 1);
    if (ret < 0) return ret;
    return 0;
}

/* chip erase */
int32_t ch341EraseChip(void)
{
    uint8_t out[1];
    uint8_t in[1];
    int32_t ret;

    if (devHandle == NULL) return -1;
    out[0] = 0x06; // Write enable
    ret = ch341SpiStream(out, in, 1);
    if (ret < 0) return ret;
    out[0] = 0xC7; // Chip erase
    ret = ch341SpiStream(out, in, 1);
    if (ret < 0) return ret;
    out[0] = 0x04; // Write disable
    ret = ch341SpiStream(out, in, 1);
    if (ret < 0) return ret;
    return 0;
}

/* callback for bulk out async transfer */
void cbBulkOut(struct libusb_transfer *transfer)
{
    if (transfer->status != LIBUSB_TRANSFER_COMPLETED) {
        fprintf(stderr, "\ncbBulkOut: error : %d\n", transfer->status);
    }
}

/* callback for bulk in async transfer */
void cbBulkIn(struct libusb_transfer *transfer)
{
    switch(transfer->status) {
        case LIBUSB_TRANSFER_COMPLETED:
            /* the first package has cmd and address info, so discard 4 bytes */
            if (transfer->user_data != NULL) {
                for(int i = (bulkin_count == 0) ? 4 : 0; i < transfer->actual_length; ++i) {
                    *((uint8_t*)transfer->user_data++) = swapByte(transfer->buffer[i]);
                }
            }
            bulkin_count++;
            break;
        default:
            fprintf(stderr, "\ncbBulkIn: error : %d\n", transfer->status);
            bulkin_count = -1;
    }
    return;
}

/* read the content of SPI device to buf, make sure the buf is big enough before call  */
int32_t ch341SpiRead(uint8_t *buf, uint32_t add, uint32_t len)
{
    uint8_t out[CH341_MAX_PACKET_LEN];
    uint8_t in[CH341_PACKET_LENGTH];

    if (devHandle == NULL) return -1;
    /* what subtracted is: 1. first cs package, 2. leading command for every other packages,
     * 3. second package contains read flash command and 3 bytes address */
    const uint32_t max_payload = CH341_MAX_PACKET_LEN - CH341_PACKET_LENGTH - CH341_MAX_PACKETS + 1 - 4;
    uint32_t tmp, pkg_len, pkg_count;
    struct libusb_transfer *xferBulkIn, *xferBulkOut;
    uint32_t idx = 0;
    uint32_t ret;
    int32_t old_counter;
    struct timeval tv = {0, 100};

    memset(out, 0xff, CH341_MAX_PACKET_LEN);
    for (int i = 1; i < CH341_MAX_PACKETS; ++i) // fill CH341A_CMD_SPI_STREAM for every packet
        out[i * CH341_PACKET_LENGTH] = CH341A_CMD_SPI_STREAM;
    memset(in, 0x00, CH341_PACKET_LENGTH);
    xferBulkIn  = libusb_alloc_transfer(0);
    xferBulkOut = libusb_alloc_transfer(0);

    while (len > 0) {
        ch341SpiCs(out, true);
        idx = CH341_PACKET_LENGTH + 1;
        out[idx++] = 0xC0; // byte swapped command for Flash Read
        tmp = add;
        for (int i = 0; i < 3; ++i) { // starting address of next read
            out[idx++] = swapByte((tmp >> 16) & 0xFF);
            tmp <<= 8;
        }
        if (len > max_payload) {
            pkg_len = CH341_MAX_PACKET_LEN;
            pkg_count = CH341_MAX_PACKETS - 1;
            len -= max_payload;
            add += max_payload;
        } else {
            pkg_count = (len + 4) / (CH341_PACKET_LENGTH - 1);
            if ((len + 4) % (CH341_PACKET_LENGTH - 1)) pkg_count ++;
            pkg_len = (pkg_count) * CH341_PACKET_LENGTH + ((len + 4) % (CH341_PACKET_LENGTH - 1)) + 1;
            len = 0;
        }
        bulkin_count = 0;
        libusb_fill_bulk_transfer(xferBulkIn, devHandle, BULK_READ_ENDPOINT, in,
                CH341_PACKET_LENGTH, cbBulkIn, buf, DEFAULT_TIMEOUT);
        buf += max_payload; // advance user's pointer
        libusb_submit_transfer(xferBulkIn);
        libusb_fill_bulk_transfer(xferBulkOut, devHandle, BULK_WRITE_ENDPOINT, out,
                pkg_len, cbBulkOut, NULL, DEFAULT_TIMEOUT);
        libusb_submit_transfer(xferBulkOut);
        old_counter = bulkin_count;
        while (bulkin_count < pkg_count) {
            libusb_handle_events_timeout(NULL, &tv);
            if (bulkin_count == -1) { // encountered error
                len = 0;
                ret = -1;
                break;
            }
            if (old_counter != bulkin_count) { // new package came
                if (bulkin_count != pkg_count)
                    libusb_submit_transfer(xferBulkIn);  // resubmit bulk in request
                old_counter = bulkin_count;
            }
        }
        ch341SpiCs(out, false);
        ret = usbTransfer(__func__, BULK_WRITE_ENDPOINT, out, 3);
        if (ret < 0) break;
        if (force_stop == 1) { // user hit ctrl+C
            force_stop = 0;
            if (len > 0)
                fprintf(stderr, "User hit Ctrl+C, reading unfinished.\n");
            break;
        }
    }
    libusb_free_transfer(xferBulkIn);
    libusb_free_transfer(xferBulkOut);
    return ret;
}

#define WRITE_PAYLOAD_LENGTH 301 // 301 is the length of a page(256)'s data with protocol overhead 
/* write buffer(*buf) to SPI flash */
int32_t ch341SpiWrite(uint8_t *buf, uint32_t add, uint32_t len)
{
    uint8_t out[WRITE_PAYLOAD_LENGTH];
    uint8_t in[CH341_PACKET_LENGTH];
    uint32_t tmp, pkg_count;
    struct libusb_transfer *xferBulkIn, *xferBulkOut;
    uint32_t idx = 0;
    uint32_t ret;
    int32_t old_counter;
    struct timeval tv = {0, 100};

    if (devHandle == NULL) return -1;
    memset(out, 0xff, WRITE_PAYLOAD_LENGTH);
    xferBulkIn  = libusb_alloc_transfer(0);
    xferBulkOut = libusb_alloc_transfer(0);

    while (len > 0) {
        out[0] = 0x06; // Write enable
        ret = ch341SpiStream(out, in, 1);
        ch341SpiCs(out, true);
        idx = CH341_PACKET_LENGTH;
        out[idx++] = CH341A_CMD_SPI_STREAM;
        out[idx++] = 0x40; // byte swapped command for Flash Page Write
        tmp = add;
        for (int i = 0; i < 3; ++i) { // starting address of next write
            out[idx++] = swapByte((tmp >> 16) & 0xFF);
            tmp <<= 8;
        }
        tmp = 0;
        pkg_count = 1;
        while ((idx < WRITE_PAYLOAD_LENGTH) && (len > tmp)) {
            if (idx % CH341_PACKET_LENGTH == 0) {
                out[idx++] = CH341A_CMD_SPI_STREAM;
                pkg_count ++;
            } else {
                out[idx++] = swapByte(*buf++);
                tmp++;
                if (((add + tmp) & 0xFF) == 0) // cross page boundary
                    break;
            }
        }
        len -= tmp;
        add += tmp;
        bulkin_count = 0;
        libusb_fill_bulk_transfer(xferBulkIn, devHandle, BULK_READ_ENDPOINT, in,
                CH341_PACKET_LENGTH, cbBulkIn, NULL, DEFAULT_TIMEOUT);
        libusb_submit_transfer(xferBulkIn);
        libusb_fill_bulk_transfer(xferBulkOut, devHandle, BULK_WRITE_ENDPOINT, out,
                idx, cbBulkOut, NULL, DEFAULT_TIMEOUT);
        libusb_submit_transfer(xferBulkOut);
        old_counter = bulkin_count;
        ret = 0;
        while (bulkin_count < pkg_count) {
            libusb_handle_events_timeout(NULL, &tv);
            if (bulkin_count == -1) { // encountered error
                ret = -1;
                break;
            }
            if (old_counter != bulkin_count) { // new package came
                if (bulkin_count != pkg_count)
                    libusb_submit_transfer(xferBulkIn);  // resubmit bulk in request
                old_counter = bulkin_count;
            }
        }
        if (ret < 0) break;
        ch341SpiCs(out, false);
        ret = usbTransfer(__func__, BULK_WRITE_ENDPOINT, out, 3);
        if (ret < 0) break;
        out[0] = 0x04; // Write disable
        ret = ch341SpiStream(out, in, 1);
        do {
            ret = ch341ReadStatus();
            if (ret != 0)
                libusb_handle_events_timeout(NULL, &tv);
        } while(ret != 0);
        if (force_stop == 1) { // user hit ctrl+C
            force_stop = 0;
            if (len > 0)
                fprintf(stderr, "User hit Ctrl+C, writing unfinished.\n");
            break;
        }
    }
    libusb_free_transfer(xferBulkIn);
    libusb_free_transfer(xferBulkOut);
    return ret;
}
