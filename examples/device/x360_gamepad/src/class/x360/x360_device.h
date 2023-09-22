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

#ifndef _X360_DEVICE_H_
#define _X360_DEVICE_H_

#include "common/tusb_common.h"

#ifdef __cplusplus
 extern "C" {
#endif

//--------------------------------------------------------------------+
// INCLUDE
//--------------------------------------------------------------------+

#include "x360.h"

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+

// Size the endpoint can transfer in a single transaction
#define X360_ENDPOINT_IN_SIZE          32
#define X360_ENDPOINT_OUT_SIZE         32

// Size of a complete transfer independent for each direction, gets split into multiple transactions if a transfer is bigger than the endpoint can handle (wMaxPacketSize)
#define X360_TRANSFER_IN_BUFFER_SIZE   0x14
#define X360_TRANSFER_OUT_BUFFER_SIZE  0x8

// Descriptor relevant defines
#define X360_CLASS_CONTROL             0xff
#define X360_SUBCLASS_CONTROL          0x5d
#define X360_PROTOCOL_CONTROL          0x01
#define X360_CLASS_SPECIFIC_TYPE       0x21

// Request-specific wValues for the setup stage
#define X360_HANDLE_RUMBLE	0x0000
#define X360_HANDLE_CONTROL	0x0100
#define X360_HANDLE_SERIAL	0x0000

// Specific X360 class report-in descriptor
typedef struct TU_ATTR_PACKED
{
    uint8_t count   : 4;
    uint8_t type    : 4;
    uint8_t ep_addr;
    uint8_t ep_size;
    uint8_t data[4];
} x360_specific_class_report_in_t;

// Specific X360 class report-out descriptor
typedef struct TU_ATTR_PACKED
{
    uint8_t count  : 4;
    uint8_t type   : 4;
    uint8_t ep_addr;
    uint8_t ep_size;
    uint8_t data[2];
} x360_specific_class_report_out_t;

// X360 class Descriptor
typedef struct TU_ATTR_PACKED
{
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t unknown1[3];
    x360_specific_class_report_in_t report_in;
    x360_specific_class_report_out_t report_out;
} x360_desc_specific_class_t;

//--------------------------------------------------------------------+
// INTERNAL CLASS DRIVER API
//--------------------------------------------------------------------+

void      x360d_init             (void);
void      x360d_reset            (uint8_t rhport);
uint16_t  x360d_open             (uint8_t rhport, tusb_desc_interface_t const * desc_itf, uint16_t max_len);
bool      x360d_control_xfer_cb  (uint8_t rhport, uint8_t stage, tusb_control_request_t const * request);
bool      x360d_xfer_cb          (uint8_t rhport, uint8_t ep_addr, xfer_result_t result, uint32_t xferred_bytes);

//--------------------------------------------------------------------+
// APPLICATION API (Multiple Instances)
//--------------------------------------------------------------------+

// Check if the interface is ready to use
bool x360d_n_ready(uint8_t itf_num);

// Send report to host, report must be equal or smaller than the transfer buffer
bool x360d_n_report(uint8_t itf_num, x360_controls_t const * report);

//--------------------------------------------------------------------+
// CALLBACK (Weak is optional)
//--------------------------------------------------------------------+

// Callback which is invoked when there was a communication issue
TU_ATTR_WEAK void x360d_report_issue_cb(uint8_t itf_num, uint8_t ep_addr, xfer_result_t result, uint32_t xferred_bytes);

// Callback which is invoked when the last report was sent successfully to host
TU_ATTR_WEAK void x360d_report_complete_cb(uint8_t itf_num, uint8_t const* report, uint16_t len);

// Callback which is invoked when the a new LED state was received from the host
TU_ATTR_WEAK void x360d_received_led_cb(uint8_t itf_num, x360_led_animations_t led);

// Callback which is invoked when the a new rumble state was received from the host
TU_ATTR_WEAK void x360d_received_rumble_cb(uint8_t itf_num, uint8_t motor_left, uint8_t motor_right);

#ifdef __cplusplus
 }
#endif

#endif /* _X360_DEVICE_H_ */
