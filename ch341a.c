#include <libusb-1.0/libusb.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "ch341a.h"

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

int32_t ch341Release(struct libusb_device_handle *devHandle)
{
    libusb_release_interface(devHandle, 0);
    libusb_close(devHandle);
    libusb_exit(NULL);
    return 0;
}

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

int32_t ch341SpiStream(struct libusb_device_handle *devHandle, uint8_t *out, uint8_t *in, uint32_t len)
{
    uint8_t outBuf[CH341_MAX_PACKET_LEN], *outp;

    if (len > CH341_MAX_PACKET_LEN - CH341_PACKET_LENGTH)
        return -1;
    outp = outBuf;
    *outp++ = CH341A_CMD_UIO_STREAM;
    *outp++ = CH341A_CMD_UIO_STM_OUT | 0x36; // chip select
    *outp++ = CH341A_CMD_UIO_STM_DIR | 0x3F; // pin direction
    *outp++ = CH341A_CMD_UIO_STM_END;
    outp = outBuf + CH341_PACKET_LENGTH; // don't care what's after CH341A_CMD_UIO_STM_END
    *outp++ = CH341A_CMD_SPI_STREAM;
    for (int i = 0; i < len; ++i)
        *outp++ = swapByte(*out++);
    usbTransfer(__func__, devHandle, BULK_WRITE_ENDPOINT, outBuf, len + CH341_PACKET_LENGTH + 1);
    usbTransfer(__func__, devHandle, BULK_READ_ENDPOINT, in, len);
    outp = outBuf;
    *outp++ = CH341A_CMD_UIO_STREAM;
    *outp++ = CH341A_CMD_UIO_STM_OUT | 0x37; // chip deselect
    *outp++ = CH341A_CMD_UIO_STM_END;
    usbTransfer(__func__, devHandle, BULK_WRITE_ENDPOINT, outBuf, 3);
    return 0;
}

