/*******************************************************
 Windows HID simplification

 Alan Ott
 Signal 11 Software

 8/22/2009

 Copyright 2009

 This contents of this file may be used by anyone
 for any reason without any conditions and may be
 used as a starting point for your own applications
 which use HIDAPI.
********************************************************/

#include <stdio.h>
#include <wchar.h>
#include <string.h>
#include <stdlib.h>
#include "hidapi.h"

#include <iostream>
#include <cstring> // for memset
#include <cstdlib> // for exit
#include <winsock2.h>
#include <iostream>
#include <string>

#pragma comment(lib, "ws2_32.lib") // ¡¥Ω”Winsockø‚
#pragma warning(disable:4996) 

// Headers needed for sleeping.
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

#define BIT_BTN_LEFT (7)
#define BIT_BTN_RIGHT (6)
#define BIT_SWD (5)
#define BIT_SWC (3)
#define BIT_SWB (1)
#define BIT_SWA (0)

#define SW_UP  (0)
#define SW_MID (1)
#define SW_DOWN (2)

#define BTN_OFF (0)
#define BTN_ON (1)


enum channel_id {
	CH1 = 0,
	CH2,
	CH3,
	CH4,
	SWA,
	SWB,
	SWC,
	SWD,
	BTN_L,
	BTN_R
};

UINT16 channel_value[10];

typedef struct {
	double timestamp;                               // in seconds
	UINT16 channels[16];   // RC channels
} rc_packet;

int main(int argc, char* argv[])
{
	WSADATA wsaData;
	SOCKET sock;
	struct sockaddr_in server;
	char buffer[1024];

	int res;
	unsigned char buf[256];
#define MAX_STR 255
	wchar_t wstr[MAX_STR];
	hid_device* handle;
	int i;

#ifdef WIN32
	UNREFERENCED_PARAMETER(argc);
	UNREFERENCED_PARAMETER(argv);
#endif

	struct hid_device_info* devs, * cur_dev;

	if (hid_init()) {
		printf("hid init failed \n");
		return -1;
	}

	// ≥ı ºªØWinsock
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
		std::cerr << "Winsock Initialization failed. Error Code : " << WSAGetLastError() << std::endl;
		return 1;
	}

	server.sin_addr.s_addr = inet_addr("172.25.58.170");
	server.sin_family = AF_INET;
	server.sin_port = htons(9004);

	sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock == INVALID_SOCKET) {
		std::cerr << "Could not create socket : " << WSAGetLastError() << std::endl;
		WSACleanup();
		return 1;
	}

	//devs = hid_enumerate(0x0, 0x0);
	//cur_dev = devs;
	//while (cur_dev) {
	//	printf("Device Found\n  type: %04hx %04hx\n  path: %s\n  serial_number: %ls", cur_dev->vendor_id, cur_dev->product_id, cur_dev->path, cur_dev->serial_number);
	//	printf("\n");
	//	printf("  Manufacturer: %ls\n", cur_dev->manufacturer_string);
	//	printf("  Product:      %ls\n", cur_dev->product_string);
	//	printf("  Release:      %hx\n", cur_dev->release_number);
	//	printf("  Interface:    %d\n", cur_dev->interface_number);
	//	printf("\n");
	//	cur_dev = cur_dev->next;
	//}
	//hid_free_enumeration(devs);

	////handle = hid_open(0x4d8, 0x3f, L"12345");
	handle = hid_open(0x284E, 0x7FFF, NULL); // flysky i6s product page https://www.flysky-cn.com/fsi6s
	if (!handle) {
		printf("unable to open fusi i6s rc hid device\n");
		return 1;
	}

	// Read the Manufacturer String
	wstr[0] = 0x0000;
	res = hid_get_manufacturer_string(handle, wstr, MAX_STR);
	if (res < 0)
		printf("Unable to read manufacturer string\n");
	printf("Manufacturer String: %ls\n", wstr);

	// Read the Product String
	wstr[0] = 0x0000;
	res = hid_get_product_string(handle, wstr, MAX_STR);
	if (res < 0)
		printf("Unable to read product string\n");
	printf("Product String: %ls\n", wstr);

	// Read the Serial Number String
	wstr[0] = 0x0000;
	res = hid_get_serial_number_string(handle, wstr, MAX_STR);
	if (res < 0)
		printf("Unable to read serial number string\n");
	printf("Serial Number String: (%d) %ls", wstr[0], wstr);
	printf("\n");

	// Read Indexed String 1
	wstr[0] = 0x0000;
	res = hid_get_indexed_string(handle, 1, wstr, MAX_STR);
	if (res < 0)
		printf("Unable to read indexed string 1\n");
	printf("Indexed String 1: %ls\n", wstr);

	// Try to read from the device. There shoud be no
	// data here, but execution should not block.

	while (1) {
		res = hid_read(handle, buf, 64);

		if (res < 0) {
			printf("hid read error \n");
			break;
		}

		// remap changnel value to 1000~2000
		for (int i = 0; i < 4; i++) {
			channel_value[i] = buf[i];

		}
		channel_value[SWA] = (((buf[6] >> BIT_SWA) & 1 )+1) *1000;
		channel_value[SWB] = ((buf[6] >> BIT_SWB) & 3)*1000 ;
		if (channel_value[SWB] == 0) {
			channel_value[SWB] = 1500;
		}
		channel_value[SWC] = ((buf[6] >> BIT_SWC) & 3)*1000;
		if (channel_value[SWC] == 0) {
			channel_value[SWC] = 1500;
		}
		channel_value[SWD] = (((buf[6] >> BIT_SWD) & 1 ) + 1) *1000;
		channel_value[BTN_L] = (((buf[6] >> BIT_BTN_LEFT) & 1) +1 ) *1000;
		channel_value[BTN_R] = (((buf[6] >> BIT_BTN_RIGHT) & 1)+1)*1000;

		rc_packet pkg;

		for (int i = 0; i < 4; i++) {
			channel_value[i] = (float)channel_value[i] / 256.0 * 1000 + 1000;
		}

		for (int i = 0; i <= BTN_R; i++) {
			pkg.channels[i] = channel_value[i];
			printf("%d ", pkg.channels[i]);
		}
		printf("\n");

		memcpy(buffer, &pkg, sizeof(rc_packet));
		if(sendto(sock, buffer, sizeof(rc_packet), 0, (SOCKADDR*)&server, sizeof(server)) == SOCKET_ERROR){
			std::cerr << "sendto() failed with error code : " << WSAGetLastError() << std::endl;
			closesocket(sock);
			WSACleanup();
			break;
		}
		Sleep(20);
	}

	hid_close(handle);

	/* Free static HIDAPI objects. */
	hid_exit();

#ifdef WIN32
	system("pause");
#endif

	return 0;
}
