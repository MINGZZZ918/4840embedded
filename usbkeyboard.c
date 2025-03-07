#include "usbkeyboard.h"
#include <stdio.h>
#include <stdlib.h>

/* References on libusb 1.0 and the USB HID/keyboard protocol
 *
 * http://libusb.org
 * http://www.dreamincode.net/forums/topic/148707-introduction-to-using-libusb-10/
 * http://www.usb.org/developers/devclass_docs/HID1_11.pdf
 * http://www.usb.org/developers/devclass_docs/Hut1_11.pdf
 */
/*使用libusb库实现的USB键盘设备检测和初始化的函数。它的核心功能是遍历当前连接的USB设备，找到符合HID键盘协议的设备，进行初始化并返回设备句柄。*/
//保留USB键盘检测功能的基础上，新增了键码到字符的转换逻辑，使其能够直接输出可读文本，


struct libusb_device_handle *openkeyboard(uint8_t *endpoint_address) {
  libusb_device **devs;
  struct libusb_device_handle *keyboard = NULL;
  struct libusb_device_descriptor desc;
  ssize_t num_devs, d;
  uint8_t i, k;

  if ( libusb_init(NULL) < 0 ) {
    fprintf(stderr, "Error: libusb_init failed\n");
    exit(1);
  }

  if ( (num_devs = libusb_get_device_list(NULL, &devs)) < 0 ) {
    fprintf(stderr, "Error: libusb_get_device_list failed\n");
    exit(1);
  }

  for (d = 0 ; d < num_devs ; d++) {
    libusb_device *dev = devs[d];
    if ( libusb_get_device_descriptor(dev, &desc) < 0 ) {
      fprintf(stderr, "Error: libusb_get_device_descriptor failed\n");
      exit(1);
    }

    if (desc.bDeviceClass == LIBUSB_CLASS_PER_INTERFACE) {
      struct libusb_config_descriptor *config;
      // ==================== FIX ====================
      // 修复资源泄漏：增加获取配置描述符的错误检查
      if (libusb_get_config_descriptor(dev, 0, &config) != 0) {
        fprintf(stderr, "Error: libusb_get_config_descriptor failed\n");
        continue;
}
      // ==================== END FIX ====================
      for (i = 0 ; i < config->bNumInterfaces ; i++)
        for ( k = 0 ; k < config->interface[i].num_altsetting ; k++ ) {
          const struct libusb_interface_descriptor *inter =
            config->interface[i].altsetting + k ;
          if ( inter->bInterfaceClass == LIBUSB_CLASS_HID &&
               inter->bInterfaceProtocol == USB_HID_KEYBOARD_PROTOCOL) {
            int r;
            if ((r = libusb_open(dev, &keyboard)) != 0) {
              fprintf(stderr, "Error: libusb_open failed: %d\n", r);
              // ==================== FIX ====================
              libusb_free_config_descriptor(config);  // 释放配置描述符
              // ==================== END FIX ====================
              exit(1);
            }
            if (libusb_kernel_driver_active(keyboard,i))
              libusb_detach_kernel_driver(keyboard, i);
            libusb_set_auto_detach_kernel_driver(keyboard, i);
            if ((r = libusb_claim_interface(keyboard, i)) != 0) {
              fprintf(stderr, "Error: libusb_claim_interface failed: %d\n", r);
              // ==================== FIX ====================
              libusb_free_config_descriptor(config);  // 释放配置描述符
              // ==================== END FIX ====================
              exit(1);
            }
            *endpoint_address = inter->endpoint[0].bEndpointAddress;
            // ==================== FIX ====================
            libusb_free_config_descriptor(config);  // 正确释放资源
            // ==================== END FIX ====================
            goto found;
          }
        }
      // ==================== FIX ====================
      libusb_free_config_descriptor(config);  // 遍历完接口后释放资源
      // ==================== END FIX ====================
    }
  }

 found:
  libusb_free_device_list(devs, 1);
  return keyboard;
}

