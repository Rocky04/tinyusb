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

#include "tusb_option.h"

#if (CFG_TUD_ENABLED && CFG_APP_X360)

//--------------------------------------------------------------------+
// INCLUDE
//--------------------------------------------------------------------+

#include "tusb.h"
#include "device/usbd_pvt.h"

#include "x360_device.h"
#include "ms_os_10_descriptor.h"

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+

#if CFG_TUSB_DEBUG >= 2
  #define DRIVER_NAME(_name)    .name = _name,
#else
  #define DRIVER_NAME(_name)
#endif

//--------------------------------------------------------------------+
// INTERNAL OBJECT & FUNCTION DECLARATION
//--------------------------------------------------------------------+

bool ms_os_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const * request);
TU_ATTR_WEAK uint8_t const * tud_descriptor_ms_compatid_cb(uint16_t * length);
TU_ATTR_WEAK uint8_t const * tud_descriptor_ms_property_cb(uint16_t * length);

//--------------------------------------------------------------------+
// App class Driver
//--------------------------------------------------------------------+

static const usbd_class_driver_t x360_driver[] =
{
	{
		DRIVER_NAME("X360")
		.init             = x360d_init,
		.reset            = x360d_reset,
		.open             = x360d_open,
		.control_xfer_cb  = x360d_control_xfer_cb, // Should be ignored by USBD because the vendor defined type is done by a callback
		.xfer_cb          = x360d_xfer_cb,
		.sof              = NULL,
	},
};

//--------------------------------------------------------------------+
// USBD DRIVER API
//--------------------------------------------------------------------+

// Callback to update the custom driver for usbd, will be joined before the internal drivers so it allows to overload them
TU_ATTR_USED usbd_class_driver_t const * usbd_app_driver_get_cb(uint8_t * driver_count)
{
    // Ensure a pointer was given which can be updated
	TU_ASSERT(driver_count);

	// Provide the amount of the custom drivers
	*driver_count = TU_ARRAY_SIZE(x360_driver);

	// Return the descriptor
	return x360_driver;
}

// Invoked when USBD received a control request for vedor defined type request
TU_ATTR_USED bool tud_vendor_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const * request)
{
	// Try to handle the request by the X360 class driver
	if (x360d_control_xfer_cb(rhport, stage, request))
	{
		return true;
	}

	// Try to handle MS OS 1.0 descriptors
	if (ms_os_control_xfer_cb(rhport, stage, request))
	{
		return true;
	}

	// Request is unknown so stall it
	return false;
}

bool ms_os_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const * request)
{
	// Ensure that it's a vendor-specific request
	TU_VERIFY(TUSB_REQ_TYPE_VENDOR == request->bmRequestType_bit.type);

	// Ensure the request identifier is the same as the propagated one from the MS OS string request
	TU_VERIFY(MS_OS_DESCRIPTOR_VENDORCODE == request->bRequest);

	uint16_t len;

    // Handle the request via the request-specific information
	switch (request->wIndex)
	{
		case MS_EXTENDED_COMPATID_DESCRIPTOR:
			if (tud_descriptor_ms_compatid_cb)
			{
				// There is no callback to inform application about a successful handled request so only handle the setup stage
				TU_VERIFY(CONTROL_STAGE_SETUP == stage, true);

				void * buffer = (void *)(uintptr_t) tud_descriptor_ms_compatid_cb(&len);

				return tud_control_xfer(rhport, request, buffer, len);
			}
			else
			{
				return false;
			}

		case MS_EXTENDED_PROPERTIES_DESCRIPTOR:
			if (tud_descriptor_ms_property_cb)
			{
				// There is no callback to inform application about a successful handled request so only handle the setup stage
				TU_VERIFY(CONTROL_STAGE_SETUP == stage, true);

				void * buffer = (void *)(uintptr_t) tud_descriptor_ms_property_cb(&len);

				return tud_control_xfer(rhport, request, buffer, len);
			}
			else
			{
				return false;
			}

		// Stall all unsupported requests
		default:
			return false;
	}
}

#endif /* CFG_TUD_ENABLED && CFG_APP_X360 */
