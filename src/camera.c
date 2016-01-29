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
****************************************************/

#include <string.h>
#include "asf.h"
#include <asf/common/utils/stdio/stdio_serial/stdio_serial.h>
#include "conf_board.h"
#include "conf_clock.h"
#include "global_var.h"
#include "camera.h"

// Initialization code used by all constructor types
common_init(void) {
    frameptr = 0;
    bufferLen = 0;
    serialNum = 0;
}



int begin(uint16_t baud) {
    begin(baud);
    return reset();
}

int reset() {
	uint8_t args[] = { 0x0 };

	return runCommand(VC0706_RESET, args, 1, 5);
}

int motionDetected() {
	if (readResponse(4, 200) != 4) {
		return 0;
	}
	if (!verifyResponse(VC0706_COMM_MOTION_DETECTED))
		return 0;

	return 1;
}


int setMotionStatus(uint8_t x, uint8_t d1, uint8_t d2) {
	uint8_t args[] = { 0x03, x, d1, d2 };

	return runCommand(VC0706_MOTION_CTRL, args, sizeof(args), 5);
}


uint8_t getMotionStatus(uint8_t x) {
	uint8_t args[] = { 0x01, x };

	return runCommand(VC0706_MOTION_STATUS, args, sizeof(args), 5);
}


int setMotionDetect(int flag) {
	if (!setMotionStatus(VC0706_MOTIONCONTROL,
		VC0706_UARTMOTION, VC0706_ACTIVATEMOTION))
		return 0;

	uint8_t args[] = { 0x01, flag };

	runCommand(VC0706_COMM_MOTION_CTRL, args, sizeof(args), 5);
}



int getMotionDetect(void) {
	uint8_t args[] = { 0x0 };

	if (!runCommand(VC0706_COMM_MOTION_STATUS, args, 1, 6))
		return 0;

	return camerabuff[5];
}

uint8_t getImageSize() {
	uint8_t args[] = { 0x4, 0x4, 0x1, 0x00, 0x19 };
	if (!runCommand(VC0706_READ_DATA, args, sizeof(args), 6))
		return -1;

	return camerabuff[5];
}

int setImageSize(uint8_t x) {
	uint8_t args[] = { 0x05, 0x04, 0x01, 0x00, 0x19, x };

	return runCommand(VC0706_WRITE_DATA, args, sizeof(args), 5);
}

/****************** downsize image control */

uint8_t getDownsize(void) {
	uint8_t args[] = { 0x0 };
	if (!runCommand(VC0706_DOWNSIZE_STATUS, args, 1, 6))
		return -1;

	return camerabuff[5];
}

int setDownsize(uint8_t newsize) {
	uint8_t args[] = { 0x01, newsize };

	return runCommand(VC0706_DOWNSIZE_CTRL, args, 2, 5);
}

/***************** other high level commands */

char * getVersion(void) {
	uint8_t args[] = { 0x01 };

	sendCommand(VC0706_GEN_VERSION, args, 1);
	// get reply
	if (!readResponse(CAMERABUFFSIZ, 200))
		return 0;
	camerabuff[bufferLen] = 0;  // end it!
	return (char *)camerabuff;  // return it!
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

	runCommand(VC0706_OSD_ADD_CHAR, args, strlen(str) + 3, 5);
	printBuff();
}

int setCompression(uint8_t c) {
	uint8_t args[] = { 0x5, 0x1, 0x1, 0x12, 0x04, c };
	return runCommand(VC0706_WRITE_DATA, args, sizeof(args), 5);
}

uint8_t getCompression(void) {
	uint8_t args[] = { 0x4, 0x1, 0x1, 0x12, 0x04 };
	runCommand(VC0706_READ_DATA, args, sizeof(args), 6);
	printBuff();
	return camerabuff[5];
}

int setPTZ(uint16_t wz, uint16_t hz, uint16_t pan, uint16_t tilt) {
	uint8_t args[] = { 0x08, wz >> 8, wz,
		hz >> 8, wz,
		pan >> 8, pan,
		tilt >> 8, tilt };

	return (!runCommand(VC0706_SET_ZOOM, args, sizeof(args), 5));
}


int getPTZ(uint16_t &w, uint16_t &h, uint16_t &wz, uint16_t &hz, uint16_t &pan, uint16_t &tilt) {
	uint8_t args[] = { 0x0 };

	if (!runCommand(VC0706_GET_ZOOM, args, sizeof(args), 16))
		return 0;
	printBuff();

	w = camerabuff[5];
	w <<= 8;
	w |= camera
        buff[6];

	h = camerabuff[7];
	h <<= 8;
	h |= camerabuff[8];

	wz = camerabuff[9];
	wz <<= 8;
	wz |= camerabuff[10];

	hz = camerabuff[11];
	hz <<= 8;
	hz |= camerabuff[12];

	pan = camerabuff[13];
	pan <<= 8;
	pan |= camerabuff[14];

	tilt = camerabuff[15];
	tilt <<= 8;
	tilt |= camerabuff[16];

	return 1;
}

/*
int takePicture() {
	frameptr = 0;
	return cameraFrameBuffCtrl(VC0706_STOPCURRENTFRAME);
}
*/

int resumeVideo() {
	return cameraFrameBuffCtrl(VC0706_RESUMEFRAME);
}

int TVon() {
	uint8_t args[] = { 0x1, 0x1 };
	return runCommand(VC0706_TVOUT_CTRL, args, sizeof(args), 5);
}
int TVoff() {
	uint8_t args[] = { 0x1, 0x0 };
	return runCommand(VC0706_TVOUT_CTRL, args, sizeof(args), 5);
}

int cameraFrameBuffCtrl(uint8_t command) {
	uint8_t args[] = { 0x1, command };
	return runCommand(VC0706_FBUF_CTRL, args, sizeof(args), 5);
}

uint32_t frameLength(void) {
	uint8_t args[] = { 0x01, 0x00 };
	if (!runCommand(VC0706_GET_FBUF_LEN, args, sizeof(args), 9))
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

uint16_t min(uint16_t x, uint16_t y){
    return (x<y ? x:y);
}

void takePic(){
    uint16_t jpglen = frameLength();
    
    // Read all the data up to # bytes!
    uint16_t wCount = 0; // For counting # of writes
    while (jpglen > 0) {
        // read 32 bytes at a time;
        uint8_t *buffer;
        uint8_t bytesToRead = min(32, jpglen);
        buffer = readPicture(bytesToRead);
        usart_write(BOARD_USART, buffer);
        if(++wCount >= 64) { 
            wCount = 0;
        }

        //Serial.print("Read ");  Serial.print(bytesToRead, DEC); Serial.println(" bytes");
        jpglen -= bytesToRead;
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

void sendCommand(uint8_t cmd, uint8_t args[] = 0, uint8_t argn = 0) {
                
		usart_write(BOARD_USART, (byte)0x56);
		usart_write(BOARD_USART, (byte)serialNum);
		usart_write(BOARD_USART, (byte)cmd);

		for (uint8_t i = 0; i<argn; i++) {
			usart_write(BOARD_USART, (byte)args[i]);
			//Serial.(" 0x");
			//Serial.(args[i], HEX);
		}

		printf(0x56, BYTE);
		printf(serialNum, BYTE);
		printf(cmd, BYTE);

		for (uint8_t i = 0; i<argn; i++) {
			printf(args[i], BYTE);
			//Serial.printf(" 0x");
			//Serial.printf(args[i], HEX);
		}
}

uint8_t readResponse(uint8_t numbytes, uint8_t timeout) {
	uint8_t counter = 0;
	bufferLen = 0;
	int avail;

	while ((timeout != counter) && (bufferLen != numbytes)) {
		avail = available();
		if (avail <= 0) {
			delay(1);
			counter++;
			continue;
		}
		counter = 0;
		// there's a byte!

		camerabuff[bufferLen++] = read();
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
	return 1;

}

void printBuff() {
	for (uint8_t i = 0; i< bufferLen; i++) {
		printf(" 0x");
		printf(camerabuff[i], HEX);
	}
	Serial.println();
}