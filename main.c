#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "bsp/board.h"
#include "tusb.h"

#define BUF_COUNT 8

tusb_desc_device_t desc_device;

uint8_t temp_buf[321];
uint8_t buf_pool[BUF_COUNT][64];
uint8_t buf_owner[BUF_COUNT] = {0};

void parse_config_descriptor(uint8_t dev_addr, tusb_desc_configuration_t const *desc_cfg);
uint16_t count_interface_total_len(tusb_desc_interface_t const *desc_itf, uint8_t itf_count, uint16_t max_len);
void open_vdr_interface(uint8_t daddr, tusb_desc_interface_t const *desc_itf, uint16_t max_len);
void vdr_report_received(tuh_xfer_t *xfer);
uint8_t *get_vdr_buf(uint8_t daddr);

int main(void)
{
  board_init();
  printf("started\r\n");
  tuh_init(BOARD_TUH_RHPORT);
  while (1)
  {
    tuh_task();
  }
  return 0;
}

void print_device_descriptor(tuh_xfer_t *xfer)
{
  if (XFER_RESULT_SUCCESS != xfer->result)
    return;
  uint8_t const daddr = xfer->daddr;
  /*
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
  */
  if (XFER_RESULT_SUCCESS == tuh_descriptor_get_configuration_sync(daddr, 0, temp_buf, sizeof(temp_buf)))
    parse_config_descriptor(daddr, (tusb_desc_configuration_t *)temp_buf);
}

void parse_config_descriptor(uint8_t dev_addr, tusb_desc_configuration_t const *desc_cfg)
{
  uint8_t const *desc_end = ((uint8_t const *)desc_cfg) + tu_le16toh(desc_cfg->wTotalLength);
  uint8_t const *p_desc = tu_desc_next(desc_cfg);

  while (p_desc < desc_end)
  {
    uint8_t assoc_itf_count = 1;

    if (TUSB_DESC_INTERFACE_ASSOCIATION == tu_desc_type(p_desc))
    {
      tusb_desc_interface_assoc_t const *desc_iad = (tusb_desc_interface_assoc_t const *)p_desc;
      assoc_itf_count = desc_iad->bInterfaceCount;

      p_desc = tu_desc_next(p_desc);
    }

    if (TUSB_DESC_INTERFACE != tu_desc_type(p_desc))
      return;
    tusb_desc_interface_t const *desc_itf = (tusb_desc_interface_t const *)p_desc;

    uint16_t const drv_len = count_interface_total_len(desc_itf, assoc_itf_count, (uint16_t)(desc_end - p_desc));

    // probably corrupted descriptor
    if (drv_len < sizeof(tusb_desc_interface_t))
      return;

    if (desc_itf->bInterfaceClass == TUSB_CLASS_VENDOR_SPECIFIC && desc_itf->bInterfaceSubClass == 0x5d && desc_itf->bInterfaceProtocol == 0x81)
    {
      open_vdr_interface(dev_addr, desc_itf, drv_len);
    }

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

void test(tuh_xfer_t *xfer)
{
  printf("Sent Inquiry pad presence");
}

void open_vdr_interface(uint8_t daddr, tusb_desc_interface_t const *desc_itf, uint16_t max_len)
{
  uint8_t const *p_desc = (uint8_t const *)desc_itf;

  // Unknown descriptor
  p_desc = tu_desc_next(p_desc);
  if (tu_desc_type(p_desc) != 0x22)
    return;

  for (int i = 0; i < desc_itf->bNumEndpoints; i++)
  {
    // Endpoint descriptor
    p_desc = tu_desc_next(p_desc);
    tusb_desc_endpoint_t const *desc_ep = (tusb_desc_endpoint_t const *)p_desc;
    if (TUSB_DESC_ENDPOINT != desc_ep->bDescriptorType || tu_edpt_number(desc_ep->bEndpointAddress) != 1)
      return;
    if (tu_edpt_number(desc_ep->bEndpointAddress) != 1)
      continue;

    if (!tuh_edpt_open(daddr, desc_ep))
      return;

    uint8_t *buf = get_vdr_buf(daddr);
    if (!buf)
      return;
    if (tu_edpt_dir(desc_ep->bEndpointAddress) == TUSB_DIR_IN)
    {
      tuh_xfer_t xfer =
          {
              .daddr = daddr,
              .ep_addr = desc_ep->bEndpointAddress,
              .buflen = 64,
              .buffer = buf,
              .complete_cb = vdr_report_received,
              .user_data = (uintptr_t)buf, // since buffer is not available in callback, use user data to store the buffer
          };
      tuh_edpt_xfer(&xfer);

      printf("Listen to [ITF: %d EP: %02x]\r\n", desc_itf->bInterfaceNumber, tu_edpt_number(desc_ep->bEndpointAddress));
    }
    else
    {
      buf[0] = 0x08;
      buf[1] = 0x00;
      buf[2] = 0x0F;
      buf[3] = 0xC0;
      buf[4] = 0x00;
      buf[5] = 0x00;
      buf[6] = 0x00;
      buf[7] = 0x00;
      buf[8] = 0x00;
      buf[9] = 0x00;
      buf[10] = 0x00;
      buf[11] = 0x00;
      tuh_xfer_t xfer =
          {
              .daddr = daddr,
              .ep_addr = desc_ep->bEndpointAddress,
              .buflen = 12,
              .buffer = buf,
              .complete_cb = test,
              .user_data = (uintptr_t)buf, // since buffer is not available in callback, use user data to store the buffer
          };
      tuh_edpt_xfer(&xfer);

      printf("Sending Inquiry pad presence [ITF: %d EP: %02x]\r\n", desc_itf->bInterfaceNumber, tu_edpt_number(desc_ep->bEndpointAddress));
    }
  }
}

/*
USB Data format:
https://github.com/torvalds/linux/blob/610a9b8f49fbcf1100716370d3b5f6f884a2835a/drivers/input/joystick/xpad.c#L810
https://www.partsnotincluded.com/understanding-the-xbox-360-wired-controllers-usb-data/

 [0] 0x08 Presence change
 [1] 0x80 Present 0x00 Not present 0x01 Valid input check
 [2] ?? unknown
 [3] 0xf0 Always this value when valid input

 [0]     0x00 Valid input check
 [1]     0x13 Always this value when valid input
 [2]     1 bit per button, 8 buttons
 [3]     1 bit per button, bit 3 unused, 7 buttons
 [4]     LeftTrigger 0 (released) 255 (fully pressed)
 [5]     RightTrigger 0 (released) 255 (fully pressed)
 [6.7]   Left joystick X, 16 bit signed little endian
 [8.9]   Left joystick Y, 16 bit signed little endian
 [10.11] Right joystick X, 16 bit signed little endian
 [12.13] Right joystick Y, 16 bit signed little endian
*/
// Inquiry pad presence https://github.com/torvalds/linux/blob/610a9b8f49fbcf1100716370d3b5f6f884a2835a/drivers/input/joystick/xpad.c#L1369
// LEDs control: https://github.com/torvalds/linux/blob/610a9b8f49fbcf1100716370d3b5f6f884a2835a/drivers/input/joystick/xpad.c#L1581
// Power OFF: https://github.com/torvalds/linux/blob/610a9b8f49fbcf1100716370d3b5f6f884a2835a/drivers/input/joystick/xpad.c#L1762
void vdr_report_received(tuh_xfer_t *xfer)
{
  // Note: not all field in xfer is available for use (i.e filled by tinyusb stack) in callback to save sram
  // For instance, xfer->buffer is NULL. We have used user_data to store buffer when submitted callback
  uint8_t *buf = (uint8_t *)xfer->user_data;
  uint8_t *input_buf = &buf[4];

  if (xfer->result == XFER_RESULT_SUCCESS)
  {
    if (xfer->actual_len >= 2 && buf[0] & 0x08)
    {
      if ((buf[1] & 0x80) != 0)
      {
        // Set LEDs
        printf("Pad present!\r\n");
      }
      else
      {
        printf("Pad NOT present\r\n");
      }
    }
    else if (xfer->actual_len >= 29 && buf[1] == 0x01 && buf[4] == 0x00)
    {
      printf("EP: %02x Data:\r\n", tu_edpt_number(xfer->ep_addr));
      for (uint32_t i = 2; i <= 5; i++)
      {
        printf("%02X ", input_buf[i]);
      }
      printf("%02X%02X/", input_buf[6], input_buf[7]);
      printf("%02X%02X ", input_buf[8], input_buf[9]);
      printf("%02X%02X/", input_buf[10], input_buf[11]);
      printf("%02X%02X\r\n", input_buf[12], input_buf[13]);
    }
  }

  // continue to submit transfer, with updated buffer
  // other field remain the same
  xfer->buflen = 64;
  xfer->buffer = buf;
  tuh_edpt_xfer(xfer);
}

uint8_t *get_vdr_buf(uint8_t daddr)
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

void free_vdr_buf(uint8_t daddr)
{
  for (size_t i = 0; i < BUF_COUNT; i++)
  {
    if (buf_owner[i] == daddr)
      buf_owner[i] = 0;
  }
}

void tuh_mount_cb(uint8_t daddr)
{
  tuh_descriptor_get_device(daddr, &desc_device, sizeof(tusb_desc_device_t), print_device_descriptor, 0);
}

void tuh_umount_cb(uint8_t daddr)
{
  free_vdr_buf(daddr);
}