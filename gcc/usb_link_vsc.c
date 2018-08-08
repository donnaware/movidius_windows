//---------------------------------------------------------------------------
// MVNC USB Link Utilities
//---------------------------------------------------------------------------
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/timeb.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>

#include "libusb.h"
#include "usb_link.h"
#include "usb_boot.h"
#include "common.h"

#define USB_ENDPOINT_IN         0x81
#define USB_ENDPOINT_OUT        0x01
#define USB_TIMEOUT             10000

#define USB_MAX_PACKET_SIZE     1024 * 1024 * 10

#define SLEEP_MS        100
#define ITERATIONS      50

//---------------------------------------------------------------------------
static int usb_write(void *f, const void *data, size_t size)
{
        while (size > 0) {
                int bt, ss = size;
                if(ss > USB_MAX_PACKET_SIZE) ss = USB_MAX_PACKET_SIZE;
                if(libusb_bulk_transfer(f, USB_ENDPOINT_OUT, (unsigned char *) data, ss, &bt, USB_TIMEOUT)) {
                        return -1;
        }
                data = (char *) data + bt;
                size -= bt;
        }
        return 0;
}
//---------------------------------------------------------------------------
static int usb_read(void *f, void *data, size_t size)
{
        while(size > 0) {
                int bt, ss = size;
                if(ss > USB_MAX_PACKET_SIZE) ss = USB_MAX_PACKET_SIZE;
        if(libusb_bulk_transfer(f, USB_ENDPOINT_IN, data, ss, &bt, USB_TIMEOUT)) {
                        return -1;
        }
                data = (char *) data + bt;
                size -= bt;
        }
        return 0;
}
//---------------------------------------------------------------------------
void *usblink_open(const char *path)
{
        int rc;
        libusb_device_handle *h = NULL;
        libusb_device *dev;

        rc = usb_find_device(0, (char *) path, 0, (void **) &dev, DEFAULT_OPEN_VID, DEFAULT_OPEN_PID);
        if(rc < 0) {
                return 0;
    }
        rc = libusb_open(dev, &h);
        if(rc < 0) {
                libusb_unref_device(dev);
                return 0;
        }
//      libusb_unref_device(dev);
        if (rc < 0) {
                libusb_close(h);
                return 0;
        }
        return h;
}
//---------------------------------------------------------------------------
void usblink_close(void *f)
{
        libusb_release_interface(f, 0);
        libusb_close(f);
}
//---------------------------------------------------------------------------
void usblink_resetall(void)
{
        libusb_device **devs;
        libusb_device *dev;
        struct libusb_device_descriptor desc;
        libusb_device_handle *h;
        size_t i;
        int rc, iters = 0, cnt_bootrom = 0, cnt_runtime = 0, cnt_after = 0;

        if((rc = libusb_get_device_list(NULL, &devs)) < 0) {
                return;
    }
        i = 0;
        while ((dev = devs[i++]) != NULL) {
                if(libusb_get_device_descriptor(dev, &desc) < 0)  continue;
                if(desc.idVendor == DEFAULT_VID && desc.idProduct == DEFAULT_PID) cnt_bootrom++;
                if(desc.idVendor == DEFAULT_OPEN_VID && desc.idProduct == DEFAULT_OPEN_PID) {           // If Runtime device found, reset it
                        cnt_runtime++;
                        rc = libusb_open(dev, &h);
                        if(rc < -1) {
                                continue;
            }
                        rc = libusb_claim_interface(h, 0);
                        if(rc < 0) {
                                libusb_close(h);
                                continue;
                        }
                        PRINT_DEBUG("Found stale device, resetting\n");
                        usblink_resetmyriad(h);
                        usblink_close(h);
                }
        }
        if(cnt_runtime > 0){            // If some devices needed reset
                iters = 0;
                while((cnt_after < cnt_bootrom + cnt_runtime) && (iters < ITERATIONS)){                 // Wait until all devices re-enumerate, or timeout occurs
                        usleep(SLEEP_MS*1000);
                        cnt_after = 0;
                        if ((rc = libusb_get_device_list(NULL, &devs)) < 0)
                                return;
                        i = 0;
                        while ((dev = devs[i++]) != NULL) {
                                if((rc = libusb_get_device_descriptor(dev, &desc)) < 0)
                                        continue;
                                if(desc.idVendor == DEFAULT_VID && desc.idProduct == DEFAULT_PID)
                                        cnt_after++;
                        }
                        iters++;
                }
        }
        libusb_free_device_list(devs, 1);
}

//---------------------------------------------------------------------------
int usblink_setdata(void *f, const char *name, const void *data, unsigned int length, int host_ready)
{
    int rc;
        usbHeader_t header;
        unsigned int operation_permit = 0xFFFF;

    libusb_claim_interface(f, 0);
        memset(&header, 0, sizeof(header));
        header.cmd = USB_LINK_HOST_SET_DATA;
        header.hostready = host_ready;
        strcpy(header.name, name);
        header.dataLength = length;
        if(usb_write(f, &header, sizeof(header))) return -1;
        if(usb_read(f, &operation_permit, sizeof(operation_permit))) return -1;
        if(operation_permit != 0xABCD) return -1;
        rc = usb_write(f, data, length);
    libusb_release_interface(f, 0);
        return rc;
}

//---------------------------------------------------------------------------
int usblink_getdata(void *f, const char *name, void *data, unsigned int length, unsigned int offset, int host_ready)
{
    int rc;
        unsigned int operation_permit = 0xFFFF;
        usbHeader_t header;

    libusb_claim_interface(f, 0);
        memset(&header, 0, sizeof(header));
        header.cmd = USB_LINK_HOST_GET_DATA;
        header.hostready = host_ready;
        strcpy(header.name, name);
        header.dataLength = length;
        header.offset = offset;
        if(usb_write(f, &header, sizeof(header))) return -1;
        if(usb_read(f, &operation_permit, sizeof(operation_permit))) return -1;
        if(operation_permit != 0xABCD) return -1;
        rc = usb_read(f, data, length);
    libusb_release_interface(f, 0);
    return rc;
}

//---------------------------------------------------------------------------
int usblink_resetmyriad(void *f)
{
        usbHeader_t header;
    libusb_claim_interface(f, 0);
        memset(&header, 0, sizeof(header));
        header.cmd = USB_LINK_RESET_REQUEST;
        if(usb_write(f, &header, sizeof(header))) return -1;
    libusb_release_interface(f, 0);
        return 0;
}
//---------------------------------------------------------------------------
int usblink_getmyriadstatus(void *f, myriadStatus_t* myriad_state)
{
    int rc;
        usbHeader_t header;
    libusb_claim_interface(f, 0);
        memset(&header, 0, sizeof(header));
        header.cmd = USB_LINK_GET_MYRIAD_STATUS;
        if(usb_write(f, &header, sizeof(header))) return -1;
    rc = usb_read(f, myriad_state, sizeof(*myriad_state));
    libusb_release_interface(f, 0);
        return rc;
}

//---------------------------------------------------------------------------

