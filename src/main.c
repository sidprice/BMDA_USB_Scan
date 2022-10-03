#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#if defined(_WIN32) || defined(__CYGWIN__)
#include <windows.h>
#include <stdint.h>

#include "ftd2xx.h"
#else
#include "libusb-1.0/libusb.h"
#include "libftdi1/ftdi.h"
#endif

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
/**
 * @brief Structure used to receive probe information from probe processing functions
 * 
 */
typedef struct probeInformation {
	char vid_pid[32];
	char probe_type[64];
	char serial_number[64];
} PROBE_INFORMATION;

#define MAX_PROBES 32
/**
 * @brief Array of probe inforatiokn structures for the currently attached probes.
 * 
 */
static PROBE_INFORMATION probes[MAX_PROBES];
//
// Create the list of debuggers BMDA works with
//
DEBUGGER_DEVICE debuggerDevices[] = {
	{VENDOR_ID_BMP, PRODUCT_ID_BMP, BMP_TYPE_BMP, false, "Black Magic Probe"},
	{VENDOR_ID_STLINK, PRODUCT_ID_STLINKV2, BMP_TYPE_STLINKV2, false, "ST-Link v2"},
	{VENDOR_ID_STLINK, PRODUCT_ID_STLINKV21, BMP_TYPE_STLINKV2, false, "ST-Link v2.1"},
	{VENDOR_ID_STLINK, PRODUCT_ID_STLINKV21_MSD, BMP_TYPE_STLINKV2, false, "ST-Link v2.1 MSD"},
	{VENDOR_ID_STLINK, PRODUCT_ID_STLINKV3_NO_MSD, BMP_TYPE_STLINKV2, false, "ST-Link v2.1 No MSD"},
	{VENDOR_ID_STLINK, PRODUCT_ID_STLINKV3, BMP_TYPE_STLINKV2, false, "ST-Link v3"},
	{VENDOR_ID_STLINK, PRODUCT_ID_STLINKV3E, BMP_TYPE_STLINKV2, false, "ST-Link v3E"},
	{VENDOR_ID_SEGGER, PRODUCT_ID_UNKNOWN, BMP_TYPE_JLINK, false, "Segger JLink"},
	{VENDOR_ID_FTDI, PRODUCT_ID_FTDI_FT2232, BMP_TYPE_LIBFTDI, false, "FTDI FT2232"},
	{VENDOR_ID_FTDI, PRODUCT_ID_FTDI_FT4232, BMP_TYPE_LIBFTDI, false, "FTDI FT4232"},
	{VENDOR_ID_FTDI, PRODUCT_ID_FTDI_FT232, BMP_TYPE_LIBFTDI, false, "FTDI FT232"},
	{0, 0, BMP_TYPE_NONE, false, ""},
};

void process_ftdi_probe(PROBE_INFORMATION *probe_information, size_t *debuggerCount)
{
}

struct libusb_device_descriptor *device_check_for_cmsis_interface(struct libusb_device_descriptor *device_descriptor,
	libusb_device *device, libusb_device_handle *handle, PROBE_INFORMATION *probe_information)
{
	struct libusb_device_descriptor *result = NULL;
	struct libusb_config_descriptor *config;
	if (libusb_get_active_config_descriptor(device, &config) == 0 && libusb_open(device, &handle) == 0) {
		bool cmsis_dap = false;
		for (size_t iface = 0; iface < config->bNumInterfaces && !cmsis_dap; ++iface) {
			const struct libusb_interface *interface = &config->interface[iface];
			for (int descriptorIndex = 0; descriptorIndex < interface->num_altsetting; ++descriptorIndex) {
				const struct libusb_interface_descriptor *descriptor = &interface->altsetting[descriptorIndex];
				uint8_t string_index = descriptor->iInterface;
				if (string_index == 0)
					continue;
				//
				// Read back the string descriptor interpreted as ASCII (wrong but
				// easier to deal with in C)
				//
				if (libusb_get_string_descriptor_ascii(handle, string_index,
						(unsigned char *)probe_information->probe_type, sizeof(probe_information->probe_type)) < 0)
					continue; /* We failed but that's a soft error at this point. */
				if (strstr(probe_information->probe_type, "CMSIS") != NULL) {
					//
					// Read the serial number from the probe
					//
					string_index = device_descriptor->iSerialNumber;
					if (libusb_get_string_descriptor_ascii(handle, string_index,
							(unsigned char *)probe_information->serial_number,
							sizeof(probe_information->serial_number)) < 0)
						continue; /* We failed but that's a soft error at this point. */

					result = device_descriptor;
					cmsis_dap = true;
				} else {
					memset(probe_information->probe_type, 0x00, sizeof(probe_information->probe_type));
				}
			}
		}
		libusb_close(handle);
	}
	return result;
}

struct libusb_device_descriptor *device_check_in_vid_pid_table(
	struct libusb_device_descriptor *device_descriptor, libusb_device *device, PROBE_INFORMATION *probe_information)
{
	struct libusb_device_descriptor *result = NULL;
	libusb_device_handle *handle;
	ssize_t vid_pid_index = 0;
	while (debuggerDevices[vid_pid_index].type != BMP_TYPE_NONE) {
		if (device_descriptor->idVendor == debuggerDevices[vid_pid_index].vendor &&
			(device_descriptor->idProduct == debuggerDevices[vid_pid_index].product ||
				debuggerDevices[vid_pid_index].product == PRODUCT_ID_UNKNOWN)) {
			result = device_descriptor;
			memcpy(probe_information->probe_type, debuggerDevices[vid_pid_index].typeString,
				strlen(debuggerDevices[vid_pid_index].typeString));
			if (libusb_open(device, &handle) == 0) {
				if (libusb_get_string_descriptor_ascii(handle, device_descriptor->iSerialNumber,
						(unsigned char *)&probe_information->serial_number,
						sizeof(probe_information->serial_number)) <= 0) {
					memset(probe_information->probe_type, 0x00, sizeof(probe_information->probe_type));
				}
				libusb_close(handle);
			} else {
				memcpy(probe_information->serial_number, "Unknown", sizeof("Unknown"));
			}

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
	// struct libusb_config_descriptor *config = NULL;

	int result;
	ssize_t cnt;
	size_t deviceIndex = 0;
	size_t debuggerCount = 0;
	//
	// If we are running on Windows the proprietory FTD2XX library is used
	// to collect debugger information.
	//
#if defined(_WIN32) || defined(__CYGWIN__)
	process_ftdi_probe(probes, &debuggerCount);
#endif
	result = libusb_init(NULL);
	if (result == 0) {
		cnt = libusb_get_device_list(NULL, &device_list);
		if (cnt > 0) {
			//
			// Parse the list of USB devices found
			//
			while ((device = device_list[deviceIndex++]) != NULL) {
				result = libusb_get_device_descriptor(device, &device_descriptor);
				memset(probes[debuggerCount].serial_number, 0x00, sizeof(probes[debuggerCount].serial_number));
				if (result < 0) {
					result = fprintf(stderr, "failed to get device descriptor");
					return -1;
				}
				//
				// Check if the device is an FTDI probe, these are processed above so skip
				//
				if (device_descriptor.idVendor != VENDOR_ID_FTDI) {
					if ((known_device_descriptor = device_check_in_vid_pid_table(
							 &device_descriptor, device, &probes[debuggerCount])) == NULL) {
						//
						// Check if there is a CMSIS interface on this device
						//
						known_device_descriptor = device_check_for_cmsis_interface(
							&device_descriptor, device, handle, &probes[debuggerCount]);
					}
					//
					// If we have a known device we can continue to report its data
					//
					if (known_device_descriptor != NULL) {
						if (device_descriptor.idVendor == VENDOR_ID_STLINK &&
							device_descriptor.idProduct == PRODUCT_ID_STLINKV2) {
							memcpy(probes[debuggerCount].serial_number, "Unknown", 8);
						}
						debuggerCount++;
					}
				}
			}
			libusb_free_device_list(device_list, 1);
		}
		//
		// Print the probes found
		//
		if (debuggerCount != 0) {
			for (size_t debugger_index = 0; debugger_index < debuggerCount; debugger_index++) {
				printf("%lld\t%-20s\tS/N: %s\n", debugger_index + 1, probes[debugger_index].probe_type,
					probes[debugger_index].serial_number);
			}
		}
		libusb_exit(NULL);
	}

	return result;
}

int main(void)
{
	return find_debuggers(NULL, NULL);
}

// int main(int argc, char **argv)
// {
//     int ret, i;
//     struct ftdi_context ftdic;
//     struct ftdi_device_list *devlist, *curdev;
//     char manufacturer[128], description[128], serial[128];
// 	(void)argc ;
// 	(void)argv ;
//     ftdi_init(&ftdic);
//     if((ret = ftdi_usb_find_all(&ftdic, &devlist, 0x0403, 0x6010)) < 0) {
//         fprintf(stderr, "ftdi_usb_find_all failed: %d (%s)\n", ret, ftdi_get_error_string(&ftdic));
//         return -1;
//     }
//     printf("Number of FTDI devices found: %d\n", ret);
// 	// if ((ret = ftdi_usb_open_desc(&ftdic, 0x0403, 0x6010 , description, NULL)) < 0) {
// 	// 	printf("Failed to read serial number -> %s\n", ftdi_get_error_string(&ftdic));
// 	// }

//     i = 0;
//     for (curdev = devlist; curdev != NULL; i++) {
//         printf("Checking device: %d\n", i);
//         if((ret = ftdi_usb_get_strings(&ftdic, curdev->dev, manufacturer, 128, description, 128, serial, 128)) < 0) {
// 			if ( ret != -9) {
//             fprintf(stderr, "ftdi_usb_get_strings failed: %d (%s)\n", ret, ftdi_get_error_string(&ftdic));
//             return -1;
// 			}
//         }
//         printf("Manufacturer: %s, Description: %s, Serial Number: %s\n\n", manufacturer, description, serial !=NULL ? serial : "") ;
//         curdev = curdev->next;
//     }
//     ftdi_list_free(&devlist);
//     ftdi_deinit(&ftdic);
//     return 0;
// }