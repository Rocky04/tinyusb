/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 * Copyright (c) 2023 Rocky04 - Modified as a custom HID class
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
#if (CFG_TUD_ENABLED && CFG_CUSTOM_HID)

//--------------------------------------------------------------------+
// INCLUDE
//--------------------------------------------------------------------+

#include "device/usbd.h"
#include "device/usbd_pvt.h"

#include "custom_hid_device.h"

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

	// Pointer to the external transfer buffers for sending (IN) and receiving (OUT) reports, implementations needs to take care that the space is valid, need to be big enough to hold the entire report
	uint8_t const * transfer_in_buf;
	uint8_t * transfer_out_buf;
	uint16_t  transfer_out_size;

	// Driver specifics
	uint8_t protocol_mode;                                  // 0 = Boot Protocol, 1 = Report Protocol
	uint8_t idle_rate;                                      // Idle rate for all reports, resolution 4 ms per unit
	tusb_hid_descriptor_hid_t const * hid_descriptor;
} chidd_instance_t;

//--------------------------------------------------------------------+
// INTERNAL OBJECT & FUNCTION DECLARATION
//--------------------------------------------------------------------+

CFG_TUD_MEM_SECTION tu_static chidd_instance_t _chidd_instances[CFG_CUSTOM_HID];

static inline chidd_instance_t * get_free_instance(void);
static inline chidd_instance_t * get_instance_by_ep(uint8_t ep_addr);
static inline chidd_instance_t * get_instance_by_itf(uint8_t itf_num);

static inline bool chidd_standard_request_handler(chidd_instance_t * p_itf, uint8_t stage, tusb_control_request_t const * request);
static inline bool chidd_class_specific_request_handler(chidd_instance_t * p_itf, uint8_t stage, tusb_control_request_t const * request);

//--------------------------------------------------------------------+
// APPLICATION API
//--------------------------------------------------------------------+

// Check if the interface is ready to use
bool chidd_ready(uint8_t itf_num)
{
	// Get the instance
	chidd_instance_t * p_itf = get_instance_by_itf(itf_num);

	// Ensure that an instance was found
	TU_VERIFY(NULL != p_itf);

	return tud_ready() && (p_itf->ep_in != 0) && !usbd_edpt_busy(p_itf->rhport, p_itf->ep_in);
}

// Send an Input Report to host as a new IN transfer by directly using it, buffer must exist long enough for the transfer to complete
bool chidd_send_report(uint8_t itf_num, void const * report, uint16_t len)
{
	// Get the instance
	chidd_instance_t * p_itf = get_instance_by_itf(itf_num);

	// Ensure that an instance was found
	TU_VERIFY(NULL != p_itf);

	// Claim endpoint, can only be done if not busy and not already claimed
	TU_VERIFY(usbd_edpt_claim(p_itf->rhport, p_itf->ep_in));

	// Set or clear the transfer IN buffer
	p_itf->transfer_in_buf = (uint8_t const *) report;

	// Ensure a buffer was given
	TU_VERIFY(NULL != report && len);

	// Handle the transfer if buffer wasn't cleared
	return usbd_edpt_xfer(p_itf->rhport, p_itf->ep_in, (void *)(uintptr_t) p_itf->transfer_in_buf, len);
}

// Prepare to receive an Output Report from the host by provinding a buffer for it, must exist long enough for the transfer to complete
bool chidd_receive_report(uint8_t itf_num, void * report, uint16_t len)
{
	// Get the instance
	chidd_instance_t * p_itf = get_instance_by_itf(itf_num);

	// Ensure that an instance was found
	TU_VERIFY(NULL != p_itf);

	// Ensure that there was a buffer given
	TU_VERIFY(NULL != report && 0 < len);

	// Set the OUT buffer
	p_itf->transfer_out_buf = (uint8_t *) report;
	p_itf->transfer_out_size = len;

	// Prepare the OUT endpoint to be able to receive a new transfer
	return usbd_edpt_xfer(p_itf->rhport, p_itf->ep_out, p_itf->transfer_out_buf, p_itf->transfer_out_size);
}

//--------------------------------------------------------------------+
// USBD-CLASS API
//--------------------------------------------------------------------+

// Initialize the instances
void chidd_init(void)
{
	chidd_reset(0);
}

// Resets the instances
void chidd_reset(uint8_t rhport)
{
	(void) rhport;

	// Reset the instance
	tu_memclr(_chidd_instances, sizeof(_chidd_instances));
}

// Binds a HID descriptor to an instance
uint16_t chidd_open(uint8_t rhport, tusb_desc_interface_t const * desc_itf, uint16_t max_len)
{
	// Check if the given interface is a HID interface
	TU_VERIFY(TUSB_CLASS_HID == desc_itf->bInterfaceClass);

	// Get the length of the belong together descriptors, should be the length until the next interface descriptors appears
	// Here, the length of one interface descriptor, one HID descriptor and the used endpoint descriptors
	uint16_t const drv_len = (uint16_t) (sizeof(tusb_desc_interface_t) + sizeof(tusb_hid_descriptor_hid_t) + (desc_itf->bNumEndpoints * sizeof(tusb_desc_endpoint_t)));

	// The length should never be smaller than expected
	TU_ASSERT(max_len >= drv_len);

	// Find an available (unbound) interface
	chidd_instance_t * p_itf = get_free_instance();

	// Ensure a free interface was found
	TU_ASSERT(p_itf);

	uint8_t const *p_desc = (uint8_t const *) desc_itf;

	// Parse to the next descriptor, which should be the HID descriptor
	p_desc = tu_desc_next(p_desc);

	// Check if the descriptor type matches the expected one
	TU_ASSERT(HID_DESC_TYPE_HID == tu_desc_type(p_desc));

	// Store the descriptor to comply in the case it's requested again later on for some reason
	p_itf->hid_descriptor = (tusb_hid_descriptor_hid_t const *) p_desc;

	// Parse to the next descriptorsr, which should be the first endpoint descriptor
	p_desc = tu_desc_next(p_desc);

	// Check if both endpoints are interrupt endpoints and assign the endpoints of the descriptor to the current driver interface
	TU_ASSERT(usbd_open_edpt_pair(rhport, p_desc, desc_itf->bNumEndpoints, TUSB_XFER_INTERRUPT, &p_itf->ep_out, &p_itf->ep_in));

	// At this point the found driver interface is used

	// Assign the current interface number to the current driver interface
	p_itf->itf_num = desc_itf->bInterfaceNumber;

	// Store the used device port
	p_itf->rhport = rhport;

	// Set default protocol mode
	p_itf->protocol_mode = HID_PROTOCOL_REPORT;

	// Handle the OUT transfer buffer
	if (p_itf->ep_out)
	{
		// Only execute WEAK function if present
		if (chidd_out_endpoint_opened_cb)
		{
			chidd_out_endpoint_opened_cb(p_itf->itf_num);
		}
	}

	return drv_len;
}

// Callback which is invoked when a transfer on the control endpoint occurred for an interface of this class (like a control request or data was received if there is no dedicated OUT endpoint)
bool chidd_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const * request)
{
	// Ensure that it's a request for an interface
	TU_VERIFY(TUSB_REQ_RCPT_INTERFACE == request->bmRequestType_bit.recipient);

	// Get the instance
	chidd_instance_t * p_itf = get_instance_by_itf((uint8_t) request->wIndex);

	// Ensure that an instance was found
	TU_VERIFY(NULL != p_itf);

	// The device port must be the same as initialized
	TU_ASSERT(rhport == p_itf->rhport);

	// Ensure request is for the interface of this instance
	TU_VERIFY(p_itf->itf_num == request->wIndex);

	// Handle the different requests
	switch (request->bmRequestType_bit.type)
	{
		// Interface specific requests
		case TUSB_REQ_TYPE_STANDARD:
			return chidd_standard_request_handler(p_itf, stage, request);

		// Class specific requests
		case TUSB_REQ_TYPE_CLASS:
			return chidd_class_specific_request_handler(p_itf, stage, request);

		// Stall all unsupported request types
		default:
			return false;
	}
}

// Callback which is invoked when a transfer on a non-control endpoint for this class occurred (like when an IN transfer completed or data was received on a dedicated OUT endpoint)
bool chidd_xfer_cb(uint8_t rhport, uint8_t ep_addr, xfer_result_t result, uint32_t xferred_bytes)
{
	// Get the instance
	chidd_instance_t * p_itf = get_instance_by_ep(ep_addr);

	// Ensure that an instance was found
	TU_VERIFY(NULL != p_itf);

	// The device port must be the same as initialized
	TU_ASSERT(rhport == p_itf->rhport);

	// Check if there was a problem
	if (XFER_RESULT_SUCCESS != result)
	{
		// Inform application about the issue, ATTENTION: The application then needs to allow to receive a new transfer for the endpoint
		if (chidd_report_issue_cb)
		{
			chidd_report_issue_cb(p_itf->itf_num, ep_addr, result, xferred_bytes);
		}
		// In the case the application don't care about issues allow a new transfer to be received
		else if (ep_addr == p_itf->ep_out)
		{
			// Prepare the OUT endpoint to be able to receive a new transfer with the same buffer characteristic as before
			TU_ASSERT(usbd_edpt_xfer(p_itf->rhport, p_itf->ep_out, p_itf->transfer_out_buf, p_itf->transfer_out_size));
		}

		return true;
	}

	// Handle a successful sent transfer
	if (ep_addr == p_itf->ep_in)
	{
		// Inform the application
		if (chidd_report_sent_complete_cb)
		{
			chidd_report_sent_complete_cb(p_itf->itf_num, p_itf->transfer_in_buf, xferred_bytes);
		}
	}
	// Handle a successful received transfer
	else if (ep_addr == p_itf->ep_out)
	{
		uint8_t * buffer = p_itf->transfer_out_buf;

		// Reset the OUT buffer
		p_itf->transfer_out_buf = 0;
		p_itf->transfer_out_size = 0;

		// Inform the application, it needs to take care to prepare the receiving before a new report can be received
		if (chidd_report_received_complete_cb)
		{
			chidd_report_received_complete_cb(p_itf->itf_num, 0xFF, HID_REPORT_TYPE_OUTPUT, buffer, xferred_bytes);
		}
	}

	return true;
}

// Get next free instance
static inline chidd_instance_t * get_free_instance(void)
{
	chidd_instance_t * p_itf = _chidd_instances;

	for (uint8_t instance = 0; instance < CFG_CUSTOM_HID; instance++)
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
static inline chidd_instance_t * get_instance_by_ep(uint8_t ep_addr)
{
	chidd_instance_t * p_itf = _chidd_instances;

	for (uint8_t instance = 0; instance < CFG_CUSTOM_HID; instance++)
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
static inline chidd_instance_t * get_instance_by_itf(uint8_t itf_num)
{
	// The preprocessor file guard ensures that there is at least one instance
	chidd_instance_t * p_itf = _chidd_instances;

	for (uint8_t instance = 0; instance < CFG_CUSTOM_HID; instance++)
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

// Handles the standard descriptor requests for the HID class
static inline bool chidd_standard_request_handler(chidd_instance_t * p_itf, uint8_t stage, tusb_control_request_t const * request)
{
	// Caller should ensure that the parameters are valid

	uint8_t const * buffer;
	uint16_t len = 0;

	// This implementation only handles the Get_Descriptor request, the Set_Descriptor request isn't supported
	TU_VERIFY(TUSB_REQ_GET_DESCRIPTOR == request->bRequest);

	// Handle the different requests
	switch (tu_u16_high(request->wValue))
	{
		// The HID descriptor request is mandatory
		case HID_DESC_TYPE_HID:
			// There is no callback to inform application about a successful handled request so only handle the setup stage
			TU_VERIFY(CONTROL_STAGE_SETUP == stage, true);

			// Ensure a HID descriptor is present
			TU_VERIFY(p_itf->hid_descriptor);

			// Send the reply
			TU_VERIFY(tud_control_xfer(p_itf->rhport, request, (void *)(uintptr_t) p_itf->hid_descriptor, p_itf->hid_descriptor->bLength));

			return true;

		// The Report descriptor request is mandatory
		case HID_DESC_TYPE_REPORT:
			// There is no callback to inform application about a successful handled request so only handle the setup stage
			TU_VERIFY(CONTROL_STAGE_SETUP == stage, true);

			// Get the Report descriptor
			buffer = chidd_descriptor_report_cb(p_itf->itf_num, &len);

			// Ensure that a HID report descriptor was given
			TU_VERIFY(NULL != buffer && 0 < len);

			// Send the reply
			tud_control_xfer(p_itf->rhport, request, (void *)(uintptr_t) buffer, len);

			return true;

		// Physical descriptors requests are optional
		case HID_DESC_TYPE_PHYSICAL:
			// There is no callback to inform application about a successful handled request so only handle the setup stage
			TU_VERIFY(CONTROL_STAGE_SETUP == stage, true);

			// Stall if WEAK request is not supported
			if (!chidd_descriptor_physical_cb)
			{
				return false;
			}

			// Get the Physical descriptor
			buffer = chidd_descriptor_physical_cb(p_itf->itf_num, tu_u16_low(request->wValue), &len);

			// Ensure that a HID report descriptor was given
			TU_VERIFY(NULL != buffer && 0 < len);

			// Send the reply
			tud_control_xfer(p_itf->rhport, request, (void *)(uintptr_t) buffer, len);

			return true;

		// Stall all unsupported request types
		default:
			return false;
	}
}

// Handles the class specific requests for the HID class where data is send or received over the control endpoint
static inline bool chidd_class_specific_request_handler(chidd_instance_t * p_itf, uint8_t stage, tusb_control_request_t const * request)
{
	// Caller should ensure that the parameters are valid

	uint16_t len = 0;

	// Handle the different requests
	switch(request->bRequest)
	{
		// Get_Report requests are mandatory for all HID devices
		case HID_REQ_CONTROL_GET_REPORT:
			// Ensure the request is for an IN transfer
			TU_VERIFY(1 == request->bmRequestType_bit.direction);

			// There is no callback to inform application about a successful handled request so only handle the setup stage
			TU_VERIFY(CONTROL_STAGE_SETUP == stage, true);

			// Get the report
			p_itf->transfer_in_buf = chidd_get_report_cb(p_itf->itf_num, tu_u16_low(request->wValue), tu_u16_high(request->wValue), &len);

			// Ensure that a report was given back
			TU_VERIFY(NULL != p_itf->transfer_in_buf && 0 < len);

			// Send the report
			tud_control_xfer(p_itf->rhport, request, (void *)(uintptr_t) p_itf->transfer_in_buf, len);

			return true;

		// Set_Report requests are optional but mandatory when an Output report is specified in the HID Report descriptor
		case HID_REQ_CONTROL_SET_REPORT:
			// Ensure the request is for an OUT transfer
			TU_VERIFY(0 == request->bmRequestType_bit.direction);

			// Handle the request
			if (CONTROL_STAGE_SETUP == stage)
			{
				// Stall if WEAK request is not supported
				if (!chidd_set_report_cb)
				{
					return false;
				}

				// Get the buffer for the Output transfer
				p_itf->transfer_out_buf = chidd_set_report_cb(p_itf->itf_num, tu_u16_low(request->wValue), tu_u16_high(request->wValue), &p_itf->transfer_out_size);

				// Ensure that there was a buffer given back
				TU_VERIFY(NULL != p_itf->transfer_out_buf && 0 < p_itf->transfer_out_size);

				// Prepare to receive a report
				tud_control_xfer(p_itf->rhport, request, p_itf->transfer_out_buf, p_itf->transfer_out_size);
			}
			// Handle when the data was received
			else if (CONTROL_STAGE_ACK == stage)
			{
				// Acknowledge if WEAK request is not supported
				if (!chidd_report_received_complete_cb)
				{
					return true;
				}

				// Inform the application
				chidd_report_received_complete_cb(p_itf->itf_num, tu_u16_low(request->wValue), tu_u16_high(request->wValue), p_itf->transfer_out_buf, request->wLength);
			}

			return true;

		// Get_Idle requests are optional but mandatory for keyboards
		case HID_REQ_CONTROL_GET_IDLE:
			// Ensure the request is for an IN transfer
			TU_VERIFY(1 == request->bmRequestType_bit.direction);

			// There is no callback to inform application about a successful handled request so only handle the setup stage
			TU_VERIFY(CONTROL_STAGE_SETUP == stage, true);

			// Idle rate is for all report IDs
			if (0 == tu_u16_low(request->wValue))
			{
				// Send the general idle duration
				tud_control_xfer(p_itf->rhport, request, &p_itf->idle_rate, sizeof(p_itf->idle_rate));

				return true;
			}

			// Stall Get_Idle request if for a specific report ID and the WEAK request is not supported
			if (!chidd_get_idle_cb)
			{
				return false;
			}

			// Stall request if callback returns false
			TU_VERIFY(chidd_get_idle_cb(p_itf->itf_num, tu_u16_low(request->wValue), (uint8_t *) &len));

			// Send the idle duration
			tud_control_xfer(p_itf->rhport, request, &len, sizeof(uint8_t));

			return true;

		// Set_Idle requests are optional but mandatory for keyboards
		case HID_REQ_CONTROL_SET_IDLE:
			// Ensure the request is for an OUT transfer
			TU_VERIFY(0 == request->bmRequestType_bit.direction);

			// There is no callback to inform application about a successful handled request so only handle the setup stage
			TU_VERIFY(CONTROL_STAGE_SETUP == stage, true);

			// Send ZLP
			tud_control_status(p_itf->rhport, request);

			// Idle rate is for all report IDs
			if (0 == tu_u16_low(request->wValue))
			{
				// Store the general idle rate
				p_itf->idle_rate = tu_u16_high(request->wValue);
			}

			// Stall Get_Idle request if WEAK request is not supported
			if (!chidd_set_idle_cb)
			{
				return false;
			}

			// Stall request if callback returns false
			TU_VERIFY(chidd_set_idle_cb(p_itf->itf_num, tu_u16_low(request->wValue), tu_u16_high(request->wValue)));

			return true;

		// Get_Protocol requests are optional but mandatory for boot devices
		case HID_REQ_CONTROL_GET_PROTOCOL:
			// Ensure the request is for an IN transfer
			TU_VERIFY(1 == request->bmRequestType_bit.direction);

			// There is no callback to inform application about a successful handled request so only handle the setup stage
			TU_VERIFY(CONTROL_STAGE_SETUP == stage, true);

			// Send the protocol mode
			tud_control_xfer(p_itf->rhport, request, &p_itf->protocol_mode, sizeof(p_itf->protocol_mode));

			return true;

		// Set_Protocol requests are optional but mandatory for boot devices
		case HID_REQ_CONTROL_SET_PROTOCOL:
			// Ensure the request is for an OUT transfer
			TU_VERIFY(0 == request->bmRequestType_bit.direction);

			// There is no callback to inform application about a successful handled request so only handle the setup stage
			TU_VERIFY(CONTROL_STAGE_SETUP == stage, true);

			// Send ZLP
			tud_control_status(p_itf->rhport, request);

			// Store the protocol mode
			p_itf->protocol_mode = tu_u16_low(request->wValue);

			// Stall if WEAK request is not supported
			if (!chidd_set_protocol_cb)
			{
				return false;
			}

			// Stall request if callback returns false
			TU_VERIFY(chidd_set_protocol_cb(p_itf->itf_num, (uint8_t) request->wValue));

			return true;

		// Stall all unsupported request types
		default:
			return false;
	}
}

// Provides the current protocol mode, 0 = Boot Protocol, 1 = Report Protocol, return false if instance wasn't found
bool chidd_get_protocol(uint8_t itf_num, uint8_t * mode)
{
	chidd_instance_t * p_itf = get_instance_by_itf(itf_num);

	TU_ASSERT(NULL != p_itf && NULL != mode);

	*mode = p_itf->protocol_mode;

	return true;
}

#endif /* (CFG_TUD_ENABLED && CFG_CUSTOM_HID) */
