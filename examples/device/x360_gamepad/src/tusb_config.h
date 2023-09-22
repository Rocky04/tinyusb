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

#ifndef _TUSB_CONFIG_H_
#define _TUSB_CONFIG_H_

#ifdef __cplusplus
 extern "C" {
#endif

//--------------------------------------------------------------------+
// Board Specific Configuration
//--------------------------------------------------------------------+

// RHPort number used for device can be defined by board.mk, default to port 0
#ifndef BOARD_TUD_RHPORT
 #define BOARD_TUD_RHPORT        0
#endif

// RHPort max operational speed can defined by board.mk
#ifndef BOARD_TUD_MAX_SPEED
 #define BOARD_TUD_MAX_SPEED     OPT_MODE_DEFAULT_SPEED
#endif

//--------------------------------------------------------------------
// COMMON CONFIGURATION
//--------------------------------------------------------------------

#ifndef CFG_TUD_MEM_SECTION
  #define CFG_TUD_MEM_SECTION
#endif

#ifndef tu_static
  #define tu_static static
#endif

// Defined by compiler flags for flexibility
#ifndef CFG_TUSB_MCU
 #error CFG_TUSB_MCU must be defined
#endif

#ifndef CFG_TUSB_OS
 #define CFG_TUSB_OS             OPT_OS_NONE
#endif

#ifndef CFG_TUSB_DEBUG
 #define CFG_TUSB_DEBUG          0
#endif

// Enable Device stack
#define CFG_TUD_ENABLED          1

// Default is max speed that hardware controller could support with on-chip PHY
#define CFG_TUD_MAX_SPEED        BOARD_TUD_MAX_SPEED

/* USB DMA on some MCUs can only access a specific SRAM region with restriction on alignment.
 * Tinyusb use follows macros to declare transferring memory so that they can be put
 * into those specific section.
 * e.g
 * - CFG_TUSB_MEM SECTION : __attribute__ (( section(".usb_ram") ))
 * - CFG_TUSB_MEM_ALIGN   : __attribute__ ((aligned(4)))
 */
#ifndef CFG_TUSB_MEM_SECTION
 #define CFG_TUSB_MEM_SECTION
#endif

#ifndef CFG_TUSB_MEM_ALIGN
 #define CFG_TUSB_MEM_ALIGN      __attribute__ ((aligned(4)))
#endif

//--------------------------------------------------------------------
// DEVICE CONFIGURATION
//--------------------------------------------------------------------

// General device settings
#define USB_VID                      0xCAFE
#define USB_PID                      0x1234
#define USB_BCD                      0x0100

// String descriptors
#define DEVICE_STRING_MANUFACTURER   L"TinyUSB"
#define DEVICE_STRING_PRODUCT        L"TinyUSB X360"

// The default serial number, also limits its max length
#define DEVICE_STRING_SERIAL_DEFAULT  L"000000"

// MS OS 1.0 descriptor settings
#define MS_OS_DESCRIPTOR_VENDORCODE  0x42

// X360 settings
#define CFG_APP_X360                 1
#define X360_SERIAL_NUMBER           "ABC"
#define X360_RUMBLE_SUPPORT          { 0x0, 0x0 }
#define X360_INPUT_SUPPORT           { 0xFF, 0xF7, 0xFF, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }

// Size of the control endpoint
#ifndef CFG_TUD_ENDPOINT0_SIZE
 #define CFG_TUD_ENDPOINT0_SIZE      64
#endif

// Interface descriptor indices
enum
{
	ITF_NUM_X360 = 0,

	// Number of all interfaces
	ITF_NUM_TOTAL,
};

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_CONFIG_H_ */
