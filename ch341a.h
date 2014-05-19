#ifndef __CH341_H__
#define __CH341_H__

#ifdef __cplusplus                              
extern "C" {
#endif
#define     DEFAULT_TIMEOUT        300     // 300mS for USB timeouts
#define     BULK_WRITE_ENDPOINT    0x02
#define     BULK_READ_ENDPOINT     0x82

#define     CH341_PACKET_LENGTH    0x20
#define     CH341_MAX_PACKET_LEN   4096
#define     CH341A_USB_VENDOR      0x1A86
#define     CH341A_USB_PRODUCT     0x5512

#define     CH341A_CMD_SET_OUTPUT  0xA1
#define     CH341A_CMD_IO_ADDR     0xA2
#define     CH341A_CMD_PRINT_OUT   0xA3
#define     CH341A_CMD_SPI_STREAM  0xA8
#define     CH341A_CMD_SIO_STREAM  0xA9
#define     CH341A_CMD_I2C_STREAM  0xAA
#define     CH341A_CMD_UIO_STREAM  0xAB

#define     CH341A_CMD_I2C_STM_STA 0x74
#define     CH341A_CMD_I2C_STM_STO 0x75
#define     CH341A_CMD_I2C_STM_OUT 0x80
#define     CH341A_CMD_I2C_STM_IN  0xC0
#define     CH341A_CMD_I2C_STM_MAX ( min( 0x3F, CH341_PACKET_LENGTH ) )
#define     CH341A_CMD_I2C_STM_SET 0x60
#define     CH341A_CMD_I2C_STM_US  0x40
#define     CH341A_CMD_I2C_STM_MS  0x50
#define     CH341A_CMD_I2C_STM_DLY 0x0F
#define     CH341A_CMD_I2C_STM_END 0x00

#define     CH341A_CMD_UIO_STM_IN  0x00
#define     CH341A_CMD_UIO_STM_DIR 0x40
#define     CH341A_CMD_UIO_STM_OUT 0x80
#define     CH341A_CMD_UIO_STM_US  0xC0
#define     CH341A_CMD_UIO_STM_END 0x20


int32_t usbTransfer(const char * func, struct libusb_device_handle *devHandle, uint8_t type, uint8_t* buf, int len);
struct libusb_device_handle *ch341Configure(uint16_t vid, uint16_t pid);
int32_t ch341SetStream(struct libusb_device_handle *devHandle, uint32_t speed);
int32_t ch341SpiStream(struct libusb_device_handle *devHandle, uint8_t *out, uint8_t *in, uint32_t len);
int32_t ch341Release(struct libusb_device_handle *devHandle);
uint8_t swapByte(uint8_t c);

#ifdef __cplusplus                              
}
#endif

#endif
