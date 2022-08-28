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
	{ 0, 0, BMP_TYPE_NONE, false, ""},
} ;

int find_debuggers(BMP_CL_OPTIONS_t *cl_opts, bmp_info_t *info)
{
	(void)cl_opts ;
	(void)info ;
	libusb_device **devs;
	int result;
	ssize_t cnt;
	libusb_device *dev;
	int i = 0, j = 0;
	int debuggerCount = 1 ;
	//uint8_t path[8];
	result = libusb_init(NULL);
	if (result == 0) {
		cnt = libusb_get_device_list(NULL, &devs);
		if (cnt > 0) {
			//
			// Parse the list of USB devices found and initially
			// filter them using the VID:PID table
			//
			while ((dev = devs[i++]) != NULL) {
				struct libusb_device_descriptor desc;
				int r = libusb_get_device_descriptor(dev, &desc);
				if (r < 0) {
					fprintf(stderr, "failed to get device descriptor");
					return -1;
				}
				while ( debuggerDevices[j].type != BMP_TYPE_NONE) {
					if ( desc.idVendor == debuggerDevices[j].vendor && 
							(desc.idProduct == debuggerDevices[j].product || debuggerDevices[j].product == PRODUCT_ID_UNKNOWN)) {
						printf("%d\t%04hX:%04hX\t%s\n", debuggerCount++, debuggerDevices[j].vendor, debuggerDevices[j].product,debuggerDevices[j].typeString) ;
					}
					j++ ;
				}
				j = 0 ;
				// printf("%04x:%04x (bus %d, device %d)",
				// 	desc.idVendor, desc.idProduct,
				// 	libusb_get_bus_number(dev), libusb_get_device_address(dev));

				// r = libusb_get_port_numbers(dev, path, sizeof(path));
				// if (r > 0) {
				// 	printf(" path: %d", path[0]);
				// 	for (j = 1; j < r; j++)
				// 		printf(".%d", path[j]);
				// }
				// printf("\n");
			}


			libusb_free_device_list(devs, 1);
		}

		libusb_exit(NULL);
	}

	return result ;
}

int main(void)
{
	return find_debuggers(NULL, NULL) ;
}