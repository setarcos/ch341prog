#include <libusb-1.0/libusb.h>
#include <stdint.h>
#include <stdio.h>
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
    ch341SpiCapacity(devHandle);
    ret = ch341SpiRead(devHandle, buf, 0, 8000);
    if (ret < 0) goto out;
    for (int i = 0; i < 8888; ++i) {
        if (i % 32 == 0) printf("\n");
        printf("%02x ", buf[i]);
    }
out:
    ch341Release(devHandle);
    return 0;
}
