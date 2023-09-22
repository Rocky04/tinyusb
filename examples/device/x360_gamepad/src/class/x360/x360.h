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

#ifndef _X360_H_
#define _X360_H_

#include "common/tusb_common.h"

#ifdef __cplusplus
 extern "C" {
#endif

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+

// X360 message types for report-out data from the host to the device
typedef enum
{
	X360_MESSAGE_TYPE_OUT_RUMBLE  = 0x00,               // Identify the message as rumble controls
	X360_MESSAGE_TYPE_OUT_LED     = 0x01,               // Identify the message as LED state controls
} x360_message_type_out_t;

// X360 message types for report-in data from the device to the host
typedef enum
{
	X360_MESSAGE_TYPE_IN_INPUT    = 0x00,               // Identify the message as rumble controls
	X360_MESSAGE_TYPE_IN_LED      = 0x01,               // Identify the message as LED state controls
} x360_message_type_in_t;

// X360 LED animation types for the x360 home button
typedef enum
{
	X360_LED_ALL_OFF              = 0x00,               // No LED, typically for an unset slot, like of a 5th controller
	X360_LED_ALL_BLINKING         = 0x01,               // All blinking for 2 seconds, then back to previous LED state
	X360_LED_SLOT_1_FLASH         = 0x02,               // Short flashing on 1 then stay on it, typically for first connection with initialisation for slot 1
	X360_LED_SLOT_2_FLASH         = 0x03,               // Short flashing on 2 then stay on it, typically for first connection with initialisation for slot 2
	X360_LED_SLOT_3_FLASH         = 0x04,               // Short flashing on 3 then stay on it, typically for first connection with initialisation for slot 3
	X360_LED_SLOT_4_FLASH         = 0x05,               // Short flashing on 4 then stay on it, typically for first connection with initialisation for slot 4
	X360_LED_SLOT_1_ON            = 0x06,               // Stay on 1, typically for slot 1
	X360_LED_SLOT_2_ON            = 0x07,               // Stay on 2, typically for slot 2
	X360_LED_SLOT_3_ON            = 0x08,               // Stay on 3, typically for slot 3
	X360_LED_SLOT_4_ON            = 0x09,               // Stay on 4, typically for slot 4
	X360_LED_ROTATING             = 0x0a,               // LED rotational blinking (1 -> 2 -> 4 -> 3 -> 1) forever, typical during pairing
	X360_LED_BLINKING_FAST        = 0x0b,               // Fast blinking (current slot) for 8 seconds, typical during initialisation
	X360_LED_BLINKING_SLOW        = 0x0c,               // Forever slow blinking (current slot), typical during connection attempts
	X360_LED_ALTERNATING          = 0x0d,               // Alternating blinking (1 & 4 -> 2 & 3 -> 1 & 4) for 8 seconds, typical indication for low battery
	X360_LED_INIT                 = 0x0e,               // Forever slow blinking, initial state
	X360_LED_BLINK_ONCE           = 0x0f,               // Blink once then off
} x360_led_animations_t;

// X360 structure for the message header
typedef struct TU_ATTR_PACKED
{
	uint8_t type;                               // Identifier of the messages type
	uint8_t length;                                     // Length of the complete message
} x360_message_header_t;

// X360 control structure
typedef struct TU_ATTR_PACKED
{
	union
	{
		uint8_t bytes[18];
		union
		{
			uint16_t value;                             // Value for all the buttons
			struct
			{
				uint8_t up         : 1;                 // Value for the up arrow of the D-pad
				uint8_t down       : 1;                 // Value for the down arrow of the D-pad
				uint8_t left       : 1;                 // Value for the left arrow of the D-pad
				uint8_t right      : 1;                 // Value for the right arrow of the D-pad
				uint8_t start      : 1;                 // Value for the Start button
				uint8_t back       : 1;                 // Value for the back button
				uint8_t L3         : 1;                 // Value for the left joystick button
				uint8_t R3         : 1;                 // Value for the right joystick button
				uint8_t LB         : 1;                 // Value for the left bumper
				uint8_t RB         : 1;                 // Value for the right bumper
				uint8_t home       : 1;                 // Value for the home button
				uint8_t reserved1  : 1;                 // Reserved / unused bytes
				uint8_t A          : 1;                 // Value for the A face button
				uint8_t B          : 1;                 // Value for the B face button
				uint8_t X          : 1;                 // Value for the X face button
				uint8_t Y          : 1;                 // Value for the Y face button
			};
		} buttons;
		union
		{
			uint16_t value;                             // Value for all trigger
			struct
			{
				uint8_t LT;                             // Value for the left trigger
				uint8_t RT;                             // Value for the right trigger
			};
		} triggers;
		struct
		{
			struct
			{
				int16_t LX;                             // Value for the X axis of the left joystick
				int16_t LY;                             // Value for the Y axis of the left joystick
			};
			struct
			{
				int16_t RX;                             // Value for the X axis of the right joystick
				int16_t RY;                             // Value for the Y axis of the right joystick
			};
		} joysticks;
		struct
		{
			uint8_t reserved2[6];                      // Reserved / unused bytes
		};
	};
} x360_controls_t;

typedef struct TU_ATTR_PACKED
{
	union
	{
		uint8_t bytes[20];
		struct
		{
			x360_message_header_t x360_message_header;  // Structure of the message header
			x360_controls_t x360_controls;              // Structure of the controls
		};
	};
} x360_message_controls_t;

// X360 structure for the input message to control the force feedback rumble motors
typedef struct TU_ATTR_PACKED
{
	union
	{
		uint8_t bytes[8];
		struct
		{
			x360_message_header_t x360_message_header;  // Structure of the message header
			uint8_t reserved1;                          // Reserved / unused byte
			union
			{
				uint8_t rumble[2];
				struct
				{
					uint8_t rumble_left;                // Value for the left motor with the large weight
					uint8_t rumble_right;               // Value for the right motor with the small weight
				};
			};
			uint8_t reserved2[3];                       // Reserved / unused bytes
		};
	};
} x360_message_rumble_t;

// X360 structure for the input messages to control the LED animation
typedef struct TU_ATTR_PACKED
{
	union
	{
		uint8_t bytes[3];
		struct
		{
			x360_message_header_t x360_message_header;  // Structure of the message header
			uint8_t led;								// Value for the LED animation of type X360_LED_ANIMATIONS
		};
	};
} x360_message_led_t;

#ifdef __cplusplus
 }
#endif

#endif /* _X360_H_ */
