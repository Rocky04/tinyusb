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
#include "ms_os_10_descriptor.h"
#include "x360_device.h"

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
// Device Descriptors
//--------------------------------------------------------------------+

static const tusb_desc_device_t desc_device =
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
// Configuration Descriptor
//--------------------------------------------------------------------+

typedef struct TU_ATTR_PACKED
{
	tusb_desc_configuration_t conf1;
	tusb_desc_interface_t itf1;
	x360_desc_specific_class_t class1;
	tusb_desc_endpoint_t ep1_in;
	tusb_desc_endpoint_t ep1_out;
} app_desc_t;

static const app_desc_t desc_configuration[] =
{
	{
		.conf1 =
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
		.itf1 =
		{
			.bLength              = sizeof(tusb_desc_interface_t),
			.bDescriptorType      = TUSB_DESC_INTERFACE,
			.bInterfaceNumber     = ITF_NUM_X360,
			.bAlternateSetting    = 0,
			.bNumEndpoints        = 2,
			.bInterfaceClass      = X360_CLASS_CONTROL,
			.bInterfaceSubClass   = X360_SUBCLASS_CONTROL,
			.bInterfaceProtocol   = X360_PROTOCOL_CONTROL,
			.iInterface           = 0,
		},
		.class1 =
		{
			.bLength              = sizeof(x360_desc_specific_class_t),
			.bDescriptorType      = X360_CLASS_SPECIFIC_TYPE,
			.unknown1             = { 0x00, 0x01, 0x01 },
			.report_in =
			{
				.type             = 0x2,
				.count            = sizeof(x360_specific_class_report_in_t) - 2,
				.ep_addr          = 1 | TUSB_DIR_IN_MASK,
				.ep_size          = X360_TRANSFER_IN_BUFFER_SIZE,
				.data             = { 0x00, 0x00, 0x00, 0x00 },
			},
			.report_out =
			{
				.type             = 0x1,
				.count            = sizeof(x360_specific_class_report_out_t) - 2,
				.ep_addr          = 1,
				.ep_size          = X360_TRANSFER_OUT_BUFFER_SIZE,
				.data             = { 0x00, 0x00 },
			},
		},
		.ep1_in =
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
			.wMaxPacketSize       = tu_le16toh(X360_ENDPOINT_IN_SIZE),
			.bInterval            = 4,
		},
		.ep1_out =
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
			.wMaxPacketSize       = tu_le16toh(X360_ENDPOINT_OUT_SIZE),
			.bInterval            = 8,
		},
	},
};

//--------------------------------------------------------------------+
// MS OS 1.0 Descriptor
//--------------------------------------------------------------------+

// MS descriptor to automatically apply the X360 driver on Win
static const ms_extended_compatID_os_feature_descriptor_t ms_driver =
{
	.header =
	{
		.dwLength                   = tu_le32toh(sizeof(ms_extended_compatID_os_feature_descriptor_t)),
		.bcdVersion                 = tu_le16toh(MS_OS_DESCRIPTOR_BCD_VERSION),
		.wIndex                     = tu_le16toh(MS_EXTENDED_COMPATID_DESCRIPTOR),
		.bCount                     = 1,
		.reserved                   = { 0, 0, 0, 0, 0, 0, 0 },
	},
	.functions =
	{
		{
			.bFirstInterfaceNumber  = 0,
			.reserved1              = 0x01,
			.compatibleID           = "XUSB10\0\0",
			.subCompatibleID        = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
			.reserved2              = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
		}
	}
};

//--------------------------------------------------------------------+
// String Descriptors
//--------------------------------------------------------------------+

// Fixed string descriptor for the language IDs
TU_ATTR_ALIGNED(2) static const tusb_desc_string_t desc_string_landid =
{
	.bLength             = sizeof(tusb_desc_string_t) + 2,
	.bDescriptorType     = TUSB_DESC_STRING,
	.unicode_string      = { tu_le16toh(0x0409) },
};

// Fixed string descriptor for the manufacturer name
// Attention, alignment needed to assign wide character string, also utilize the null termination character for the header size
TU_ATTR_ALIGNED(2) static const tusb_desc_string_t desc_string_manufacturer =
{
	.bLength             = sizeof(tusb_desc_string_t) + sizeof(DEVICE_STRING_MANUFACTURER),
	.bDescriptorType     = TUSB_DESC_STRING,
	.unicode_string      = DEVICE_STRING_MANUFACTURER,
};

// Fixed string descriptor for the product name
// Attention, alignment needed to assign wide character string, also utilize the null termination character for the header size
TU_ATTR_ALIGNED(2) static const tusb_desc_string_t desc_string_product =
{
	.bLength             = sizeof(tusb_desc_string_t) + sizeof(DEVICE_STRING_PRODUCT),
	.bDescriptorType     = TUSB_DESC_STRING,
	.unicode_string      = DEVICE_STRING_PRODUCT,
};

#ifdef DEVICE_STRING_SERIAL_DEFAULT
// Dynamic string descriptor for the serial number, initialized with a default serial which should be replaced
// Attention, alignment needed to assign wide character string, also utilize the null termination character for the header size
TU_ATTR_ALIGNED(2) static tusb_desc_string_t desc_string_serial =
{
	.bLength             = sizeof(DEVICE_STRING_SERIAL_DEFAULT),
	.bDescriptorType     = TUSB_DESC_STRING,
	.unicode_string      = DEVICE_STRING_SERIAL_DEFAULT,
};
#endif

// Complete string descriptor for the MS OS 1.00 descriptor
static const ms_os_descriptor_t desc_ms_os =
{
	.bLength             = sizeof(ms_os_descriptor_t),
	.bDescriptorType     = TUSB_DESC_STRING,
	.qwSignature         = MS_OS_DESCRIPTOR_SIGNATURE,
	.bMS_VendorCode      = MS_OS_DESCRIPTOR_VENDORCODE,
	.bPad                = 0,
	.bFlags.ContainerID  = MS_OS_DESCRIPTOR_CONTAINERID_SUPPORT,
};

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

	&desc_ms_os,
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

	// Handle the reordering of the extra index
	if (MS_OS_DESCRIPTOR_STRING_INDEX == index)
	{
		index = STRID_TOTAL;
	}
	// String index is not supported
	else if (STRID_TOTAL <= index)
	{
		return NULL;
	}

	// Provide the requested string descriptor
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

// Invoked when a compatID descriptor is requested
TU_ATTR_USED uint8_t const * tud_descriptor_ms_compatid_cb(uint16_t * length)
{
	// Ensure a pointer was given which can be updated
	TU_ASSERT(length);

	// Provide the length of the descriptor
	*length = sizeof(ms_driver);

	// Return the descriptor
	return (uint8_t const *) &ms_driver;
}
