#include <stdbool.h>
#include <stdio.h>

#include "libusb-1.0/libusb.h"

#include "../inc/cli.h"
#include "../inc/platform.h"
#include "../inc/bmp_hosted.h"

typedef struct debuggerDevice {
	uint16_t vendor;
	uint16_t product;
	bmp_type_t type;
	bool isCMSIS;
	char *typeString;
} DEBUGGER_DEVICE;

//
// Create the list of debuggers BMDA works with
//
DEBUGGER_DEVICE debuggerDevices[] = {
	{VENDOR_ID_BMP, PRODUCT_ID_BMP, BMP_TYPE_BMP, false, "Black Magic Probe"},
	{VENDOR_ID_STLINK, PRODUCT_ID_STLINKV2, BMP_TYPE_STLINKV2, false, "STLink V2"},
	{VENDOR_ID_STLINK, PRODUCT_ID_STLINKV21, BMP_TYPE_STLINKV2, false, "STLink V21"},
	{VENDOR_ID_STLINK, PRODUCT_ID_STLINKV21_MSD, BMP_TYPE_STLINKV2, false, "STLink V21 MSD"},
	{VENDOR_ID_STLINK, PRODUCT_ID_STLINKV3_NO_MSD, BMP_TYPE_STLINKV2, false, "STLink V21 No MSD"},
	{VENDOR_ID_STLINK, PRODUCT_ID_STLINKV3, BMP_TYPE_STLINKV2, false, "STLink V3"},
	{VENDOR_ID_STLINK, PRODUCT_ID_STLINKV3E, BMP_TYPE_STLINKV2, false, "STLink V3E"},
	{VENDOR_ID_SEGGER, PRODUCT_ID_UNKNOWN, BMP_TYPE_JLINK, false, "Segger JLink"},
	{VENDOR_ID_FTDI, PRODUCT_ID_FTDI_FT2232, BMP_TYPE_LIBFTDI, false, "FTDI FT2232"},
	{VENDOR_ID_FTDI, PRODUCT_ID_FTDI_FT4232, BMP_TYPE_LIBFTDI, false, "FTDI FT4232"},
	{VENDOR_ID_FTDI, PRODUCT_ID_FTDI_FT232, BMP_TYPE_LIBFTDI, false, "FTDI FT232"},
	{0, 0, BMP_TYPE_NONE, false, ""},
};

struct libusb_device_descriptor *device_check_for_cmsis_interface(struct libusb_device_descriptor *device_descriptor,
	struct libusb_config_descriptor *config, libusb_device *device, libusb_device_handle *handle, char *type_string,
	int type_string_max_len)
{
	struct libusb_device_descriptor *result = NULL;
	if (libusb_get_active_config_descriptor(device, &config) == 0 && libusb_open(device, &handle) == 0) {
		bool cmsis_dap = false;
		for (size_t iface = 0; iface < config->bNumInterfaces && !cmsis_dap; ++iface) {
			const struct libusb_interface *interface = &config->interface[iface];
			for (int descriptorIndex = 0; descriptorIndex < interface->num_altsetting; ++descriptorIndex) {
				const struct libusb_interface_descriptor *descriptor = &interface->altsetting[descriptorIndex];
				const uint8_t string_index = descriptor->iInterface;
				if (string_index == 0)
					continue;
				/* Read back the string descriptor interpreted as ASCII (wrong but
         * easier to deal with in C) */
				if (libusb_get_string_descriptor_ascii(
						handle, string_index, (unsigned char *)type_string, type_string_max_len) < 0)
					continue; /* We failed but that's a soft error at this point. */
				if (strstr((char *)type_string, "CMSIS") != NULL) {
					result = device_descriptor;
					cmsis_dap = true;
				} else {
					memset(type_string, 0x00, type_string_max_len);
				}
			}
		}
	}
	return result;
}

struct libusb_device_descriptor *device_in_vid_pid_table(
	struct libusb_device_descriptor *device_descriptor, char *debugger_type_string, ssize_t string_length)
{
	struct libusb_device_descriptor *result = NULL;
	ssize_t vid_pid_index = 0;
	while (debuggerDevices[vid_pid_index].type != BMP_TYPE_NONE) {
		if (device_descriptor->idVendor == debuggerDevices[vid_pid_index].vendor &&
			(device_descriptor->idProduct == debuggerDevices[vid_pid_index].product ||
				debuggerDevices[vid_pid_index].product == PRODUCT_ID_UNKNOWN)) {
			result = device_descriptor;
			strncpy(debugger_type_string, debuggerDevices[vid_pid_index].typeString, string_length);
			break;
		}
		vid_pid_index++;
	}
	return result;
}

int find_debuggers(BMP_CL_OPTIONS_t *cl_opts, bmp_info_t *info)
{
	(void)cl_opts;
	(void)info;

	libusb_device **device_list;
	struct libusb_device_descriptor device_descriptor;
	struct libusb_device_descriptor *known_device_descriptor;
	libusb_device *device;
	libusb_device_handle *handle = NULL;
	struct libusb_config_descriptor *config = NULL;

	int result;
	ssize_t cnt;
	int deviceIndex = 0;
	int debuggerCount = 1;

	result = libusb_init(NULL);
	if (result == 0) {
		cnt = libusb_get_device_list(NULL, &device_list);
		if (cnt > 0) {
			//
			// Parse the list of USB devices found
			//
			while ((device = device_list[deviceIndex++]) != NULL) {
				char serial_number_string[64];
				char debugger_type_string[64];
				result = libusb_get_device_descriptor(device, &device_descriptor);
				memset(debugger_type_string, 0x00, sizeof(debugger_type_string));
				if (result < 0) {
					result = fprintf(stderr, "failed to get device descriptor");
					return -1;
				}
				//
				// First check if the device is in the VID:PID table
				//
				if ((known_device_descriptor = device_in_vid_pid_table(
						 &device_descriptor, debugger_type_string, sizeof(debugger_type_string) - 1)) == NULL) {
					//
					// Open the device and check if there is a CMSIS interface
					//
					known_device_descriptor = device_check_for_cmsis_interface(
						&device_descriptor, config, device, handle, debugger_type_string, sizeof(debugger_type_string));
				}
				//
				// If we have a known device we can continue to report its data
				//
				if (known_device_descriptor != NULL) {
					if (device_descriptor.idVendor == VENDOR_ID_STLINK &&
						device_descriptor.idProduct == PRODUCT_ID_STLINKV2) {
						memcpy(serial_number_string, "Unknown", 8);
					} else {
						//
						// Read the serial number from the config descriptor
						//
						if (handle == 0) {
							libusb_open(device, &handle);
						}
						if (handle != 0) {
							if ((result = libusb_get_string_descriptor_ascii(handle,
									 known_device_descriptor->iSerialNumber, (unsigned char *)serial_number_string,
									 sizeof(serial_number_string))) <= 0) {
								serial_number_string[0] = 0x00;
							}
						} else {
							memcpy(serial_number_string, "Unknown", sizeof("Unknown"));
						}
					}

					printf("%d\t%04hX:%04hX\t%-20s\tS/N: %s\n", debuggerCount++, device_descriptor.idVendor,
						device_descriptor.idProduct, debugger_type_string, serial_number_string);
					if (handle != 0) {
						libusb_close(handle); // Clean up
						handle = 0;
					}
				}
			}
			libusb_free_device_list(device_list, 1);

		}
		libusb_exit(NULL);
	}

	return result;
}

int main(void)
{
	return find_debuggers(NULL, NULL);
}