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

#if (CFG_TUD_ENABLED && CFG_CUSTOM_HID)

//--------------------------------------------------------------------+
// INCLUDE
//--------------------------------------------------------------------+

#include "tusb.h"
#include "device/usbd_pvt.h"

#include "custom_hid_device.h"

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+

#if CFG_TUSB_DEBUG >= 2
  #define DRIVER_NAME(_name)    .name = _name,
#else
  #define DRIVER_NAME(_name)
#endif

//--------------------------------------------------------------------+
// App class Driver
//--------------------------------------------------------------------+

static const usbd_class_driver_t custom_driver[] =
{
	{
		DRIVER_NAME("CUSTOM_HID")
		.init             = chidd_init,
		.reset            = chidd_reset,
		.open             = chidd_open,
		.control_xfer_cb  = chidd_control_xfer_cb,
		.xfer_cb          = chidd_xfer_cb,
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
	*driver_count = TU_ARRAY_SIZE(custom_driver);

	// Return the descriptor
	return custom_driver;
}

#endif /* CFG_TUD_ENABLED && CFG_APP_X360 */
