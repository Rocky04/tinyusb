/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 * Copyright (c) 2023 Rocky04 - Modified for X360 class
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
#include "x360_device.h"

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF PROTYPES
//--------------------------------------------------------------------+

/* Blink pattern
 * - 250 ms  : device not mounted
 * - 1000 ms : device mounted
 * - 2500 ms : device is suspended
 */
enum
{
	BLINK_NOT_MOUNTED  =  250,
	BLINK_MOUNTED      = 1000,
	BLINK_SUSPENDED    = 2500,
};

//--------------------------------------------------------------------+
// INTERNAL OBJECT & FUNCTION DECLARATION
//--------------------------------------------------------------------+

static uint32_t blink_interval_ms = BLINK_NOT_MOUNTED;

void led_blinking_task(void);
void x360_task(void);
void replace_serial_string_descriptor(void);
void replace_serial_string_number(uint16_t * serial, uint8_t len);

//--------------------------------------------------------------------+
// MAIN LOOP
//--------------------------------------------------------------------+

int main(void)
{
	board_init();

	// Init device stack on configured roothub port
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
		x360_task();
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
// X360 TASK
//--------------------------------------------------------------------+

void x360_task(void)
{
	// Poll every 10ms
	const uint32_t interval_ms = 10;

	static uint32_t btn_old = 0;
	static uint32_t start_ms = 0;
	static x360_controls_t controls =
	{
		.buttons.value   = tu_le16toh(0),
		.triggers.value  = tu_le16toh(0),
		.joysticks.LX    = tu_le16toh(0),
		.joysticks.LY    = tu_le16toh(0),
		.joysticks.RX    = tu_le16toh(0),
		.joysticks.RY    = tu_le16toh(0),
		.reserved2       = { 0, 0, 0, 0, 0, 0 },
	};

	// Check if enough time has passed
	if (board_millis() - start_ms < interval_ms)
	{
		// Not enough time
		return;
	}

	// Update the start time
	start_ms += interval_ms;

	// Get the button state
	uint32_t const btn_new = board_button_read();

	// Check if the device is sleeping
	if (tud_suspended() && btn_old)
	{
		// Wake up host if we are in suspend mode
		// and REMOTE_WAKEUP feature is enabled by host
		tud_remote_wakeup();
	}

	// Only sent reports if there was a button state change
	if (btn_old != btn_new)
	{
		// Update the last button state
		btn_old = btn_new;

        // Toggle the X360 home button state
		controls.buttons.home = !controls.buttons.home;

		// Toggle the X360 Y button state
		controls.buttons.Y = !controls.buttons.Y;

		// Send the new X360 controls to host
		x360d_n_report(ITF_NUM_X360, &controls);

		return;
	}
}

//--------------------------------------------------------------------+
// BLINKING TASK
//--------------------------------------------------------------------+
void led_blinking_task(void)
{
	static uint32_t start_ms = 0;
	static bool led_state = false;

	// Quit if blink is disabled
	if (!blink_interval_ms)
	{
		return;
	}

	// Blink every interval ms
	if (board_millis() - start_ms < blink_interval_ms)
	{
        // Not enough time
		return;
	}

	start_ms += blink_interval_ms;

    // Update LED
	board_led_write(led_state);

	// Toggle state
	led_state = 1 - led_state;
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
