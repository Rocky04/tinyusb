/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 * Copyright (c) 2023 Rocky04 - Modified for custom HID class
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

//--------------------------------------------------------------------+
// INCLUDE
//--------------------------------------------------------------------+

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "bsp/board_api.h"
#include "tusb.h"
#include "device/usbd_pvt.h"

#include "custom_hid_device.h"

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF PROTYPES
//--------------------------------------------------------------------+

/* Blink pattern
 * - 250 ms  : device not mounted
 * - 1000 ms : device mounted
 * - 2500 ms : device is suspended
 */
enum  {
	BLINK_NOT_MOUNTED  =  250,
	BLINK_MOUNTED      = 1000,
	BLINK_SUSPENDED    = 2500,
};

//--------------------------------------------------------------------+
// INTERNAL OBJECT & FUNCTION DECLARATION
//--------------------------------------------------------------------+

static uint16_t idle_duration = 0;                       // Idling duration until an unchanged report can be resend
static uint8_t report_keyboard_IN[8] = { 0 };            // Keyboard control report Input
static uint8_t report_keyboard_OUT[1] = { 0 };           // Keyboard LED report Output
static uint8_t report_config_IN[256] = { 0 };            // Configuration Input report
static uint8_t report_config_OUT[256] = { 0 };           // Configuration Output report
static uint32_t blink_interval_ms = BLINK_NOT_MOUNTED;

void led_blinking_task(void);
void hid_task(void);
void replace_serial_string_descriptor(void);
void replace_serial_string_number(uint16_t * serial, uint8_t len);

//--------------------------------------------------------------------+
// MAIN LOOP
//--------------------------------------------------------------------+

int main(void)
{
	board_init();

	// init device stack on configured roothub port
	tud_init(BOARD_TUD_RHPORT);

	if (board_init_after_tusb)
	{
		board_init_after_tusb();
	}

#ifdef DEVICE_STRING_SERIAL_DEFAULT
	replace_serial_string_descriptor();
#endif

	while (true)
	{
		// TinyUSB device task
		tud_task();

		// Update LED state
		led_blinking_task();

		// Handling the button input
		hid_task();
	}
}

//--------------------------------------------------------------------+
// DEVICE CALLBACKS
//--------------------------------------------------------------------+

// Invoked when device is mounted
TU_ATTR_USED void tud_mount_cb(void)
{
	blink_interval_ms = BLINK_MOUNTED;
}

// Invoked when device is unmounted
TU_ATTR_USED void tud_umount_cb(void)
{
	blink_interval_ms = BLINK_NOT_MOUNTED;
}

// Invoked when usb bus is suspended
// remote_wakeup_en : if host allow us  to perform remote wakeup
// Within 7ms, device must draw an average of current less than 2.5 mA from bus
TU_ATTR_USED void tud_suspend_cb(bool remote_wakeup_en)
{
	(void) remote_wakeup_en;

	blink_interval_ms = BLINK_SUSPENDED;
}

// Invoked when usb bus is resumed
TU_ATTR_USED void tud_resume_cb(void)
{
	blink_interval_ms = tud_mounted() ? BLINK_MOUNTED : BLINK_NOT_MOUNTED;
}

//--------------------------------------------------------------------+
// USB HID
//--------------------------------------------------------------------+

void hid_task(void)
{
	// Poll every 10ms
	const uint32_t interval_ms     = 10;
	static uint32_t start_ms_poll  = 0;
	static uint32_t start_ms_idle  = 0;
	static uint32_t btn_old        = 0;

	const uint32_t board_millies = board_millis();

	// Check if the keyboard report should be resend
	if (idle_duration && board_millies - start_ms_idle >= idle_duration)
	{
		// Update the start time
		start_ms_idle += idle_duration;

		// Re-send the last keyboard report
		chidd_send_report(ITF_NUM_KEYBOARD, report_keyboard_IN, sizeof(report_keyboard_IN));
	}

	// Check if enough time has passed
	if (board_millies - start_ms_poll < interval_ms)
	{
		// Not enough time has passed
		return;
	}

	// Update the start time
	start_ms_poll += interval_ms;

	// Get the button state
	uint32_t const btn_new = board_button_read();

	// Handle remote wakeup
	if (tud_suspended() && btn_new)
	{
		// Wake up host if we are in suspend mode
		// and REMOTE_WAKEUP feature is enabled by host
		tud_remote_wakeup();

		return;
	}

	// Only sent reports if there was a button state change
	if (btn_old != btn_new)
	{
		// Update the last button state
		btn_old = btn_new;

		// Have something to send for the keyboard interface
		report_keyboard_IN[2] = btn_new ? HID_KEY_A : 0;    // Send the A-key as being pressed if button is pressed

		// Send the keyboard report
		chidd_send_report(ITF_NUM_KEYBOARD, report_keyboard_IN, sizeof(report_keyboard_IN));

		// Send a short config report
		chidd_send_report(ITF_NUM_CONFIGURATION, report_config_IN, 130);

		return;
	}
}
// Invoked when an OUT endpoint is opened, needed so application know when to prepare the buffer for the OUT transfer
TU_ATTR_USED void chidd_out_endpoint_opened_cb(uint8_t itf_num)
{
	// Handle the different interfaces
	switch (itf_num)
	{
		// The buffer for keyboard interface is requested
		case ITF_NUM_KEYBOARD:
			chidd_receive_report(ITF_NUM_KEYBOARD, report_keyboard_OUT, sizeof(report_keyboard_OUT));

			return;

		// The buffer for the configuration interface is requested
		case ITF_NUM_CONFIGURATION:
			chidd_receive_report(ITF_NUM_CONFIGURATION, report_config_OUT, sizeof(report_config_OUT));

			return;

		// Interface is unknown
		default:
			return;
	}
}

// Invoked when an Input Report is requested, must return pointer to the report data which must exist long enough for the transfer to complete
uint8_t const * chidd_get_report_cb(uint8_t itf_num, uint8_t report_id, uint8_t report_type, uint16_t * bufsize)
{
	// Ensure the requested is correct
	if (HID_REPORT_TYPE_INPUT != report_type || 0 != report_id)
	{
		return 0;
	}

	// Handle the different interfaces
	switch (itf_num)
	{
		// The keyboard interface data is requested
		case ITF_NUM_KEYBOARD:
			*bufsize = sizeof(report_keyboard_IN);

			return report_keyboard_IN;

		// The configuration interface data is requested
		case ITF_NUM_CONFIGURATION:
			*bufsize = sizeof(report_config_IN);

			return report_config_IN;

		// Interface is unknown
		default:
			return 0;
	}
}

// Invoked when an Output Report is scheduled, must return pointer to the used buffer which must exist long enough for the transfer to complete
// Mandatory when an Output report is specified in the HID Report descriptor
TU_ATTR_USED uint8_t * chidd_set_report_cb(uint8_t itf_num, uint8_t report_id, uint8_t report_type, uint16_t * bufsize)
{
	(void) report_id;
	(void) report_type;

	// Ensure the request is for the keyboard interface
	if (ITF_NUM_KEYBOARD != itf_num)
	{
		return NULL;
	}

	*bufsize = sizeof(report_keyboard_OUT);

	return report_keyboard_OUT;
}

// Invoked when an Input Report was successfully sent to the host, first byte contains report ID if present
TU_ATTR_USED void chidd_report_sent_complete_cb(uint8_t itf_num, uint8_t const* report, uint32_t len)
{
	(void) report;
	(void) len;

	// Only handle the configuration interface
	if (ITF_NUM_CONFIGURATION != itf_num)
	{
		return;
	}

	// Prepare new data to send for the config interface
	report_config_IN[0]--;
	report_config_IN[129]++;
}

// Invoked when an Output or Feature Report was successfully received from the host, first byte contains report ID if present
// Mandatory if a dedicated OUT endpoint descriptor is specified, for receiving data this needs to be prepared by calling "chidd_receive_report"
TU_ATTR_USED void chidd_report_received_complete_cb(uint8_t itf_num, uint8_t report_id, hid_report_type_t report_type, uint8_t const* report, uint32_t len)
{
	(void) report_id;
	(void) report_type;
	(void) len;

	// Handle the different interfaces
	switch (itf_num)
	{
		case ITF_NUM_KEYBOARD:
			// Ensure only the valid data amount is received
			if (sizeof(report_keyboard_OUT) < len)
			{
				return;
			}

			// Send back the data on the configuration interface to have some variance
			report_config_IN[2] = *report;

			// Prepare receiving so a new outgoing report can be accepted
			chidd_receive_report(ITF_NUM_KEYBOARD, report_keyboard_OUT, sizeof(report_keyboard_OUT));

			return;

		case ITF_NUM_CONFIGURATION:
			// Ensure only the valid data amount is received
			if (sizeof(report_config_OUT) < len)
			{
				return;
			}

			// Prepare new data to send for the config interface
			report_config_IN[1]++;
			report_config_IN[128]--;

			// Prepare receiving so a new outgoing report can be accepted
			chidd_receive_report(ITF_NUM_CONFIGURATION, report_config_OUT, sizeof(report_config_OUT));

			return;

		default:
			return;
	}
}

// Invoked when the host requests the idle duration for a specific report ID, mandatory for keyboards
TU_ATTR_USED bool chidd_get_idle_cb(uint8_t itf_num, uint8_t report_id, uint8_t * duration)
{
	// Ensure the request is for the keyboard interface and no report ID
	if (ITF_NUM_KEYBOARD != itf_num || 0 != report_id)
	{
		return false;
	}

	// Provide the duration, resolution is 4 ms per unit
	*duration = 0xFF & (idle_duration / 4);

	return true;
}

// Invoked when the host sets the idle duration for a specific report ID, mandatory for keyboards, resolution 4 ms per unit
TU_ATTR_USED bool chidd_set_idle_cb(uint8_t itf_num, uint8_t report_id, uint8_t duration)
{
	// Ensure the request is for the keyboard interface and no report ID
	if (ITF_NUM_KEYBOARD != itf_num || 0 != report_id)
	{
		return false;
	}

	// Update the internal invall according to the duration, resolution is 4 ms per unit
	idle_duration = duration * 4;

	return true;
}

//--------------------------------------------------------------------+
// BLINKING TASK
//--------------------------------------------------------------------+

void led_blinking_task(void)
{
	static uint32_t start_ms = 0;
	static bool led_state = false;

	// Bink is disabled
	if (!blink_interval_ms) return;

	// Blink every interval
	if ( board_millis() - start_ms < blink_interval_ms)
	{
		// Not enough time
		return;
	}

	start_ms += blink_interval_ms;

	board_led_write(led_state);
	led_state = 1 - led_state; // toggle
}

#ifdef DEVICE_STRING_SERIAL_DEFAULT
// Replace the default serial number for a dynamic one
void replace_serial_string_descriptor(void)
{
	// Use buffer to avoid compiler warning about potential alignment issues on packed structs
	uint16_t string_buffer[sizeof(DEVICE_STRING_SERIAL_DEFAULT) - 2];

	// Copy the serial number into the buffer, the null termination character isn't needed
	uint8_t len = 0xFF & board_usb_get_serial(string_buffer, sizeof(DEVICE_STRING_SERIAL_DEFAULT) - 2);

	// Set the new serial number
	replace_serial_string_number(string_buffer, len);
}
#endif
