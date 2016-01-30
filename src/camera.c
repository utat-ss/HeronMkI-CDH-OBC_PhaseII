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
#include "time.h"

// Initialization code used by all constructor types
void cam_initialize(){
	common_init();
	cam_begin();
	setImageSize(VC0706_320x240); 
	setBaud38400();
}

void common_init() {
    frameptr = 0;
    bufferLen = 0;
}


int cam_begin() {
	setBaud38400();
    return reset();
}

void clear_cam_buffer(){

	for (uint32_t i = 0; i < 64000; i++)
		camerabuff[i] = 0;

}

int reset() {
	uint8_t args[] = { 0x0 };

	return runCommand(VC0706_RESET, args, 1, 5, true);
}


int setImageSize(uint8_t x) {
	uint8_t args[] = { 0x05, 0x04, 0x01, 0x00, 0x19, x };

	return runCommand(VC0706_WRITE_DATA, args, sizeof(args), 5, true);
}


/***************** baud rate commands */


char* setBaud38400() {
	uint8_t args[] = { 0x03, 0x01, 0x2A, 0xF2 };

	sendCommand(VC0706_SET_PORT, args, sizeof(args));
	// get reply
	if (!readResponse(CAMERABUFFSIZ, 200))
		return 0;
	camerabuff[bufferLen] = 0;  // end it!
	return (char *)camerabuff;  // return it!
}

/****************** high level photo comamnds */

int setCompression(uint8_t c) {
	uint8_t args[] = { 0x5, 0x1, 0x1, 0x12, 0x04, c };
	return runCommand(VC0706_WRITE_DATA, args, sizeof(args), 5, true);
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
	//CAMERA_BASE = 81920;
	uint32_t cam_write_size;
	uint32_t wCount = 0;
	uint32_t cam_base = CAMERA_BASE;
	while ( (wCount < 64000) || (numWrites > 0) || (cam_base < (CAMERA_BASE+64000)) ){ //writing to SPI memory in 256byte chunks
		
		numWrites -= wCount;
		
		cam_write_size = min(256, numWrites);		
		spimem_write(cam_base, (camerabuff+wCount), cam_write_size);
		
		cam_base+=256;
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

void sendCommand(uint32_t cmd, uint8_t *args, uint8_t argn) {
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
	uint32_t delay = 1;
	uint32_t temp;

	while ((timeout != counter) && (bufferLen != numbytes)) {
		
		if (usart_get_status(BOARD_USART) == US_CSR_RXRDY) {
			delay_ms(delay);
			counter++;
		}

		else{
			usart_getchar(BOARD_USART, &temp); //usart_getchar outputs 0 upon success
			camerabuff[bufferLen++] = (uint8_t)temp;
		}
	}
	
	camerabuff[bufferLen] = 0;
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
