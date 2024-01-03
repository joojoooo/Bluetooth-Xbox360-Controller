/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

/* This example current worked and tested with following controller
 * - Sony DualShock 4 [CUH-ZCT2x] VID = 0x054c, PID = 0x09cc
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "bsp/board.h"
#include "tusb.h"

#define BUF_COUNT 8

tusb_desc_device_t desc_device;

uint8_t temp_buf[321];
uint8_t buf_pool[BUF_COUNT][64];
uint8_t buf_owner[BUF_COUNT] = {0}; // device address that owns buffer

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF PROTYPES
//--------------------------------------------------------------------+
void led_blinking_task(void);

static void print_utf16(uint16_t *temp_buf, size_t buf_len);
void print_device_descriptor(tuh_xfer_t *xfer);
void parse_config_descriptor(uint8_t dev_addr, tusb_desc_configuration_t const *desc_cfg);

uint8_t *get_hid_buf(uint8_t daddr);
void free_hid_buf(uint8_t daddr);

/*------------- MAIN -------------*/
int main(void)
{
  board_init();

  printf("TinyUSB Bare API Example\r\n");

  // init host stack on configured roothub port
  tuh_init(BOARD_TUH_RHPORT);

  while (1)
  {
    // tinyusb host task
    tuh_task();
    led_blinking_task();
  }

  return 0;
}

/*------------- TinyUSB Callbacks -------------*/

// Invoked when device is mounted (configured)
void tuh_mount_cb(uint8_t daddr)
{
  printf("Device attached, address = %d\r\n", daddr);

  // Get Device Descriptor
  // TODO: invoking control transfer now has issue with mounting hub with multiple devices attached, fix later
  tuh_descriptor_get_device(daddr, &desc_device, sizeof(tusb_desc_device_t), print_device_descriptor, 0);
}

/// Invoked when device is unmounted (bus reset/unplugged)
void tuh_umount_cb(uint8_t daddr)
{
  printf("Device removed, address = %d\r\n", daddr);
  free_hid_buf(daddr);
}

//--------------------------------------------------------------------+
// Device Descriptor
//--------------------------------------------------------------------+

void print_device_descriptor(tuh_xfer_t *xfer)
{
  if (XFER_RESULT_SUCCESS != xfer->result)
  {
    printf("Failed to get device descriptor\r\n");
    return;
  }

  uint8_t const daddr = xfer->daddr;

  printf("Device addr: %u Vid: %04x Pid: %04x\r\n", daddr, desc_device.idVendor, desc_device.idProduct);
  printf("Device Descriptor:\r\n");
  printf("  bLength             %u\r\n", desc_device.bLength);
  printf("  bDescriptorType     %u\r\n", desc_device.bDescriptorType);
  printf("  bcdUSB              %04x\r\n", desc_device.bcdUSB);
  printf("  bDeviceClass        %u\r\n", desc_device.bDeviceClass);
  printf("  bDeviceSubClass     %u\r\n", desc_device.bDeviceSubClass);
  printf("  bDeviceProtocol     %u\r\n", desc_device.bDeviceProtocol);
  printf("  bMaxPacketSize0     %u\r\n", desc_device.bMaxPacketSize0);
  printf("  idVendor            0x%04x\r\n", desc_device.idVendor);
  printf("  idProduct           0x%04x\r\n", desc_device.idProduct);
  printf("  bcdDevice           %04x\r\n", desc_device.bcdDevice);
  printf("  iManufacturer       %u\r\n", desc_device.iManufacturer);
  printf("  iProduct            %u\r\n", desc_device.iProduct);
  printf("  iSerialNumber       %u\r\n", desc_device.iSerialNumber);
  printf("  bNumConfigurations  %u\r\n", desc_device.bNumConfigurations);

  if (XFER_RESULT_SUCCESS == tuh_descriptor_get_configuration_sync(daddr, 0, temp_buf, sizeof(temp_buf)))
  {
    parse_config_descriptor(daddr, (tusb_desc_configuration_t *)temp_buf);
  }
  else
  {
    printf("Failed to get configuration descriptor\r\n");
  }
}

//--------------------------------------------------------------------+
// Configuration Descriptor
//--------------------------------------------------------------------+

// count total length of an interface
uint16_t count_interface_total_len(tusb_desc_interface_t const *desc_itf, uint8_t itf_count, uint16_t max_len);

void open_hid_interface(uint8_t daddr, tusb_desc_interface_t const *desc_itf, uint16_t max_len);

// simple configuration parser to open and listen to HID Endpoint IN
void parse_config_descriptor(uint8_t dev_addr, tusb_desc_configuration_t const *desc_cfg)
{
  uint8_t const *desc_end = ((uint8_t const *)desc_cfg) + tu_le16toh(desc_cfg->wTotalLength);
  uint8_t const *p_desc = tu_desc_next(desc_cfg);

  // parse each interfaces
  while (p_desc < desc_end)
  {
    uint8_t assoc_itf_count = 1;

    // Class will always starts with Interface Association (if any) and then Interface descriptor
    if (TUSB_DESC_INTERFACE_ASSOCIATION == tu_desc_type(p_desc))
    {
      tusb_desc_interface_assoc_t const *desc_iad = (tusb_desc_interface_assoc_t const *)p_desc;
      assoc_itf_count = desc_iad->bInterfaceCount;

      p_desc = tu_desc_next(p_desc); // next to Interface
    }

    // must be interface from now
    if (TUSB_DESC_INTERFACE != tu_desc_type(p_desc))
      return;
    tusb_desc_interface_t const *desc_itf = (tusb_desc_interface_t const *)p_desc;

    uint16_t const drv_len = count_interface_total_len(desc_itf, assoc_itf_count, (uint16_t)(desc_end - p_desc));

    // probably corrupted descriptor
    if (drv_len < sizeof(tusb_desc_interface_t))
      return;

    // only open and listen to HID endpoint IN
    if (desc_itf->bInterfaceClass == TUSB_CLASS_HID || (desc_itf->bInterfaceClass == TUSB_CLASS_VENDOR_SPECIFIC && desc_itf->bInterfaceSubClass == 0x5d && desc_itf->bInterfaceProtocol == 0x81))
    {
      open_hid_interface(dev_addr, desc_itf, drv_len);
    }

    // next Interface or IAD descriptor
    p_desc += drv_len;
  }
}

uint16_t count_interface_total_len(tusb_desc_interface_t const *desc_itf, uint8_t itf_count, uint16_t max_len)
{
  uint8_t const *p_desc = (uint8_t const *)desc_itf;
  uint16_t len = 0;

  while (itf_count--)
  {
    // Next on interface desc
    len += tu_desc_len(desc_itf);
    p_desc = tu_desc_next(p_desc);

    while (len < max_len)
    {
      // return on IAD regardless of itf count
      if (tu_desc_type(p_desc) == TUSB_DESC_INTERFACE_ASSOCIATION)
        return len;

      if ((tu_desc_type(p_desc) == TUSB_DESC_INTERFACE) &&
          ((tusb_desc_interface_t const *)p_desc)->bAlternateSetting == 0)
      {
        break;
      }

      len += tu_desc_len(p_desc);
      p_desc = tu_desc_next(p_desc);
    }
  }

  return len;
}

//--------------------------------------------------------------------+
// HID Interface
//--------------------------------------------------------------------+

void hid_report_received(tuh_xfer_t *xfer);

void open_hid_interface(uint8_t daddr, tusb_desc_interface_t const *desc_itf, uint16_t max_len)
{
  uint8_t const *p_desc = (uint8_t const *)desc_itf;

  // Unknown descriptor
  p_desc = tu_desc_next(p_desc);
  // if (tu_desc_type(p_desc) != 0x22)
  // return;

  for (int i = 0; i < desc_itf->bNumEndpoints; i++)
  {
    // Endpoint descriptor
    p_desc = tu_desc_next(p_desc);
    tusb_desc_endpoint_t const *desc_ep = (tusb_desc_endpoint_t const *)p_desc;
    if (TUSB_DESC_ENDPOINT != desc_ep->bDescriptorType)
      return;

    if (tu_edpt_dir(desc_ep->bEndpointAddress) == TUSB_DIR_IN)
    {
      // skip if failed to open endpoint
      if (!tuh_edpt_open(daddr, desc_ep))
        return;

      uint8_t *buf = get_hid_buf(daddr);
      if (!buf)
        return; // out of memory

      tuh_xfer_t xfer =
          {
              .daddr = daddr,
              .ep_addr = desc_ep->bEndpointAddress,
              .buflen = 64,
              .buffer = buf,
              .complete_cb = hid_report_received,
              .user_data = (uintptr_t)buf, // since buffer is not available in callback, use user data to store the buffer
          };

      // submit transfer for this EP
      tuh_edpt_xfer(&xfer);

      printf("Listen to [itf: %d ep: %02x]\r\n", desc_itf->bInterfaceNumber, tu_edpt_number(desc_ep->bEndpointAddress));
    }
    else
    {
    }
  }
}

void hid_report_received(tuh_xfer_t *xfer)
{
  // Note: not all field in xfer is available for use (i.e filled by tinyusb stack) in callback to save sram
  // For instance, xfer->buffer is NULL. We have used user_data to store buffer when submitted callback
  uint8_t *buf = (uint8_t *)xfer->user_data;

  if (xfer->result == XFER_RESULT_SUCCESS)
  {
    printf("ep: %02x", tu_edpt_number(xfer->ep_addr));
    for (uint32_t i = 0; i < xfer->actual_len; i++)
    {
      if (i % 16 == 0)
        printf("\r\n  ");
      printf("%02X ", buf[i]);
    }
    printf("\r\n");
  }

  // continue to submit transfer, with updated buffer
  // other field remain the same
  xfer->buflen = 64;
  xfer->buffer = buf;

  tuh_edpt_xfer(xfer);
}

//--------------------------------------------------------------------+
// Buffer helper
//--------------------------------------------------------------------+

// get an buffer from pool
uint8_t *get_hid_buf(uint8_t daddr)
{
  for (size_t i = 0; i < BUF_COUNT; i++)
  {
    if (buf_owner[i] == 0)
    {
      buf_owner[i] = daddr;
      return buf_pool[i];
    }
  }

  // out of memory, increase BUF_COUNT
  return NULL;
}

// free all buffer owned by device
void free_hid_buf(uint8_t daddr)
{
  for (size_t i = 0; i < BUF_COUNT; i++)
  {
    if (buf_owner[i] == daddr)
      buf_owner[i] = 0;
  }
}

//--------------------------------------------------------------------+
// Blinking Task
//--------------------------------------------------------------------+
void led_blinking_task(void)
{
  const uint32_t interval_ms = 1000;
  static uint32_t start_ms = 0;

  static bool led_state = false;

  // Blink every interval ms
  if (board_millis() - start_ms < interval_ms)
    return; // not enough time
  start_ms += interval_ms;

  board_led_write(led_state);
  led_state = 1 - led_state; // toggle
}