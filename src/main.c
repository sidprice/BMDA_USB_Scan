#include <stdio.h>
#include <stdbool.h>

#include "libusb-1.0/libusb.h"

#include "../inc/platform.h"
#include "../inc/cli.h"
#include "../inc/bmp_hosted.h"

typedef struct debuggerDevice {
	uint16_t	vendor ;
	uint16_t	product ;
	bmp_type_t	type ;
	bool		isCMSIS ;
	char		*typeString ;
} DEBUGGER_DEVICE ;

//
// Create the list of debuggers BMDA works with
//
DEBUGGER_DEVICE	debuggerDevices[] = {
	{ VENDOR_ID_BMP, PRODUCT_ID_BMP, BMP_TYPE_BMP, false, "Black Magic Probe"},
	{ VENDOR_ID_STLINK, PRODUCT_ID_STLINKV2, BMP_TYPE_STLINKV2, false, "STLink V2"},
	{ VENDOR_ID_STLINK, PRODUCT_ID_STLINKV21, BMP_TYPE_STLINKV2, false, "STLink V21"},
	{ VENDOR_ID_STLINK, PRODUCT_ID_STLINKV21_MSD, BMP_TYPE_STLINKV2, false, "STLink V21 MSD"},
	{ VENDOR_ID_STLINK, PRODUCT_ID_STLINKV3_NO_MSD, BMP_TYPE_STLINKV2, false, "STLink V21 No MSD"},
	{ VENDOR_ID_STLINK, PRODUCT_ID_STLINKV3, BMP_TYPE_STLINKV2, false, "STLink V3"},
	{ VENDOR_ID_STLINK, PRODUCT_ID_STLINKV3E, BMP_TYPE_STLINKV2, false, "STLink V3E"},
	{ VENDOR_ID_SEGGER, PRODUCT_ID_UNKNOWN, BMP_TYPE_JLINK, false, "Segger JLink"},
	{ VENDOR_ID_FTDI, PRODUCT_ID_FTDI_FT2232, BMP_TYPE_LIBFTDI, false, "FTDI FT2232"},
	{ VENDOR_ID_FTDI, PRODUCT_ID_FTDI_FT4232, BMP_TYPE_LIBFTDI, false, "FTDI FT4232"},
	{ VENDOR_ID_FTDI, PRODUCT_ID_FTDI_FT232, BMP_TYPE_LIBFTDI, false, "FTDI FT232"},
	{ 0, 0, BMP_TYPE_NONE, false, ""},
} ;

struct libusb_device_descriptor *device_check_for_cmsis_interface(struct libusb_device_descriptor *device_descriptor, struct libusb_config_descriptor *config)
{
	(void)device_descriptor ;
	(void)config ;
	return NULL ;
}

struct libusb_device_descriptor * device_in_vid_pid_table(struct libusb_device_descriptor *device_descriptor) {
	(void)device_descriptor ;
	return NULL ;
}

int find_debuggers(BMP_CL_OPTIONS_t *cl_opts, bmp_info_t *info)
{
	(void)cl_opts ;
	(void)info ;

	libusb_device **device_list;
	struct libusb_device_descriptor device_descriptor;
	struct libusb_device_descriptor *known_device_descriptor;
	libusb_device *device;
	libusb_device_handle *handle = NULL;
	struct libusb_config_descriptor *config = NULL;

	int result;
	ssize_t cnt;
	int deviceIndex = 0 ;
	//int debuggerCount = 1 ;

	result = libusb_init(NULL);
	if (result == 0) {
		cnt = libusb_get_device_list(NULL, &device_list);
		if (cnt > 0) {
			//
			// Parse the list of USB devices found
			//
			while ((device = device_list[deviceIndex++]) != NULL) {
				int r = libusb_get_device_descriptor(device, &device_descriptor);
				if (r < 0) {
					fprintf(stderr, "failed to get device descriptor");
					return -1;
				}				//
				// First check if the device is in the VID:PID table
				//
				if ( (known_device_descriptor = device_in_vid_pid_table(&device_descriptor)) == NULL) {
					//
					// Open the device and check if there is a CMSIS interface
					//
					known_device_descriptor = device_check_for_cmsis_interface(&device_descriptor, config) ;
				} else {
					//
					// In order to later get the serial number the device must be opened
					//
					// Note that the process of checking for a CMSIS interface opens the
					// device so we must not repeat that below
					//
					if ( libusb_open(device, &handle) != 0) {
						known_device_descriptor = NULL;	// failed to open device
					} else {
						//
						// Similarly, the code below assumes the config descriptor has been read,
						// so we must do it here for devices in the VID:PID table
						//
						if (libusb_get_active_config_descriptor(device, &config) != 0 ) {
							//
							// Failed to read the config descriptor, no longer have valid device
							//
							known_device_descriptor = NULL ;
							libusb_close(handle) ;
						}
					}
				}
				//
				// If we have a known device we can continue to report its data
				//
				if ( known_device_descriptor != NULL) {
					libusb_close(handle) ;	// Clean up
				}
			}
			libusb_free_device_list(device_list, 1);



#if 0
			while ((device = device_list[deviceIndex++]) != NULL) {
				bool	debuggerFound = false ;
				int r = libusb_get_device_descriptor(device, &device_descriptor);
				if (r < 0) {
					fprintf(stderr, "failed to get device descriptor");
					return -1;
				}

				while ( debuggerDevices[vid_pid_index].type != BMP_TYPE_NONE) {
					debuggerFound = false ;
					if ( device_descriptor.idVendor == debuggerDevices[vid_pid_index].vendor && 
							(device_descriptor.idProduct == debuggerDevices[vid_pid_index].product || debuggerDevices[vid_pid_index].product == PRODUCT_ID_UNKNOWN)) {
						printf("%d\t%04hX:%04hX\t%-20s\tS/N: %s\n", debuggerCount++, device_descriptor.idVendor, device_descriptor.idProduct,debuggerDevices[vid_pid_index].typeString, "serial") ;
						debuggerFound = true ;
						break;
					}
					vid_pid_index++ ;
				}
				vid_pid_index = 0 ;

				if (debuggerFound == false)	{
					//
					// The USB device is not in the VID:PID table, scan
					// the devices interface strings to check for
					// CMSIS DAP devices.
					//
					struct libusb_config_descriptor *config = NULL;
					libusb_device_handle *handle = NULL;
					if (libusb_get_active_config_descriptor(device, &config) != 0 || libusb_open(device, &handle) != 0) {
						// Handle the error.
						continue;
					}	
					bool cmsis_dap = false;
					for (size_t iface = 0; iface < config->bNumInterfaces && !cmsis_dap; ++iface) {
						const struct libusb_interface *interface = &config->interface[iface];
						for (int descriptorIndex = 0; descriptorIndex < interface->num_altsetting; ++descriptorIndex) {
							const struct libusb_interface_descriptor *descriptor = &interface->altsetting[descriptorIndex];
							const uint8_t string_index = descriptor->iInterface;
							if (string_index == 0)
								continue;
							char iface_string[256] ;
							char serial_number_string[256] ;
							/* Read back the string descriptor interpreted as ASCII (wrong but easier to deal with in C) */
							if (libusb_get_string_descriptor_ascii(handle, string_index, (unsigned char *)iface_string, sizeof(iface_string)) < 0 )
								continue; /* We failed but that's a soft error at this point. */
							if (libusb_get_string_descriptor_ascii(handle,device_descriptor.iSerialNumber, (unsigned char *)serial_number_string, sizeof(serial_number_string)) < 0 )
								continue;
							if (strstr(iface_string, "CMSIS") != NULL) {
								printf("%d\t%04hX:%04hX\t%-20s\tS/N: %s\n", 
									debuggerCount++,
									device_descriptor.idVendor,
									device_descriptor.idProduct,
									iface_string, (char *)&serial_number_string[0]) ;
								cmsis_dap = true;
								break;
							}
						}
					}
					if (handle != NULL ) {
						libusb_close(handle) ;
					}
				}
			}
#endif
		}
		libusb_exit(NULL);
	}

	return result ;
}

int main(void)
{
	return find_debuggers(NULL, NULL) ;
}