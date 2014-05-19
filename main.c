#include <libusb-1.0/libusb.h>
#include <stdint.h>
#include <stdio.h>
#include "ch341a.h"

int main(int argc, char* argv[])
{
    struct libusb_device_handle *devHandle = NULL;
    uint8_t out[10];
    uint8_t in[10], *ptr;
    uint32_t ret;

    devHandle = ch341Configure(CH341A_USB_VENDOR, CH341A_USB_PRODUCT);
    if (devHandle == NULL)
        return -1;
    ret = ch341SetStream(devHandle, 1);
    if (ret < 0) goto out;
    ptr = out;
    *ptr++ = 0x9F; // Read JEDEC ID
    for (int i = 0; i < 3; ++i)
        *ptr++ = 0x00;
    ret = ch341SpiStream(devHandle, out, in, 4);
    if (ret < 0) goto out;
    printf("Manufacturer ID: %02x\n", in[1]);
    printf("Memory Type: %02x\n", in[2]);
    printf("Capacity: %02x\n", in[3]);
out:
    ch341Release(devHandle);
    return 0;
}
