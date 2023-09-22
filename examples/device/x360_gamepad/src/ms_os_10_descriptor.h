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

#ifndef _MS_OS_10_H_
#define _MS_OS_10_H_

#ifdef __cplusplus
 extern "C" {
#endif

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+

#define MS_OS_DESCRIPTOR_STRING_INDEX             0xEE
#define MS_OS_DESCRIPTOR_BCD_VERSION              0x0100
#define MS_OS_DESCRIPTOR_SIGNATURE                { 0x4D, 0x00, 0x53, 0x00, 0x46, 0x00, 0x54, 0x00, 0x31, 0x00, 0x30, 0x00, 0x30, 0x00 } /* MSFT100 */
#define MS_OS_DESCRIPTOR_CONTAINERID_SUPPORT      1

// Indices for the wIndex of the ms_os_feature_descriptor_request_t to identify the specific OS descriptor request type
typedef enum
{
	MS_EXTENDED_GENRE_DESCRIPTOR                  = 0x01,                                 // Value to identify a genre OS descriptor request, might be supported in future Windows versions
	MS_EXTENDED_COMPATID_DESCRIPTOR               = 0x04,                                 // Value to identify an extended compat ID OS descriptor request
	MS_EXTENDED_PROPERTIES_DESCRIPTOR             = 0x05,                                 // Value to identify an extended properties OS descriptor request
	MS_CONTAINERID_DESCRIPTOR                     = 0x06,                                 // Value to identify a ContainerID OS descriptor request
} ms_os_feature_descriptor_request_type_t;

// Indices for the dwPropertyDataType of the ms_extended_properties_os_feature_descriptor_custom_property_section_t to identify the data type to be used for the Registry
// Only indices which are in the list are allowed, every index which isn't in the list is reserved.
typedef enum
{
	MS_PROPERTY_DATA_TYPE_REG_SZ                  = 0x00000001,                            // Value for the index for a NULL-terminated Unicode string
	MS_PROPERTY_DATA_TYPE_REG_EXPAND_SZ           = 0x00000002,                            // Value for the index for a NULL-terminated Unicode string that includes environment variables
	MS_PROPERTY_DATA_TYPE_REG_BINARY              = 0x00000003,                            // Value for the index for a free form binary
	MS_PROPERTY_DATA_TYPE_REG_DWORD_LITTLE_EDIAN  = 0x00000004,                            // Value for the index for a little-endian 32-bit integer
	MS_PROPERTY_DATA_TYPE_REG_DWORD_BIG_ENDIAN    = 0x00000005,                            // Value for the index for a big-endian 32-bit integer
	MS_PROPERTY_DATA_TYPE_REG_LINK                = 0x00000006,                            // Value for the index for a NULL-terminated Unicode string that contains a symbolic link
	MS_PROPERTY_DATA_TYPE_REG_MULTI_SZ            = 0x00000007,                            // Value for the index for multiple NULL-terminated Unicode strings
} ms_extended_properties_os_feature_descriptor_property_data_type_t;

// Structure for the MS extended OS descriptor
// Used to inform the system that the device is capable of using MS specific extended OS feature descriptors.
// This descriptor must be replied for a string descriptor request on index 0xEE in order to support the MS extended OS descriptors.
typedef struct TU_ATTR_PACKED
{
	uint8_t bLength;                                                                       // Value of the length of the entire descriptor
	uint8_t bDescriptorType;                                                               // Type of the descriptor, must be 0x03 for a string descriptor
	uint8_t qwSignature[14];                                                               // Contains the Unicode string that identifies the descriptor as an OS string descriptor and include the version number for it
	uint8_t bMS_VendorCode;                                                                // Value for vendor specific MS_VendorCode to retrieve any associated feature descriptor
	union
	{
		uint8_t bPad;                                                                      // Value for the pad field, must be 0x00 for normal usage
		struct
		{
			uint8_t            :1;                                                         // Reserved
			uint8_t ContainerID:1;                                                         // Value which indicates if the device supports the USB Container ID, 0 if the device don't support the Container ID, 1 if it does
			uint8_t            :6;                                                         // Reserved
		} bFlags;							                	                           // Bitfield to declare supported extended OS descriptor features
	};
} ms_os_descriptor_t;

// Specified structure of the common USB Device SETUP request used for MS extended OS descriptor
// Specified structure for easy accessing a Microsoft OS descriptor.
// Composition still matches the official USB Device SETUP request.
// A Microsoft OS descriptor must be offered together with a device specific vendor code (which will be used to retrieve a MS feature descriptor) and need to be stored in the USB flags for the device within the Windows Registry in order to obtain a Microsoft OS feature descriptor request.
typedef struct TU_ATTR_PACKED
{
	uint8_t bmRequestType;                                                                 // Value of the requested type, should be 0xC0 (vendor specific device to host request for the entire device) to retrieve any OS feature descriptor but it seems except for an extended properties OS descriptor request which should be 0xC1
	uint8_t bRequest;                                                                      // Value to identify the request as a OS feature descriptor request, must be the same as bMS_VendorCode which is sent in the OS descriptor
	union
	{
		uint16_t wValue;                                                                   // (Normally for various usage depending on the precise request)
		struct
		{
			uint8_t wValueLowByte;                                                         // Value for the page number used to retrieve the descriptor if it's larger than 64 KB, must be be index of the forwarded 64 KB block, is 0x00 if the descriptor is less or equal 64 KB
			uint8_t wValueHighbyte;                                                        // Value for the interface number that is associated with the descriptor, typically 0x00 for a compatID descriptor but should be ignored then or the interface number of the corresponding interface for a extended feature descriptor
		};
	};
	uint16_t wIndex;                                                                       // Value to identify the requested OS feature descriptor, must be a valid index of the enum ms_os_feature_descriptor_request_type_t, 0x04 for an extended compat ID OS descriptor or 0x05 for an extended properties OS descriptor (Normally for various usage depending on the precise request)
	uint16_t wLength;                                                                      // Value for the entire descriptor, even if the buffer is larger than wLength only this value must be returned, is 0x10 for the header portion only for an extended compat ID OS descriptor or 0x0A for the header portion only for an extended properties OS descriptor
} ms_os_feature_descriptor_request_t;

// Structure of the extended compat ID OS feature descriptor - header section
// First part of the extended compat ID OS feature descriptor, followed by the function sections.
typedef struct TU_ATTR_PACKED
{
	uint32_t dwLength;                                                                     // Length in byte of the entire  extended compat ID OS feature descriptor, including the header
	uint16_t bcdVersion;                                                                   // Version number of the descriptor in binary coded decimal, should be 0x0100 for version 1.00
	uint16_t wIndex;                                                                       // Index of the particular OS feature descriptor, must be the same as value as for the request of this descriptor, must be MS_EXTENDED_COMPATID_DESCRIPTOR
	uint8_t bCount;                                                                        // Number of function sections
	uint8_t reserved[7];                                                                   // Reserved / unused bytes, should be 0x00000000000000
} ms_extended_compatID_os_feature_descriptor_header_section_t;

// Structure of the extended compat ID OS feature descriptor - function section
// Second part of the extended compat ID OS feature descriptor, subsequent of the header section.
typedef struct TU_ATTR_PACKED
{
	uint8_t bFirstInterfaceNumber;                                                         // Value of the interface or function number in increasing order
	uint8_t reserved1;                                                                     // Reserved / unused byte, must be 0x01
	uint8_t compatibleID[8];                                                               // Value of the functions compatible ID, last byte must be NULL (only uppercase letters, numbers & underscores are allowed)
	uint8_t subCompatibleID[8];                                                            // Value of the functions subcompatible ID, last byte must be NULL (only uppercase letters, numbers & underscores are allowed)
	uint8_t reserved2[6];                                                                  // Reserved / unused bytes, must be NULLs
} ms_extended_compatID_os_feature_descriptor_function_section_t;

// Structure of the entire extended compat ID OS feature descriptor
// Used to specify specific driver information for Windows for the offered interfaces (groups) for automated driver assignment.
// Only a single extended compat ID OS feature descriptor is requested for the entire device. Each interface (group) must have a corresponding function section present.
// Multiple interfaces which belongs to the same group must be linked to an interface group with an interface association descriptor (IAD).
// A Microsoft OS descriptor must be offered together with a device specific vendor code (which will be used to retrieve a MS feature descriptor) and need to be stored in the USB flags for the device within the Windows Registry in order to obtain a Microsoft OS feature descriptor request.
typedef struct TU_ATTR_PACKED
{
	ms_extended_compatID_os_feature_descriptor_header_section_t header;                    // Structure of the header section of the extended compat ID OS feature descriptor
	ms_extended_compatID_os_feature_descriptor_function_section_t functions[1];            // Structure of the function section of the extended compat ID OS feature descriptor
} ms_extended_compatID_os_feature_descriptor_t;

// Structure of the extended properties OS feature descriptor - header section
// First part of the extended properties OS feature descriptor, followed by the custom property sections.
typedef struct TU_ATTR_PACKED
{
	uint32_t dwLength;                                                                     // Length in byte of the entire  extended compat ID OS feature descriptor, including the header
	uint16_t bcdVersion;                                                                   // Version number of the descriptor in binary coded decimal, should be 0x0100 for version 1.00
	uint16_t wIndex;                                                                       // Index of the particular OS feature descriptor, must be the same as value as for the request of this descriptor, must be MS_EXTENDED_PROPERTIES_DESCRIPTOR
	uint16_t wCount;                                                                       // Number of custom property sections
} ms_extended_properties_os_feature_descriptor_header_section_t;

// Structure of the extended properties OS feature descriptor - custom property section
// Second part of the extended properties OS feature descriptor, subsequent of the header section.
typedef struct TU_ATTR_PACKED
{
	uint32_t dwSize;                                                                       // Value of the size of the property
	uint32_t dwPropertyDataType;                                                           // Index of the format of the property, see ms_extended_properties_os_feature_descriptor_property_data_type_t
	uint16_t wPropertyNameLength;                                                          // Value of the length of the property name including a tailing NULL character
	uint8_t bPropertyName[26];                                                             // Name of the property as a NULL-terminated Unicode string (two byte per character needed)
	uint32_t dwPropertyDataLength;                                                         // Value of the size of the property data
	uint8_t bPropertyData[22];                                                             // Property data (typically in Unicode with two byte per character)
} ms_extended_properties_os_feature_descriptor_custom_property_section_t;

// Structure of the entire extended properties OS feature descriptor
// Used to specify specific advanced driver information for Windows for the offered interfaces for automated assign of extended function properties.
// Windows will store these values under the "Device Parameters" key under the corresponding interface.
// An extended properties OS feature descriptor is requested per interface and each interface associated with an extended properties OS feature descriptor can have one or more property sections. Each custom property section contains the information of a single property.
// A Microsoft OS descriptor must be offered together with a device specific vendor code (which will be used to retrieve a MS feature descriptor) and need to be stored in the USB flags for the device within the Windows Registry in order to obtain a Microsoft OS feature descriptor request.
typedef struct TU_ATTR_PACKED
{
	ms_extended_properties_os_feature_descriptor_header_section_t header;                  // Structure of the header section of extended properties OS feature descriptor
	ms_extended_properties_os_feature_descriptor_custom_property_section_t properties[1];  // Structure of the custom property section of extended properties OS feature descriptor
} ms_extended_properties_os_feature_descriptor_t;

// Structure of the ContainerID OS feature descriptor - header section
// First part of the ContainerID OS feature descriptor, followed by the ContainerID sections.
typedef struct TU_ATTR_PACKED
{
	uint32_t dwLength;                                                                     // Length in byte of the entire ContainerID OS feature descriptor, including the header, must be 0x18
	uint16_t bcdVersion;                                                                   // Version number of the descriptor in binary coded decimal, should be 0x0100 for version 1.00
	uint16_t wIndex;                                                                       // Index of the particular OS feature descriptor, must be the same as value as for the request of this descriptor, must be MS_CONTAINERID_DESCRIPTOR
} ms_containerid_os_feature_descriptor_header_section_t;

// Structure of the ContainerID OS feature descriptor - ComtainerID section
// Second part of the ContainerID OS feature descriptor, subsequent of the ContainerID sections.
typedef struct TU_ATTR_PACKED
{
	uint8_t bContainerID[16];                                                              // Data of the ContainerID, must be a unique UUID string in a format of {xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx}, attention the byute order of the groups of the first half are inverted
} ms_containerid_os_feature_descriptor_containerid_section_t;

// Structure of the ContainerID OS feature descriptor
// Used to specify a ContainerID for the entire device.
// A Microsoft OS descriptor must be offered together with a device specific vendor code (which will be used to retrieve a MS feature descriptor) and need to be stored in the USB flags for the device within the Windows Registry in order to obtain a Microsoft OS feature descriptor request.
typedef struct TU_ATTR_PACKED
{
	ms_containerid_os_feature_descriptor_header_section_t header;                          // Structure of the header section of ContainerID OS feature descriptor
	ms_containerid_os_feature_descriptor_containerid_section_t containerID;                // Structure of the ContainerID section of ContainerID OS feature descriptor
} ms_containerid_os_feature_descriptor_t;

// Structure for the interface association descriptor (IAD)
// Used to group interfaces which belongs to a function.
// This descriptor must be present, if used, directly before the first interface of that group, and all included interfaces of that group must be in sequently order.
// If used, the bDeviceClass must be set to 0xEF (miscellaneous device class), the bDeviceSubClass must be set to 0x02 (common class) and the bDeviceProtocol must be set to 0x01 (interface association descriptor) to identify the device as an multi-interface function device class device.
// Any descriptor between the configuration descriptor and the first interface or interface association descriptor (IAD) should considered as global and be delivered to every function device driver.
typedef struct TU_ATTR_PACKED
{
	uint8_t bLength;                                                                       // Value of the length of the entire descriptor (must be 0x08)
	uint8_t bdescriptorType;                                                               // Value of the descriptor type (must be 0x0B)
	uint8_t bFirstInterface;                                                               // Value of the number of the first interface in that group
	uint8_t bInterfaceCount;                                                               // Value of the quantity of the interfaces in that group (interface must be in sequently order)
	uint8_t bFunctionClass;                                                                // Value of the class of the interface group, should match (but not required) with the first interface of the group (must contain values which are specified by the USB device class that describes the interface or function
	uint8_t bFunctionSubClass;                                                             // Value of the sub class of the interface group, should match (but not required) with the first interface of the group (must contain values which are specified by the USB device class that describes the interface or function
	uint8_t bFunctionProtocol;                                                             // Value of the protocol of the interface group (must contain values which are specified by the USB device class that describes the interface or function
	uint8_t iFunction;                                                                     // Index of the string of the ISD interface group
} usb_interface_association_descriptor_t;

#ifdef __cplusplus
 }
#endif

#endif /* _MS_OS_10_H_ */
