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

// Only add the code if it's needed
#if (CFG_TUD_ENABLED && CFG_APP_X360)

//--------------------------------------------------------------------+
// INCLUDE
//--------------------------------------------------------------------+

#include "device/usbd.h"
#include "device/usbd_pvt.h"

#include "x360_device.h"

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+

typedef struct
{
	// Endpoints and interface number to identify the instance
	uint8_t rhport;
	uint8_t itf_num;
	uint8_t ep_in;
	uint8_t ep_out;

	// Dedicated transfer buffers for sending (IN) and receiving (OUT) reports, must be big enough to hold the entire report
	CFG_TUSB_MEM_ALIGN uint8_t transfer_in_buf[X360_TRANSFER_IN_BUFFER_SIZE];
	CFG_TUSB_MEM_ALIGN uint8_t transfer_out_buf[X360_TRANSFER_OUT_BUFFER_SIZE];

	// Driver specifics
	x360_led_animations_t led;
} x360d_instance_t;

//--------------------------------------------------------------------+
// INTERNAL OBJECT & FUNCTION DECLARATION
//--------------------------------------------------------------------+

CFG_TUD_MEM_SECTION tu_static x360d_instance_t _x360_instances[CFG_APP_X360];

static void x360d_report_out_received(x360d_instance_t * instance, uint16_t xferred_bytes);

static inline x360d_instance_t * get_free_instance(void);
static inline x360d_instance_t * get_instance_by_ep(uint8_t ep_addr);
static inline x360d_instance_t * get_instance_by_itf(uint8_t itf_num);

static inline bool x360d_interface_request_handler(x360d_instance_t * p_itf, uint8_t stage, tusb_control_request_t const * request);
static inline bool x360d_device_request_handler(x360d_instance_t * p_itf, uint8_t stage, tusb_control_request_t const * request);

//--------------------------------------------------------------------+
// APPLICATION API
//--------------------------------------------------------------------+

// Check if the interface is ready to use
bool x360d_n_ready(uint8_t itf_num)
{
	// Get the instance
	x360d_instance_t * p_itf = get_instance_by_itf(itf_num);

	// Ensure that an instance was found
	TU_VERIFY(NULL != p_itf);

	return tud_ready() && (p_itf->ep_in != 0) && !usbd_edpt_busy(p_itf->rhport, p_itf->ep_in);
}

// Send report data to host as a new transfer
bool x360d_n_report(uint8_t itf_num, x360_controls_t const * report)
{
	// Get the instance
	x360d_instance_t * p_itf = get_instance_by_itf(itf_num);

	// Ensure that an instance was found
	TU_VERIFY(NULL != p_itf);

	// Claim endpoint, can only be done if not busy and not already claimed
	TU_VERIFY(usbd_edpt_claim(p_itf->rhport, p_itf->ep_in));

	// Calculate the size of the entire report
	uint16_t len = sizeof(x360_message_controls_t);

	// Ensure the transmission buffer is big enough to hold the entire report
	TU_VERIFY(len <= sizeof(p_itf->transfer_in_buf));

	x360_message_controls_t * buffer = (x360_message_controls_t *) p_itf->transfer_in_buf;

	// Prepare the report message
	buffer->x360_message_header.type = X360_MESSAGE_TYPE_IN_INPUT;
	buffer->x360_message_header.length = sizeof(x360_message_controls_t);
	memcpy(&buffer->x360_controls, report, sizeof(x360_controls_t));

	// Handle the transfer
	return usbd_edpt_xfer(p_itf->rhport, p_itf->ep_in, p_itf->transfer_in_buf, len);
}

//--------------------------------------------------------------------+
// USBD-CLASS API
//--------------------------------------------------------------------+

void x360d_init(void)
{
	x360d_reset(0);
}

void x360d_reset(uint8_t rhport)
{
	(void) rhport;

	// Reset the instance
	tu_memclr(_x360_instances, sizeof(_x360_instances));
}

// Try to bind the driver to an interface
uint16_t x360d_open(uint8_t rhport, tusb_desc_interface_t const * desc_itf, uint16_t max_len)
{
	// Check if the given interface is a X360 interface by its specific unofficial class code
	TU_VERIFY(
		X360_CLASS_CONTROL     == desc_itf->bInterfaceClass &&
		X360_SUBCLASS_CONTROL  == desc_itf->bInterfaceSubClass &&
		X360_PROTOCOL_CONTROL  == desc_itf->bInterfaceProtocol, 0
	);

	// Get the length of the belong together descriptors, should be the length until the next interface descriptors appears
	// Here, the length of one interface descriptor, one class specific descriptor and two endpoint descriptors
	uint16_t const drv_len = (uint16_t) (sizeof(tusb_desc_interface_t) + sizeof(x360_desc_specific_class_t) + (desc_itf->bNumEndpoints * sizeof(tusb_desc_endpoint_t)));

	// The length should never be smaller than expected
	TU_ASSERT(max_len >= drv_len);

	// Find an available (unbound) interface
	x360d_instance_t * p_itf = get_free_instance();

	// Ensure a free interface was found
	TU_ASSERT(p_itf);

	uint8_t const *p_desc = (uint8_t const *) desc_itf;

	// Parse to the next descriptor, which should be the special class-specific descriptor
	p_desc = tu_desc_next(p_desc);

	// Check if the descriptor type matches the expected one
	TU_ASSERT(X360_CLASS_SPECIFIC_TYPE == tu_desc_type(p_desc));

	// Parse to the next descriptorsr, which should be the first endpoint descriptor
	p_desc = tu_desc_next(p_desc);

	// Check if both endpoints are interrupt endpoints and assign the endpoints of the descriptor to the current driver interface
	TU_ASSERT(usbd_open_edpt_pair(rhport, p_desc, desc_itf->bNumEndpoints, TUSB_XFER_INTERRUPT, &p_itf->ep_out, &p_itf->ep_in));

	// At this point the found driver interface is used

	// Assign the current interface number to the current driver interface
	p_itf->itf_num = desc_itf->bInterfaceNumber;

	// Store the device port
	p_itf->rhport = rhport;

	// Prepare the output endpoint to be able to receive a new transfer
	if (!p_itf->ep_out)
	{
		return drv_len;
	}

	// Prepare the output endpoint to be able to receive a transaction
	if (!usbd_edpt_xfer(rhport, p_itf->ep_out, p_itf->transfer_out_buf, X360_TRANSFER_OUT_BUFFER_SIZE))
	{
		TU_LOG_FAILED();
		TU_BREAKPOINT();
	}

	return drv_len;
}

// Callback which is invoked when a transfer on the control endpoint occurred for an interface of this class
bool x360d_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const * request)
{
	// Ensure that it's a vendor-specific driver request
	TU_VERIFY(TUSB_REQ_TYPE_VENDOR == request->bmRequestType_bit.type);

	// Ensure the request identifier is correct
	TU_VERIFY(1 == request->bRequest);

	// Get the instance
	x360d_instance_t * p_itf = get_instance_by_itf((uint8_t) request->wIndex);

	// Ensure that an instance was found
	TU_VERIFY(NULL != p_itf);

	// The device port must be the same as initialized
	TU_ASSERT(rhport == p_itf->rhport);

	// Ensure request is for the interface of this instance
	TU_VERIFY(p_itf->itf_num == request->wIndex);

	// Handle the different requests
	switch (request->bmRequestType_bit.recipient)
	{
		// Request if for a specific X360 gamepad
		case TUSB_REQ_RCPT_INTERFACE:
			return x360d_interface_request_handler(p_itf, stage, request);

		// Request is for the entire X360 device
		case TUSB_REQ_RCPT_DEVICE:
			return x360d_device_request_handler(p_itf, stage, request);

		// Stall all unsupported request types
		default:
			return false;
	}
}

// Callback which is invoked when a transfer on a non-control endpoint for this class occurred
bool x360d_xfer_cb(uint8_t rhport, uint8_t ep_addr, xfer_result_t result, uint32_t xferred_bytes)
{
	// Get the instance
	x360d_instance_t * p_itf = get_instance_by_ep(ep_addr);

	// Ensure that an instance was found
	TU_ASSERT(NULL != p_itf);

	// The device port must be the same as initialized
	TU_ASSERT(rhport == p_itf->rhport);

	// Check if there was a problem
	if (XFER_RESULT_SUCCESS != result)
	{
		// Inform application about the issue, ATTENTION: The application then needs to allow to receive a new transfer for the endpoint
		if (x360d_report_issue_cb)
		{
			x360d_report_issue_cb(p_itf->itf_num, ep_addr, result, xferred_bytes);
		}
		// In the case the application don't care about issues allow a new transfer to be received
		else if (ep_addr == p_itf->ep_out)
		{
			// Prepare the OUT endpoint to be able to receive a new transfer
			TU_ASSERT(usbd_edpt_xfer(p_itf->rhport, p_itf->ep_out, p_itf->transfer_out_buf, X360_TRANSFER_OUT_BUFFER_SIZE));
		}

		return true;
	}

	// Handle a successful sent transfer
	if (ep_addr == p_itf->ep_in)
	{
		// Inform the application
		if (x360d_report_complete_cb)
		{
			x360d_report_complete_cb(p_itf->itf_num, p_itf->transfer_in_buf, xferred_bytes);
		}
	}
	// Handle a successful received transfer
	else if (ep_addr == p_itf->ep_out)
	{
		// Handle a successful received transfer
		x360d_report_out_received(p_itf, xferred_bytes);

		// Prepare the OUT endpoint to be able to receive a new transfer
		TU_ASSERT(usbd_edpt_xfer(p_itf->rhport, p_itf->ep_out, p_itf->transfer_out_buf, X360_TRANSFER_OUT_BUFFER_SIZE));
	}

	return true;
}

// Handle a X360 OUT transfer
static void x360d_report_out_received(x360d_instance_t * p_itf, uint16_t xferred_bytes)
{
	// Caller should ensure that the parameters are valid

	x360_message_header_t * x360_message = (x360_message_header_t *) p_itf->transfer_out_buf;

	// Check if a new rumble command was received
	if (sizeof(x360_message_rumble_t) == xferred_bytes && X360_MESSAGE_TYPE_OUT_RUMBLE == x360_message->type)
	{
		// Inform application about the rumble
		if (x360d_received_rumble_cb)
		{
			x360d_received_rumble_cb(p_itf->itf_num, ((x360_message_rumble_t *) x360_message)->rumble_left, ((x360_message_rumble_t *) x360_message)->rumble_right);
		}
	}
	// Check if a new LED animation was received
	else if (sizeof(x360_message_led_t) == xferred_bytes && X360_MESSAGE_TYPE_OUT_LED == x360_message->type)
	{
		x360_led_animations_t led = (x360_led_animations_t) (((x360_message_led_t *)x360_message)->led);

		// Only handle a LED animation change
		if (p_itf->led == led)
		{
			return;
		}

		// Store the new LED animation state
		p_itf->led = led;

		// Inform application about the LED animation change
		if (x360d_received_led_cb)
		{
			x360d_received_led_cb(p_itf->itf_num, led);
		}
	}
}

// Get next free instance
static inline x360d_instance_t * get_free_instance(void)
{
	x360d_instance_t * p_itf = _x360_instances;

	for (uint8_t instance = 0; instance < CFG_APP_X360; instance++)
	{
		// When both endpoints are unbound instance is considered free
		if (0 == p_itf->ep_in && 0 == p_itf->ep_out)
		{
			// A free interface was found
			return p_itf;
		}

		p_itf++;
	}

	// No free instance was found
	return NULL;
}

// Identify instance by endpoint number
static inline x360d_instance_t * get_instance_by_ep(uint8_t ep_addr)
{
	x360d_instance_t * p_itf = _x360_instances;

	for (uint8_t instance = 0; instance < CFG_APP_X360; instance++)
	{
		// Check if instance has the endpoint which is looked for
		if (ep_addr == p_itf->ep_in || ep_addr == p_itf->ep_out)
		{
			// The instance interface was found
			return p_itf;
		}

		p_itf++;
	}

	// Instance not found
	return NULL;
}

// Identify instance by interface number
static inline x360d_instance_t * get_instance_by_itf(uint8_t itf_num)
{
	x360d_instance_t * p_itf = _x360_instances;

	for (uint8_t instance = 0; instance < CFG_APP_X360; instance++)
	{
		// Check if instance has the interface which is looked for while not free
		if (itf_num == p_itf->itf_num && (0 != p_itf->ep_in || 0 != p_itf->ep_out))
		{
			// The instance interface was found
			return p_itf;
		}

		p_itf++;
	}

	// Instance not found
	return NULL;
}

// Handle a X360 interface request
static inline bool x360d_interface_request_handler(x360d_instance_t * p_itf, uint8_t stage, tusb_control_request_t const * request)
{
	(void) p_itf;
	uint16_t len;

#ifdef X360_RUMBLE_SUPPORT
	x360_message_rumble_t rumble =
	{
		.x360_message_header =
		{
			.type    = X360_MESSAGE_TYPE_OUT_RUMBLE,
			.length  = sizeof(x360_message_rumble_t),
		},
		.reserved1   = 0,
		.rumble      = X360_RUMBLE_SUPPORT,
		.reserved2   = { 0, 0, 0 },
	};
#endif /* X360_RUMBLE_SUPPORT */

#ifdef X360_INPUT_SUPPORT
	x360_message_controls_t controls =
	{
		.x360_message_header =
		{
			.type             = X360_MESSAGE_TYPE_IN_INPUT,
			.length           = sizeof(x360_message_controls_t),
		},
		.x360_controls.bytes  = X360_INPUT_SUPPORT,
	};
#endif /* X360_INPUT_SUPPORT */

	// Caller should ensure that the parameters are valid

	switch (request->wValue)
	{
#ifdef X360_RUMBLE_SUPPORT
		// Handle the supported rumble feature
		case X360_HANDLE_RUMBLE:
			// There is no callback to inform application about a successful handled request so only handle the setup stage
			TU_VERIFY(CONTROL_STAGE_SETUP == stage, true);

			len = sizeof(x360_message_rumble_t);

			// Ensure the buffer is big enough
			TU_ASSERT(X360_TRANSFER_IN_BUFFER_SIZE >= len);

			// Copy the message to the buffer
			memcpy(p_itf->transfer_in_buf, &rumble, sizeof(x360_message_rumble_t));

			// Send the reply
			return tud_control_xfer(p_itf->rhport, request, p_itf->transfer_in_buf, len);
#endif

#ifdef X360_INPUT_SUPPORT
		// Handle the supported controls
		case X360_HANDLE_CONTROL:
			// There is no callback to inform application about a successful handled request so only handle the setup stage
			TU_VERIFY(CONTROL_STAGE_SETUP == stage, true);

			len = sizeof (x360_message_controls_t);

			// Ensure the buffer is big enough
			TU_ASSERT(X360_TRANSFER_IN_BUFFER_SIZE >= len);

			// Copy the message to the buffer
			memcpy(p_itf->transfer_in_buf, &controls, sizeof(x360_message_controls_t));

			// Send the reply
			return tud_control_xfer(p_itf->rhport, request, p_itf->transfer_in_buf, len);
#endif

		// Stall all unsupported requests
		default:
			return false;
	}
}

// Handle the X360 device requests
static inline bool x360d_device_request_handler(x360d_instance_t * p_itf, uint8_t stage, tusb_control_request_t const * request)
{
	// Caller should ensure that the parameters are valid

	(void) p_itf;

	switch (request->wValue)
	{
#ifdef X360_SERIAL_NUMBER
		// Handle the serial number of the x360 device
		case X360_HANDLE_SERIAL:
			// There is no callback to inform application about a successful handled request so only handle the setup stage
			TU_VERIFY(CONTROL_STAGE_SETUP == stage, true);

			// Prepare the reply for the X360 serial number
			TU_VERIFY(0 == tu_memcpy_s(p_itf->transfer_in_buf, X360_TRANSFER_IN_BUFFER_SIZE, X360_SERIAL_NUMBER, sizeof(X360_SERIAL_NUMBER) - 1));

			// Send the reply
			return tud_control_xfer(p_itf->rhport, request, p_itf->transfer_in_buf, sizeof(X360_SERIAL_NUMBER) - 1);
#endif

		// Stall all unsupported requests
		default:
			return false;
	}
}

#endif /* (CFG_TUD_ENABLED && CFG_APP_X360) */
