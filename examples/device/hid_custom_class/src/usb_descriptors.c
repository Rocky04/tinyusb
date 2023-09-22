/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2023 Rocky04
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
 */

//--------------------------------------------------------------------+
// INCLUDE
//--------------------------------------------------------------------+

#include "tusb.h"
#include "custom_hid_device.h"
#include "device/usbd_pvt.h"

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF PROTYPES
//--------------------------------------------------------------------+

// String descriptor indices, don't account for an extra string descriptor
enum
{
	STRID_LANGID        = 0,
	STRID_MANUFACTURER,
	STRID_PRODUCT,

#ifdef DEVICE_STRING_SERIAL_DEFAULT
	STRID_SERIAL,
#endif

	// Number of regular string descriptors
	STRID_TOTAL,
};

//--------------------------------------------------------------------+
// DEVICE DESCRIPTOR
//--------------------------------------------------------------------+

static  const tusb_desc_device_t desc_device =
{
	.bLength             = sizeof(tusb_desc_device_t),
	.bDescriptorType     = TUSB_DESC_DEVICE,
	.bcdUSB              = tu_le16toh(0x0200),

	.bDeviceClass        = 0x00,
	.bDeviceSubClass     = 0x00,
	.bDeviceProtocol     = 0x00,
	.bMaxPacketSize0     = CFG_TUD_ENDPOINT0_SIZE,

	.idVendor            = tu_le16toh(USB_VID),
	.idProduct           = tu_le16toh(USB_PID),
	.bcdDevice           = tu_le16toh(USB_BCD),

	.iManufacturer       = STRID_MANUFACTURER,
	.iProduct            = STRID_PRODUCT,
	.iSerialNumber       = STRID_SERIAL,

	.bNumConfigurations  = 0x01
};

//--------------------------------------------------------------------+
// HID REPORT DESCRIPTOR
//--------------------------------------------------------------------+

static const uint8_t hid_report_boot_keyboard[] =
{
	HID_USAGE_PAGE        ( HID_USAGE_PAGE_DESKTOP ),
	HID_USAGE             ( HID_USAGE_DESKTOP_KEYBOARD ),
	HID_COLLECTION        ( HID_COLLECTION_APPLICATION ),
		// INPUT
		// 8 bits Modifier Keys (Shift, Control, Alt)
		HID_USAGE_PAGE    ( HID_USAGE_PAGE_KEYBOARD ),
		HID_USAGE_MIN     ( 224 ),
		HID_USAGE_MAX     ( 231 ),
		HID_LOGICAL_MIN   ( 0 ),
		HID_LOGICAL_MAX   ( 1 ),
		HID_REPORT_COUNT  ( 8 ),
		HID_REPORT_SIZE   ( 1 ),
		HID_INPUT         ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE ),
		// 8 bits reserved
		HID_REPORT_COUNT  ( 1 ),
		HID_REPORT_SIZE   ( 8 ),
		HID_INPUT         ( HID_CONSTANT ),
		// 6 bytes Keycodes
		HID_USAGE_PAGE    ( HID_USAGE_PAGE_KEYBOARD ),
		HID_USAGE_MIN     ( 0 ),
		HID_USAGE_MAX_N   ( 255, 2 ),
		HID_LOGICAL_MIN   ( 0 ),
		HID_LOGICAL_MAX_N ( 255, 2 ),
		HID_REPORT_COUNT  ( 6 ),
		HID_REPORT_SIZE   ( 8 ),
		HID_INPUT         ( HID_DATA | HID_ARRAY | HID_ABSOLUTE ),

		// OUTPUT
		// 5 bits LED Indicator Kana | Compose | ScrollLock | CapsLock | NumLock
		HID_USAGE_PAGE    ( HID_USAGE_PAGE_LED ),
		HID_USAGE_MIN     ( 1 ),
		HID_USAGE_MAX     ( 5 ),
		HID_LOGICAL_MIN   ( 0 ),
		HID_LOGICAL_MAX   ( 1 ),
		HID_REPORT_COUNT  ( 5 ),
		HID_REPORT_SIZE   ( 1 ),
		HID_OUTPUT        ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE ),
		// 3 bits LED padding
		HID_REPORT_COUNT  ( 1 ),
		HID_REPORT_SIZE   ( 3 ),
		HID_OUTPUT        ( HID_CONSTANT | HID_ARRAY | HID_ABSOLUTE),
	HID_COLLECTION_END,
};

static const uint8_t hid_report_configuration[] =
{
	HID_USAGE_PAGE_N        ( HID_USAGE_PAGE_VENDOR, 2 ),
	HID_USAGE               ( 1 ),
	HID_COLLECTION          ( HID_COLLECTION_APPLICATION ),
		HID_USAGE           ( 2 ),
		HID_LOGICAL_MIN     ( 0 ),
		HID_LOGICAL_MAX_N   ( 255, 2 ),
		HID_REPORT_COUNT_N  ( 256, 2 ),
		HID_REPORT_SIZE     ( 8 ),
		HID_INPUT           ( HID_DATA | HID_ARRAY | HID_ABSOLUTE ),
	HID_COLLECTION_END,
};

//--------------------------------------------------------------------+
// CONFIGURATION DESCRIPTOR
//--------------------------------------------------------------------+

typedef struct TU_ATTR_PACKED
{
	tusb_desc_configuration_t  conf_1;
	tusb_desc_interface_t      itf_1;
	tusb_hid_descriptor_hid_t  hid_1;
	tusb_desc_endpoint_t       ep_1_in;
	tusb_desc_endpoint_t       ep_1_out;
	tusb_desc_interface_t      itf_2;
	tusb_hid_descriptor_hid_t  hid_2;
	tusb_desc_endpoint_t       ep_2_in;
	tusb_desc_endpoint_t       ep_2_out;
} app_desc_t;

static const app_desc_t desc_configuration[] =
{
	{
		.conf_1 =
		{
			.bLength              = sizeof(tusb_desc_configuration_t),
			.bDescriptorType      = TUSB_DESC_CONFIGURATION,
			.wTotalLength         = tu_le16toh(sizeof(app_desc_t)),
			.bNumInterfaces       = ITF_NUM_TOTAL,
			.bConfigurationValue  = 1,
			.iConfiguration       = 0,
			.bmAttributes         = TU_BIT(7) | TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP,
			.bMaxPower            = 200 / 2,
		},
		.itf_1 =
		{
			.bLength              = sizeof(tusb_desc_interface_t),
			.bDescriptorType      = TUSB_DESC_INTERFACE,
			.bInterfaceNumber     = ITF_NUM_KEYBOARD,
			.bAlternateSetting    = 0,
			.bNumEndpoints        = 2,
			.bInterfaceClass      = TUSB_CLASS_HID,
			.bInterfaceSubClass   = HID_SUBCLASS_BOOT,
			.bInterfaceProtocol   = HID_ITF_PROTOCOL_KEYBOARD,
			.iInterface           = 0,
		},
		.hid_1 =
		{
			.bLength              = sizeof(tusb_hid_descriptor_hid_t),
			.bDescriptorType      = HID_DESC_TYPE_HID,
			.bcdHID               = tu_le16toh(0x0111),
			.bCountryCode         = 0,
			.bNumDescriptors      = 1,
			.bReportType          = HID_DESC_TYPE_REPORT,
			.wReportLength        = sizeof(hid_report_boot_keyboard),
		},
		.ep_1_in =
		{
			.bLength              = sizeof(tusb_desc_endpoint_t),
			.bDescriptorType      = TUSB_DESC_ENDPOINT,
			.bEndpointAddress     = 1 | TUSB_DIR_IN_MASK,
			.bmAttributes =
			{
				.xfer             = TUSB_XFER_INTERRUPT,
				.sync             = 0,
				.usage            = 0,
			},
			.wMaxPacketSize       = tu_le16toh(8),
			.bInterval            = 4,
		},
		.ep_1_out =
		{
			.bLength              = sizeof(tusb_desc_endpoint_t),
			.bDescriptorType      = TUSB_DESC_ENDPOINT,
			.bEndpointAddress     = 1,
			.bmAttributes =
			{
				.xfer             = TUSB_XFER_INTERRUPT,
				.sync             = 0,
				.usage            = 0,
			},
			.wMaxPacketSize       = tu_le16toh(1),
			.bInterval            = 4,
		},
		.itf_2 =
		{
			.bLength              = sizeof(tusb_desc_interface_t),
			.bDescriptorType      = TUSB_DESC_INTERFACE,
			.bInterfaceNumber     = ITF_NUM_CONFIGURATION,
			.bAlternateSetting    = 0,
			.bNumEndpoints        = 2,
			.bInterfaceClass      = TUSB_CLASS_HID,
			.bInterfaceSubClass   = HID_SUBCLASS_NONE,
			.bInterfaceProtocol   = HID_ITF_PROTOCOL_NONE,
			.iInterface           = 0,
		},
		.hid_2 =
		{
			.bLength              = sizeof(tusb_hid_descriptor_hid_t),
			.bDescriptorType      = HID_DESC_TYPE_HID,
			.bcdHID               = tu_le16toh(0x0111),
			.bCountryCode         = 0,
			.bNumDescriptors      = 1,
			.bReportType          = HID_DESC_TYPE_REPORT,
			.wReportLength        = sizeof(hid_report_configuration),
		},
		.ep_2_in =
		{
			.bLength              = sizeof(tusb_desc_endpoint_t),
			.bDescriptorType      = TUSB_DESC_ENDPOINT,
			.bEndpointAddress     = 2 | TUSB_DIR_IN_MASK,
			.bmAttributes =
			{
				.xfer             = TUSB_XFER_INTERRUPT,
				.sync             = 0,
				.usage            = 0,
			},
			.wMaxPacketSize       = tu_le16toh(32),
			.bInterval            = 1,
		},
		.ep_2_out =
		{
			.bLength              = sizeof(tusb_desc_endpoint_t),
			.bDescriptorType      = TUSB_DESC_ENDPOINT,
			.bEndpointAddress     = 2,
			.bmAttributes =
			{
				.xfer             = TUSB_XFER_INTERRUPT,
				.sync             = 0,
				.usage            = 0,
			},
			.wMaxPacketSize       = tu_le16toh(16),
			.bInterval            = 1,
		},
	},
};

//--------------------------------------------------------------------+
// STRING DESCRIPTOR
//--------------------------------------------------------------------+

// Fixed string descriptor for the language IDs
TU_ATTR_ALIGNED(2) static const tusb_desc_string_t desc_string_landid =
{
	.bLength          = sizeof(tusb_desc_string_t) + 2,
	.bDescriptorType  = TUSB_DESC_STRING,
	.unicode_string   = { tu_le16toh(0x0409) },
};

// Fixed string descriptor for the manufacturer name
// Attention, alignment needed to assign wide character string, also utilize the null termination character for the header size
TU_ATTR_ALIGNED(2) static const tusb_desc_string_t desc_string_manufacturer =
{
	.bLength          = sizeof(DEVICE_STRING_MANUFACTURER),
	.bDescriptorType  = TUSB_DESC_STRING,
	.unicode_string   = DEVICE_STRING_MANUFACTURER,
};

// Fixed string descriptor for the product name
// Attention, alignment needed to assign wide character string, also utilize the null termination character for the header size
TU_ATTR_ALIGNED(2) static const tusb_desc_string_t desc_string_product =
{
	.bLength          = sizeof(DEVICE_STRING_PRODUCT),
	.bDescriptorType  = TUSB_DESC_STRING,
	.unicode_string   = DEVICE_STRING_PRODUCT,
};

#ifdef DEVICE_STRING_SERIAL_DEFAULT
// Dynamic string descriptor for the serial number, initialized with a default serial which should be replaced
// Attention, alignment needed to assign wide character string, also utilize the null termination character for the header size
TU_ATTR_ALIGNED(2) static tusb_desc_string_t desc_string_serial =
{
	.bLength          = sizeof(DEVICE_STRING_SERIAL_DEFAULT),
	.bDescriptorType  = TUSB_DESC_STRING,
	.unicode_string   = DEVICE_STRING_SERIAL_DEFAULT,
};
#endif

// String descriptor collection, need to match the STRID order
// A constant list of constant pointers to save RAM
static const void * const desc_strings[] =
{
	&desc_string_landid,
	&desc_string_manufacturer,
	&desc_string_product,

#ifdef DEVICE_STRING_SERIAL_DEFAULT
	&desc_string_serial,
#endif
};

//--------------------------------------------------------------------+
// USBD-CLASS API
//--------------------------------------------------------------------+

// Invoked when the device descriptor is requested
uint8_t const * tud_descriptor_device_cb(void)
{
	return (uint8_t const *) &desc_device;
}

// Invoked when a configuration descriptor is requested
uint8_t const * tud_descriptor_configuration_cb(uint8_t index)
{
	// Ensure that the range of the device descriptor isn't exceeded
	TU_ASSERT(TU_ARRAY_SIZE(desc_configuration) > index);

	return (uint8_t const *) desc_configuration;
}

// Invoked when a string descriptor is requested
uint16_t const * tud_descriptor_string_cb(uint8_t index, uint16_t langid)
{
	(void) langid;

	// String index is not supported
	if (STRID_TOTAL <= index)
	{
		return NULL;
	}

	// Provide the requested fixed string descriptor
	return (uint16_t const *) desc_strings[index];
}

#ifdef DEVICE_STRING_SERIAL_DEFAULT
void replace_serial_string_number(uint16_t * serial, uint8_t len)
{
	// No valid string was given
	if (NULL == serial || 0 == len)
	{
		return;
	}

	// Replace the serial number string
	memcpy(desc_string_serial.unicode_string, serial, len);
}
#endif

// Invoked when the Report descriptor is requested, must return pointer to the descriptor report which must exist long enough for the transfer to complete
uint8_t const * chidd_descriptor_report_cb(uint8_t itf_num, uint16_t * bufsize)
{
	// Ensure the request parameter are right
	if (ITF_NUM_TOTAL <= itf_num)
	{
		return NULL;
	}

	// Handle the different interfaces
	switch (itf_num)
	{
		case ITF_NUM_KEYBOARD:
			*bufsize = sizeof(hid_report_boot_keyboard);

			return hid_report_boot_keyboard;

		case ITF_NUM_CONFIGURATION:
			*bufsize = sizeof(hid_report_configuration);

			return hid_report_configuration;

		// Stall if there is no report for the HID interface
		default:
			return NULL;
	}
}
