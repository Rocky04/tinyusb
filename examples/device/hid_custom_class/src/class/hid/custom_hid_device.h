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
 *
 * This file is part of the TinyUSB stack.
 */

#ifndef _TUSB_CUSTOM_HID_DEVICE_H_
#define _TUSB_CUSTOM_HID_DEVICE_H_

#include "class/hid/hid.h"

#ifdef __cplusplus
 extern "C" {
#endif

//--------------------------------------------------------------------+
// INTERNAL CLASS DRIVER API
//--------------------------------------------------------------------+
void      chidd_init             (void);
void      chidd_reset            (uint8_t rhport);
uint16_t  chidd_open             (uint8_t rhport, tusb_desc_interface_t const * itf_desc, uint16_t max_len);
bool      chidd_control_xfer_cb  (uint8_t rhport, uint8_t stage, tusb_control_request_t const * request);
bool      chidd_xfer_cb          (uint8_t rhport, uint8_t ep_addr, xfer_result_t event, uint32_t xferred_bytes);

//--------------------------------------------------------------------+
// APPLICATION API (Multiple Instances)
//--------------------------------------------------------------------+

// Check if the interface is ready to use
bool chidd_ready(uint8_t itf_num);

// Send an Input Report to host as a new IN transfer by directly using it, buffer must exist long enough for the transfer to complete
bool chidd_send_report(uint8_t itf_num, void const * report, uint16_t len);

// Prepare to receive an Output Report from the host by provinding a buffer for it, must exist long enough for the transfer to complete
bool chidd_receive_report(uint8_t itf_num, void * report, uint16_t len);

// Provides the current protocol mode, 0 = Boot Protocol, 1 = Report Protocol, return false if instance wasn't found
bool chidd_get_protocol(uint8_t itf_num, uint8_t * mode);

//--------------------------------------------------------------------+
// CALLBACK (Weak is optional)
//--------------------------------------------------------------------+

// Invoked when an OUT endpoint is opened, needed so application know when to prepare the buffer for the OUT transfer
// Mandatory when an OUT endpoint is specified in a configuration descriptor
TU_ATTR_WEAK void chidd_out_endpoint_opened_cb(uint8_t itf_num);

// Invoked when the Report descriptor is requested, must return pointer to the descriptor report which must exist long enough for the transfer to complete
uint8_t const * chidd_descriptor_report_cb(uint8_t itf_num, uint16_t * bufsize);

// Invoked when a Physical descriptor is requested, must return pointer to the requested descriptor which must exist long enough for the transfer to complete
// Index zero identifying the number of descriptor sets and their sizes, otherwiese the number of the Physical descriptor beginning with one
TU_ATTR_WEAK uint8_t const * chidd_descriptor_physical_cb(uint8_t itf_num, uint8_t desc_index, uint16_t * bufsize);

// Invoked when an Input Report is requested, must return pointer to the report data which must exist long enough for the transfer to complete
uint8_t const * chidd_get_report_cb(uint8_t itf_num, uint8_t report_id, uint8_t report_type, uint16_t * bufsize);

// Invoked when an Output Report is scheduled, must return pointer to the used buffer which must exist long enough for the transfer to complete
// Mandatory when an Output report is specified in the HID Report descriptor
TU_ATTR_WEAK uint8_t * chidd_set_report_cb(uint8_t itf_num, uint8_t report_id, uint8_t report_type, uint16_t * bufsize);

// Invoked when the host requests the idle duration for a specific report ID, mandatory for keyboards
TU_ATTR_WEAK bool chidd_get_idle_cb(uint8_t itf_num, uint8_t report_id, uint8_t * duration);

// Invoked when the host sets the idle duration for a specific report ID, mandatory for keyboards, resolution 4 ms per unit
TU_ATTR_WEAK bool chidd_set_idle_cb(uint8_t itf_num, uint8_t report_id, uint8_t duration);

// Invoked when the host sets the protocol mode, mandatory for boot devices
TU_ATTR_WEAK bool chidd_set_protocol_cb(uint8_t itf_num, uint8_t protocol_mode);

// Invoked when an Input Report was successfully sent to the host, first byte contains report ID if present
TU_ATTR_WEAK void chidd_report_sent_complete_cb(uint8_t itf_num, uint8_t const* report, uint32_t len);

// Invoked when an Output or Feature Report was successfully received from the host, first byte contains report ID if present
// Mandatory if a dedicated OUT endpoint descriptor is specified, for receiving data this needs to be prepared by calling "chidd_receive_report"
TU_ATTR_WEAK void chidd_report_received_complete_cb(uint8_t itf_num, uint8_t report_id, hid_report_type_t report_type, uint8_t const* report, uint32_t len);

// Invoked when an error occurred on either an IN or OUT endpoint
TU_ATTR_WEAK void chidd_report_issue_cb(uint8_t itf_num, uint8_t ep_addr, xfer_result_t result, uint32_t len);

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_CUSTOM_HID_DEVICE_H_ */
