#include "controller.h"

#include <stdio.h>
#include <stdlib.h>

/* References on libusb 1.0 and the USB HID/keyboard protocol
 *
 * http://libusb.org
 * http://www.dreamincode.net/forums/topic/148707-introduction-to-using-libusb-10/
 * http://www.usb.org/developers/devclass_docs/HID1_11.pdf
 * http://www.usb.org/developers/devclass_docs/Hut1_11.pdf
 */

/*
 * Find and return a USB keyboard device or NULL if not found
 * The argument con
 *
 */
struct libusb_device_handle *opencontoller(uint8_t *endpoint_address) {

  libusb_device **devs;
  struct libusb_device_handle *controller = NULL;
  struct libusb_device_descriptor desc;

  ssize_t num_devs, d;


  /* Start the library */
  if ( libusb_init(NULL) < 0 ) {
    fprintf(stderr, "Error: libusb_init failed\n");
    exit(1);
  }

  /* Enumerate all the attached USB devices */
  if ( (num_devs = libusb_get_device_list(NULL, &devs)) < 0 ) {
    fprintf(stderr, "Error: libusb_get_device_list failed\n");
    exit(1);
  }

  /* Look at each device, remembering the first HID device that speaks
     the keyboard protocol */

  for (d = 0 ; d < num_devs ; d++) {

    libusb_device *dev = devs[d];

    if ( libusb_get_device_descriptor(dev, &desc) < 0 ) {
      fprintf(stderr, "Error: libusb_get_device_descriptor failed\n");
      exit(1);
    }

    if (desc.idProduct == 0x0011) {
      struct libusb_config_descriptor *config;
      libusb_get_config_descriptor(dev, 0, &config);

      const struct libusb_interface_descriptor *inter =
      config->interface[0].altsetting;

      int r;

      if ((r = libusb_open(dev, &controller)) != 0) {
        fprintf(stderr, "Error: libusb_open failed: %d\n", r);
        exit(1);
      }

      if (libusb_kernel_driver_active(controller,0))
        libusb_detach_kernel_driver(controller, 0);

      libusb_set_auto_detach_kernel_driver(controller, 0);

      if ((r = libusb_claim_interface(controller, 0)) != 0) {
        fprintf(stderr, "Error: libusb_claim_interface failed: %d\n", r);
        exit(1);
      }

	    *endpoint_address = inter->endpoint[0].bEndpointAddress;
      
      goto found;
    }
  }

found:
  libusb_free_device_list(devs, 1);

  return controller;
}