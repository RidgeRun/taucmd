/* libtau
 * Copyright 2010 RidgeRun LLC
 * Covered by BSD 2-Clause License
 */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/time.h>

#include "tau.h"
#include "tau-utils.h"

#define TAU_HEADER_SIZE 8

/***************************************************************************
 * Communication routines
 ***************************************************************************/

tauHandler tauOpenFromSerial(char *device)
{
	int fd;
	struct termios ios;

	fd = open(device, O_RDWR);
	if (fd < 0){
		perror("Unable to open device");
		return errno;
	}
	if (tcgetattr(fd,&ios) < 0) {
		perror("Unable to get serial device attributes");
		return errno;
	}
	/* CS8: 8n1 (8bit,no parity,1 stopbit)
	 * CLOCAL  : local connection, no modem contol
	 */
	ios.c_cflag = CS8 | CLOCAL; 
	/* Set to 57600 8N1 no flow control */
	if (cfsetospeed(&ios,B57600) < 0 ) {
		perror("Unable to set baudrate");
		return errno;
	}
	if (tcsetattr(fd,TCSAFLUSH,&ios) < 0) {
		perror("Unable to get serial device attributes");
		return errno;
	}
	return fd;
}


tauHandler tauOpenFromFd(int fd){
	return (tauHandler)fd;
}


int tauFd(tauHandler handler)
{
	return (int) handler;
}

/** Sends a command packet to a Tau camera
 * \param handler used for camera data exchange
 * \param buffer holds the command packet data to send
 * \param bufferSize number of bytes of data in the buffer
 * \returns tauStatus indicating the outcome of the attempted data transmission
 */
static tauStatus tauSendCmd(tauHandler handler, char *buffer, short bufferSize)
{
	tauStatus err;

	if ((err = write(tauFd(handler),buffer,bufferSize)) != bufferSize) {
		if (err < 0 ) {
			perror("Unable to send message");
			return err;
		} else {
			/* TODO, check for EAGAIN and send the rest */
			fprintf(stderr,"Unable to write all the bytes of the message\n");
			return err;
		}
	}
	return CAM_OK;
}

/** Reads a single character from a Tau camera
 * This function times out if no character is available.
 * \param handler used for camera data exchange
 * \param c character read from camera
 * \param msWait number of milliseconds to wait before returning timeout error
 * \returns tauStatus indicating the outcome of the attempted data read
 */
static tauStatus tauReadChar(tauHandler handler, char *c, int msWait)
{
	struct timeval tv;
	fd_set readfs;    /* file descriptor set */
	int fd = tauFd(handler);
	int len;
	int ret;
  
	tv.tv_sec = msWait / 1000;
	tv.tv_usec = (msWait % 1000) * 1000;

	FD_ZERO(&readfs);
	FD_SET(fd, &readfs); 

	printf(".");
	fflush(stdout);


	ret = select(fd + 1, &readfs, NULL, NULL, &tv);

	printf("post select: 0x%X\n", ret);
	fflush(stdout);

	if (ret < 0) {
		perror("Select() encountered an error\n");
		return CAM_COMMUNICATION_ERROR;
	}

	if (!FD_ISSET(fd, &readfs)) {
		return CAM_TIMEOUT_ERROR;
	}

	len = read(fd, c, 1);
	assert(len == 1);
	fprintf(stderr, "Read: 0x%2.2X\n", c);
	return CAM_OK;
}

/** Discards any stale data in the communication channel from tau camera
 * \param handler a tau handler used to exchange data with a Tau camera
 * \returns the status of attempted read
 */
static tauStatus tauFlushReceivedData(tauHandler handler)
{
	tauStatus status = CAM_OK;
	char data;

	while (status == CAM_OK) {
		status = tauReadChar(handler, &data, 10 );

		if ((status != CAM_OK) && (status != CAM_TIMEOUT_ERROR)) {
			fprintf(stderr,"Error: unexpect problem tossing data from Tau: %d\n", status);
		}
	}
	return CAM_OK;
}

/** Receive specified number of bytes of data from a Tau camera
 * \param handler the handler for the Tau camera returned by tauOpen* functions
 * \param buffer holder for the received data
 * \param readAmount number of bytes to read
 * \param msWait number of milliseconds to wait before returning timeout error
 * \returns the status of attempted read
 */
static int tauReadBinary(tauHandler handler, char *buffer, short readAmount, int msWait)
{
	tauStatus status = CAM_OK;
	int len = 0;

	while ((status == CAM_OK) && readAmount--) {
		status = tauReadChar(handler, buffer++, msWait );
		if (status == CAM_OK) {
			len++;
		}
	}
	return len;
}


int tauClose(tauHandler handler)
{
	return close(tauFd(handler));
}

/***************************************************************************
 * Packet handling routines
 ***************************************************************************/

/** Receive in a full packet, using the packet header data size value to know
 *  how much data to read
 * \param handler the handler for the Tau camera returned by tauOpen* functions
 * \param buffer holder for the data being received from the camera
 * \param bufferCount on entry indicates the buffer size, on exit contains the
 *        number of valid bytes of data in the buffer
 * \param msWait number of milliseconds to wait before returning timeout error
 * \returns the status of attempted read
 */
static tauStatus tauReceiveCmd(tauHandler handler, char *buffer, short *bufferCount, long msWait)
{
	int len;
	uint16_t *sptr;
	short data_len;

	assert(*bufferCount >= TAU_HEADER_SIZE);

	len = tauReadBinary(handler,buffer,TAU_HEADER_SIZE, msWait);

	if (len < 0 ) {
		perror("Unable to receive response header");
		*bufferCount = 0;
		return CAM_TIMEOUT_ERROR;
	} 

	if (len != TAU_HEADER_SIZE) {
		/* TODO, check for EAGAIN and read in the rest */
		fprintf(stderr,"Unable to receive all the bytes of the response header: %d/%d\n", len, TAU_HEADER_SIZE);
		hexDump("Partial header", buffer, len);
		*bufferCount = 0;
		return CAM_TIMEOUT_ERROR;
	}

	sptr = (uint16_t *) &buffer[4];
	data_len = ntohs(*sptr);

	assert ( * bufferCount >= (TAU_HEADER_SIZE + data_len + 2)); 

	len = read(handler,&buffer[TAU_HEADER_SIZE],data_len + 2);

	if (len < 0 ) {
	
		fprintf(stderr,"Unable to receive response data: %d\n", len);
		perror(NULL);
		*bufferCount = TAU_HEADER_SIZE;
		return CAM_TIMEOUT_ERROR;
	} 

	if (len != data_len+2) {
		/* TODO, check for EAGAIN and read in the rest */
		fprintf(stderr,"Unable to receive all the bytes of response data: %d/%d\n", len, data_len+2);
		*bufferCount = TAU_HEADER_SIZE;
		return CAM_TIMEOUT_ERROR;
	}
      
	*bufferCount = TAU_HEADER_SIZE + data_len + 2;

	return CAM_OK;
}

/** Converts a Tau camera command and assoicated data into a packet with CRCs
 * \param cmd Tau camera command
 * \param buffer to hold packet
 * \param bufferCount on entry indicates the buffer size, on exit contains the
 *        size of the packet (including the packet data) in bytes
 * \param data holds data to be included in the packet
 * \param dataCount number of data bytes in the data buffer
 */
static void tauBuildRequest(tauCmd cmd, char *buffer, short *bufferCount, char *data, short dataSize)
{
	uint16_t *sptr;

	assert(*bufferCount >= TAU_HEADER_SIZE + dataSize + 2);

	buffer[0] = 0x6E; /* process code */
	buffer[1] = CAM_OK; /* status */
	buffer[2] = 0x00; /* reserved */
	buffer[3] = cmd;
	sptr = (uint16_t *)&buffer[4]; /* data byte count */
	*sptr = htons(dataSize);
	sptr = (uint16_t *)&buffer[6]; /* header crc */
	*sptr = htons(crcCcitt16(buffer,6));
	if (dataSize && data) {
		memcpy(&buffer[8],data,dataSize);
		sptr = (uint16_t *)&buffer[TAU_HEADER_SIZE + dataSize]; /* header + data crc */
		*sptr = htons(crcCcitt16(buffer,TAU_HEADER_SIZE + dataSize));
		*bufferCount = TAU_HEADER_SIZE + dataSize + 2;
	} else {
		sptr = (uint16_t *)&buffer[TAU_HEADER_SIZE]; /* crc for header + crc and no data */
		*sptr = htons(crcCcitt16(buffer,TAU_HEADER_SIZE));
		*bufferCount = TAU_HEADER_SIZE +2;
	}
}

/** Verifies packet from Tau camera is error free, matches exepected response, and extras any assoicated data
 * \param cmd expected response command
 * \param buffer contains received data
 * \param bufferSize number of bytes of data in buffer
 * \param data holder for the command data data in the received packet, if any
 * \param dataCount on entry indicates the data buffer size, on exit contains the
 *        number of valid bytes of data in the data buffer
 * \returns the status of attempted read
 */
static tauStatus tauDecodeResponse(short cmd, char *buffer, short bufferSize, char *data, short *dataCount)
{
	tauStatus status;
	uint16_t *sptr;
	short data_len;

	if (buffer[0] != 0x6E){
		fprintf(stderr,"Invalid response process code\n");
		return CAM_COMMUNICATION_ERROR;
	}

	status = buffer[1];

	if (status != CAM_OK) {
		fprintf(stderr,"Camera reports error: %d\n", status);
		return status;
	}

	/* Check the message is right */
	if (buffer[3] != cmd){
		fprintf(stderr,"Response function number doesn't match requested command: %d/%d\n", buffer[3], cmd);
		return CAM_COMMUNICATION_ERROR;
	}

	sptr = (uint16_t *)&buffer[6];
    
	if (ntohs(*sptr) != crcCcitt16(buffer,6)) {
		fprintf(stderr,"Packet received from Tau camera contains header CRC error\n");
		return CAM_CHECKSUM_ERROR;
	}

	sptr = (uint16_t *)&buffer[4];

	data_len = ntohs(*sptr);

	sptr = (uint16_t *)&buffer[TAU_HEADER_SIZE + data_len];
    
	if (ntohs(*sptr) != crcCcitt16(buffer,TAU_HEADER_SIZE + data_len)) {
		fprintf(stderr,"Packet received from Tau camera contains overall packet CRC error\n");
		return CAM_CHECKSUM_ERROR;

	}


	if (data_len && data && *dataCount) {
		assert((dataCount == NULL) || (*dataCount >= data_len));
		memcpy(data,&buffer[TAU_HEADER_SIZE],data_len);
		if (dataCount) {
			*dataCount = data_len;
		}
	} else if (data_len > 0) {
		fprintf(stderr,"WARNING response data ignored\n");
		if (dataCount) {
			*dataCount = 0;
		}
	} else {
		if (dataCount) {
			*dataCount = 0;
		}
	}

	return status;
}

/***************************************************************************
 * High level packet exchange routines
 ***************************************************************************/

tauStatus tauDoCmd(tauHandler handler,tauCmd cmd,
		   char *input, short input_size,
		   char *output, short *output_count){
	short  msg_size = 10 + input_size;
	short rsp_size;
	char *msg, *rsp;
	unsigned short crc1, crc2;
	int err;
	tauStatus status = CAM_NOT_READY;

	if (output_count) {
		rsp_size = 10 + *output_count;
	} else {
		rsp_size = 10;
	}

	msg = malloc(msg_size);
	if (!msg) {
		fprintf(stderr,"%s: failed to allocate memory for message: %d\n",__FUNCTION__, msg_size);
		return CAM_NOT_READY;
	}
	rsp = malloc(rsp_size);
	if (!rsp) {
		fprintf(stderr,"%s: failed to allocate memory for response\n",__FUNCTION__);
		err = CAM_NOT_READY;
		goto free_msg;
	}

	tauBuildRequest(cmd, msg, &msg_size, input, input_size);
	hexDump("Sending request to Tau", msg, msg_size);

	if (err =  tauSendCmd(handler, msg, msg_size)) {
		goto free_rsp;
	}

	if (err =  tauReceiveCmd(handler, rsp, &rsp_size, TAU_COMM_NORMAL_TIMEOUT)) {
		goto free_rsp;
	}

	hexDump("Received response from Tau", rsp, rsp_size);

	if (err =  tauDecodeResponse(cmd, rsp, rsp_size, output, output_count)) {
		goto free_rsp;
	}

free_rsp: 
	free(rsp);
free_msg:
	free(msg);

	return err;
}

tauStatus tauVerifyCommunication(tauHandler handler)
{
	tauStatus status;

	status = tauFlushReceivedData(handler);

	if (status != CAM_OK) {
		return status;
	}
	return tauDoCmd(handler, NO_OP, NULL, 0, NULL, NULL);
}

/***************************************************************************
 * Development and testing routines
 ***************************************************************************/

#if ENABLE_TESTS
int tauRunTests(void)
{
	char data[10];
	uint16_t *sptr;
	char buffer[100];
	short buffer_len;
	tauStatus status;

	printf("CRC test #1\n");
	/* Table B-5: Sample FFC_MODE_SELECT (0x0B) Command, example 2 get mode select
	   packet to send: 0x6E 0x00 0x00 0x0B 0x00 0x00 0x2F 0x4A 0x00 0x00
	*/

	buffer_len = 100;
	tauBuildRequest(FFC_MODE_SELECT, buffer, &buffer_len, NULL, 0);
	hexDump("Table B-5: Sample FFC_MODE_SELECT (0x0B) Command, example 2 get mode select:\nexpected: 0x6E 0x00 0x00 0x0B 0x00 0x00 0x2F 0x4A 0x00 0x00",
		buffer, buffer_len);

	/* from CameraController.exe serial data capture */
	printf("CRC test #3\n");
	buffer_len = 100;
	tauBuildRequest(GET_REVISION, buffer, &buffer_len, NULL, 0);
	hexDump("CameraController.exe serial data capture\nexpected: 0x6E 0x00 0x00 0x05 0x00 0x00 0x34 0x4B 0x00 0x00",
		buffer, buffer_len);

	/* from tau in response to get revision */
	printf("CRC test #4\n");
	buffer_len = 100;
	data[0] = 0x0A;
	data[1] = 0x00;
	data[2] = 0x02;
	data[3] = 0x2B;
	data[4] = 0x08;
	data[5] = 0x00;
	data[6] = 0x00;
	data[7] = 0x40;
	tauBuildRequest(GET_REVISION, buffer, &buffer_len, data, 8);
	hexDump("CameraController.exe serial data capture\nexpected: 0x6E 0x00 0x00 0x05 0x00 0x08 0xB5 0x43\n          0x0A 0x00 0x02 0x2B 0x08 0x00 0x00 0x40\n          0x33 0x70\n\n",
		buffer, buffer_len);

	return 0;
}
#endif
