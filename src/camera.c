/***************************************************
This is a library for the Adafruit TTL JPEG Camera (VC0706 chipset)
Pick one up today in the adafruit shop!
------> http://www.adafruit.com/products/397
These displays use Serial to communicate, 2 pins are required to interface
Adafruit invests time and resources providing this open source code,
please support Adafruit and open-source hardware by purchasing
products from Adafruit!
Written by Limor Fried/Ladyada for Adafruit Industries.
BSD license, all text above must be included in any redistribution

Modified by Brendan Graham

****************************************************/

#include <string.h>
#include "asf.h"
#include <asf/common/utils/stdio/stdio_serial/stdio_serial.h>
#include "conf_board.h"
#include "conf_clock.h"
#include "global_var.h"
#include "camera.h"
#include "spimem.h"
#include "usart_func.h"

// Initialization code used by all constructor types
int cam_init(){
	common_init();
	//cam_begin(baud_rate);
	
}

void common_init() {
    frameptr = 0;
    bufferLen = 0;
    serialNum = 0;
}


int cam_begin(uint16_t baud) {
    begin(baud);
    return reset();
}

int reset() {
	uint8_t args[] = { 0x0 };

	return runCommand(VC0706_RESET, args, 1, 5, true);
}

int motionDetected() {
	if (readResponse(4, 200) != 4) {
		return 0;
	}
	if (!verifyResponse(VC0706_COMM_MOTION_DETECTED))
		return 0;

	return 1;
}

uint8_t getImageSize() {
	uint8_t args[] = { 0x4, 0x4, 0x1, 0x00, 0x19 };
	if (!runCommand(VC0706_READ_DATA, args, sizeof(args), 6, true))
		return -1;

	return camerabuff[5];
}

int setImageSize(uint8_t x) {
	uint8_t args[] = { 0x05, 0x04, 0x01, 0x00, 0x19, x };

	return runCommand(VC0706_WRITE_DATA, args, sizeof(args), 5, true);
}

/****************** downsize image control */

uint8_t getDownsize(void) {
	uint8_t args[] = { 0x0 };
	if (!runCommand(VC0706_DOWNSIZE_STATUS, args, 1, 6, true))
		return -1;

	return camerabuff[5];
}

int setDownsize(uint8_t newsize) {
	uint8_t args[] = { 0x01, newsize };

	return runCommand(VC0706_DOWNSIZE_CTRL, args, 2, 5, true);
}


/***************** baud rate commands */

char* setBaud9600() {
	uint8_t args[] = { 0x03, 0x01, 0xAE, 0xC8 };

	sendCommand(VC0706_SET_PORT, args, sizeof(args));
	// get reply
	if (!readResponse(CAMERABUFFSIZ, 200))
		return 0;
	camerabuff[bufferLen] = 0;  // end it!
	return (char *)camerabuff;  // return it!

}

char* setBaud19200() {
	uint8_t args[] = { 0x03, 0x01, 0x56, 0xE4 };

	sendCommand(VC0706_SET_PORT, args, sizeof(args));
	// get reply
	if (!readResponse(CAMERABUFFSIZ, 200))
		return 0;
	camerabuff[bufferLen] = 0;  // end it!
	return (char *)camerabuff;  // return it!
}

char* setBaud38400() {
	uint8_t args[] = { 0x03, 0x01, 0x2A, 0xF2 };

	sendCommand(VC0706_SET_PORT, args, sizeof(args));
	// get reply
	if (!readResponse(CAMERABUFFSIZ, 200))
		return 0;
	camerabuff[bufferLen] = 0;  // end it!
	return (char *)camerabuff;  // return it!
}

char* setBaud57600() {
	uint8_t args[] = { 0x03, 0x01, 0x1C, 0x1C };

	sendCommand(VC0706_SET_PORT, args, sizeof(args));
	// get reply
	if (!readResponse(CAMERABUFFSIZ, 200))
		return 0;
	camerabuff[bufferLen] = 0;  // end it!
	return (char *)camerabuff;  // return it!
}

char* setBaud115200() {
	uint8_t args[] = { 0x03, 0x01, 0x0D, 0xA6 };

	sendCommand(VC0706_SET_PORT, args, sizeof(args));
	// get reply
	if (!readResponse(CAMERABUFFSIZ, 200))
		return 0;
	camerabuff[bufferLen] = 0;  // end it!
	return (char *)camerabuff;  // return it!
}

/****************** high level photo comamnds */

void OSD(uint8_t x, uint8_t y, char *str) {
	if (strlen(str) > 14) { str[13] = 0; }

	uint8_t args[17] = { strlen(str), strlen(str) - 1, (y & 0xF) | ((x & 0x3) << 4) };

	for (uint8_t i = 0; i<strlen(str); i++) {
		char c = str[i];
		if ((c >= '0') && (c <= '9')) {
			str[i] -= '0';
		}
		else if ((c >= 'A') && (c <= 'Z')) {
			str[i] -= 'A';
			str[i] += 10;
		}
		else if ((c >= 'a') && (c <= 'z')) {
			str[i] -= 'a';
			str[i] += 36;
		}

		args[3 + i] = str[i];
	}

	runCommand(VC0706_OSD_ADD_CHAR, args, strlen(str) + 3, 5, true);
}

int setCompression(uint8_t c) {
	uint8_t args[] = { 0x5, 0x1, 0x1, 0x12, 0x04, c };
	return runCommand(VC0706_WRITE_DATA, args, sizeof(args), 5, true);
}

uint8_t getCompression(void) {
	uint8_t args[] = { 0x4, 0x1, 0x1, 0x12, 0x04 };
	runCommand(VC0706_READ_DATA, args, sizeof(args), 6, true);
	return camerabuff[5];
}

int setPTZ(uint16_t wz, uint16_t hz, uint16_t pan, uint16_t tilt) {
	uint8_t args[] = { 0x08, wz >> 8, wz,
		hz >> 8, wz,
		pan >> 8, pan,
		tilt >> 8, tilt };

	return (!runCommand(VC0706_SET_ZOOM, args, sizeof(args), 5, true));
}




int resumeVideo() {
	return cameraFrameBuffCtrl(VC0706_RESUMEFRAME);
}


int cameraFrameBuffCtrl(uint8_t command) {
	uint8_t args[] = { 0x1, command };
	return runCommand(VC0706_FBUF_CTRL, args, sizeof(args), 5, true);
}

uint32_t frameLength(void) {
	uint8_t args[] = { 0x01, 0x00 };
	if (!runCommand(VC0706_GET_FBUF_LEN, args, sizeof(args), 9, true))
		return 0;

	uint32_t len;
	len = camerabuff[5];
	len <<= 8;
	len |= camerabuff[6];
	len <<= 8;
	len |= camerabuff[7];
	len <<= 8;
	len |= camerabuff[8];

	return len;
}


uint8_t available(void) {
	return bufferLen;
}


uint8_t * readPicture(uint8_t n) {
	uint8_t args[] = { 0x0C, 0x0, 0x0A,
		0, 0, frameptr >> 8, frameptr & 0xFF,
		0, 0, 0, n,
		CAMERADELAY >> 8, CAMERADELAY & 0xFF };

	if (!runCommand(VC0706_READ_FBUF, args, sizeof(args), 5, 0))
		return 0;


	// read into the buffer PACKETLEN!
	if (readResponse(n + 5, CAMERADELAY) == 0)
		return 0;


	frameptr += n;

	return camerabuff;
}


void takePic(){
    uint16_t jpglen = frameLength();
    
    // Read all the data up to # bytes!
    uint32_t wCount = 0; // For counting # of writes
    while (jpglen > 0 && wCount <55000) {
        // read 32 bytes at a time;
        uint8_t *buffer;
        uint8_t bytesToRead = 1;
        buffer = readPicture(bytesToRead);
		camerabuff[wCount] = *buffer;
        usart_write(BOARD_USART, *buffer);
        
		wCount++;
        //Serial.print("Read ");  Serial.print(bytesToRead, DEC); Serial.println(" bytes");
        jpglen -= bytesToRead;
    }    
	
	storePicinSPIMem(wCount);
}

void storePicinSPIMem(int numWrites){
	CAMERA_BASE = 81920;
	uint32_t cam_write_size;
	uint32_t wCount = 0;
	
	while ( (wCount < 64000) || (numWrites > 0) || (CAMERA_BASE < (81920+64000)) ){ //writing to SPI memory in 256byte chunks
		
		numWrites -= wCount;
		
		cam_write_size = min(256, numWrites);		
		spimem_write(CAMERA_BASE, (camerabuff+wCount), cam_write_size);
		
		CAMERA_BASE+=256;
		wCount+=256;
	}
}

/**************** low level commands */

int runCommand(uint8_t cmd, uint8_t *args, uint8_t argn,
	uint8_t resplen, int flushflag) {
	// flush out anything in the buffer?
	
	if (flushflag) {
		readResponse(100, 10);
	}
        
	sendCommand(cmd, args, argn);
	if (readResponse(resplen, 200) != resplen)
		return 0;
	if (!verifyResponse(cmd))
		return 0;
	return 1;
}

void sendCommand(uint32_t cmd, uint8_t args[], uint8_t argn) {
                
		
		usart_write(BOARD_USART, 0x56);
		usart_write(BOARD_USART, serialNum);
		usart_write(BOARD_USART, cmd);

		for (uint8_t i = 0; i<argn; i++) {
			usart_write(BOARD_USART, args[i]);
			//Serial.(" 0x");
			//Serial.(args[i], HEX);
		}
} 

uint8_t readResponse(uint8_t numbytes, uint8_t timeout) {
	uint8_t counter = 0;
	bufferLen = 0;
	int avail;
	uint32_t delay = 1;
	
	while ((timeout != counter) && (bufferLen != numbytes)) {
		avail = available();
		
		if (avail <= 0) {
			delay_s(delay);
			counter++;
			continue;
		}
		
		counter = 0;
		// there's a byte!

		//camerabuff[bufferLen++] = read();
	}
	//printBuff();
	//camerabuff[bufferLen] = 0;
	//Serial.println((char*)camerabuff);
	return bufferLen;
}

int verifyResponse(uint8_t command) {
	if ((camerabuff[0] != 0x76) ||
		(camerabuff[1] != serialNum) ||
		(camerabuff[2] != command) ||
		(camerabuff[3] != 0x0))
		return 0;
	else
		return 1;
}
