#include <libusb-1.0/libusb.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "ch341a.h"

int32_t bulkin_count;

/* Configure CH341A, find the device and set the default interface. */
struct libusb_device_handle *ch341Configure(uint16_t vid, uint16_t pid)
{ 
    struct libusb_device *dev;
    struct libusb_device_handle *devHandle;
    int32_t ret;

    uint8_t  desc[0x12];

    ret = libusb_init(NULL);
    if(ret < 0) {
        fprintf(stderr, "Couldnt initialise libusb\n");
        return NULL;
    }

    libusb_set_debug(NULL, 3);
    
    if(!(devHandle = libusb_open_device_with_vid_pid(NULL, vid, pid))) {
        fprintf(stderr, "Couldn't open device [%04x:%04x].\n", vid, pid);
        return NULL;
    }
 
    if(!(dev = libusb_get_device(devHandle))) {
        fprintf(stderr, "Couldn't get bus number and address.\n");
        return NULL;
    }

    if(libusb_kernel_driver_active(devHandle, 0)) {
        ret = libusb_detach_kernel_driver(devHandle, 0);
        if(ret) {
            fprintf(stderr, "Failed to detach kernel driver: '%s'\n", strerror(-ret));
            return NULL;
        }
    }
    
    ret = libusb_claim_interface(devHandle, 0);

    if(ret) {
        fprintf(stderr, "Failed to claim interface 0: '%s'\n", strerror(-ret));
        return NULL;
    }
    
    ret = libusb_get_descriptor(devHandle, LIBUSB_DT_DEVICE, 0x00, desc, 0x12);

    if(ret < 0) {
        fprintf(stderr, "Failed to get device descriptor: '%s'\n", strerror(-ret));
        return NULL;
    }
    
    printf("Device reported its revision [%d.%02d]\n", desc[12], desc[13]);
    return devHandle;
}

/* release libusb structure and ready to exit */
int32_t ch341Release(struct libusb_device_handle *devHandle)
{
    libusb_release_interface(devHandle, 0);
    libusb_close(devHandle);
    libusb_exit(NULL);
    return 0;
}

/* Helper function for libusb_bulk_transfer, display error message with the caller name */
int32_t usbTransfer(const char * func, struct libusb_device_handle *devHandle, uint8_t type, uint8_t* buf, int len)
{
    int32_t ret;
    int transfered;
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
int32_t ch341SetStream(struct libusb_device_handle *devHandle, uint32_t speed) {
    uint8_t buf[3];

    buf[0] = CH341A_CMD_I2C_STREAM;
    buf[1] = CH341A_CMD_I2C_STM_SET | (speed & 0x7);
    buf[2] = CH341A_CMD_I2C_STM_END;

    return usbTransfer(__func__, devHandle, BULK_WRITE_ENDPOINT, buf, 3);
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
int32_t ch341SpiStream(struct libusb_device_handle *devHandle, uint8_t *out, uint8_t *in, uint32_t len)
{
    uint8_t outBuf[CH341_MAX_PACKET_LEN], *ptr;
    int32_t ret;

    if (len > CH341_MAX_PACKET_LEN - CH341_PACKET_LENGTH)
        return -1;
    ch341SpiCs(outBuf, true);
    ptr = outBuf + CH341_PACKET_LENGTH; // don't care what's after CH341A_CMD_UIO_STM_END
    *ptr++ = CH341A_CMD_SPI_STREAM;
    for (int i = 0; i < len; ++i)
        *ptr++ = swapByte(*out++);
    ret = usbTransfer(__func__, devHandle, BULK_WRITE_ENDPOINT, outBuf, len + CH341_PACKET_LENGTH + 1);
    if (ret < 0) return -1;
    ret = usbTransfer(__func__, devHandle, BULK_READ_ENDPOINT, in, len);
    if (ret < 0) return -1;
    ptr = in;
    for (int i = 0; i < ret; ++i) { // swap the buffer
        *ptr = swapByte(*ptr);
        ptr++;
    }
    ch341SpiCs(outBuf, false);
    ret = usbTransfer(__func__, devHandle, BULK_WRITE_ENDPOINT, outBuf, 3);
    if (ret < 0) return -1;
    return 0;
}

/* read the JEDEC ID of the SPI Flash */
int32_t ch341SpiCapacity(struct libusb_device_handle *devHandle)
{
    uint8_t out[4];
    uint8_t in[4], *ptr;
    int32_t ret;

    ptr = out;
    *ptr++ = 0x9F; // Read JEDEC ID
    for (int i = 0; i < 3; ++i)
        *ptr++ = 0x00;
    ret = ch341SpiStream(devHandle, out, in, 4);
    if (ret < 0) return ret;
    printf("Manufacturer ID: %02x\n", in[1]);
    printf("Memory Type: %02x\n", in[2]);
    printf("Capacity: %02x\n", in[3]);
    return in[3];
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
            for(int i = (bulkin_count == 0) ? 4 : 0; i < transfer->actual_length; ++i) {
                *((uint8_t*)transfer->user_data++) = swapByte(transfer->buffer[i]);
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
int32_t ch341SpiRead(struct libusb_device_handle *devHandle, uint8_t *buf, uint32_t add, uint32_t len)
{
    uint8_t out[CH341_MAX_PACKET_LEN];
    uint8_t in[CH341_PACKET_LENGTH];

    /* what subtracted is: 1. first cs package, 2. leading command for every package, 3. leading 4 byte address  */
    const uint32_t max_pkg_count = (CH341_MAX_PACKET_LEN / CH341_PACKET_LENGTH) + ((CH341_MAX_PACKET_LEN % CH341_PACKET_LENGTH) ? 1 : 0) - 1;
    const uint32_t max_payload = CH341_MAX_PACKET_LEN - CH341_PACKET_LENGTH - (CH341_MAX_PACKET_LEN / CH341_PACKET_LENGTH) - ((CH341_MAX_PACKET_LEN % CH341_PACKET_LENGTH) ? 1 : 0) + 1 - 4;
    uint32_t tmp, pkg_len, pkg_count;
    struct libusb_transfer *xferBulkIn, *xferBulkOut;
    uint32_t idx = 0;
    uint32_t ret;
    int32_t old_counter;
    struct timeval tv = {0, 100};

    memset(out, 0xff, CH341_MAX_PACKET_LEN);
    for (int i = 1; i < CH341_MAX_PACKET_LEN / CH341_PACKET_LENGTH + 1; ++i)
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
            out[idx++] = swapByte((tmp >> 24) & 0xFF);
            tmp <<= 8;
        }
        if (len > max_payload) {
            pkg_len = CH341_MAX_PACKET_LEN;
            pkg_count = max_pkg_count;
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
        ret = usbTransfer(__func__, devHandle, BULK_WRITE_ENDPOINT, out, 3);
        if (ret < 0) break;
    }
    libusb_free_transfer(xferBulkIn);
    libusb_free_transfer(xferBulkOut);
    return ret;
}
