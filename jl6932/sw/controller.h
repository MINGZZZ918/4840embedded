#ifndef _CONTROLLER_H
#define _CONTROLLER_H

#include <libusb-1.0/libusb.h>


typedef struct {
    uint8_t pad_1;
    uint8_t pad_2;
    uint8_t pad_3;
    uint8_t lr_arrows;
    uint8_t ud_arrows;
    uint8_t buttons;
    uint8_t bumpers; // maybe change name
    uint8_t pad_4;

} controller_packet;

/* Find and open a USB keyboard device.  Argument should point to
   space to store an endpoint address.  Returns NULL if no keyboard
   device was found. */
extern struct libusb_device_handle *opencontroller(uint8_t *);

#endif