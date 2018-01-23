/* libtau header file
 * Copyright 2010 RidgeRun LLC
 * Covered by BSD 2-Clause License
 */

#ifndef __TAU_H
#define __TAU_H

/**
   For documentation on the camera see the Tau Camera User's Manual
   This API was based on version 1.20, Junary 2010
*/

#define TAU_COMM_NORMAL_TIMEOUT 1000 /* ms */

enum tauStatus {
	CAM_OK = 0,
	CAM_BUSY = 1,
	CAM_NOT_READY = 2,
	CAM_RANGE_ERROR = 3,
	CAM_CHECKSUM_ERROR = 4,
	CAM_UNDEFINED_PROCESS_ERROR = 5,
	CAM_UNDEFINED_FUNCTION_ERROR = 6,
	CAM_TIMEOUT_ERROR = 7,
	CAM_BYTE_COUNT_ERROR = 8,
	CAM_FEATURE_NOT_ENABLED = 9,
	/* Leave space for FLIR to define more codes */
	CAM_COMMUNICATION_ERROR = 100,

};
typedef enum tauStatus tauStatus;

enum tauCmd {
	NO_OP = 0x00,
	SET_DEFAULTS = 0x01,
	CAMERA_RESET = 0x02,

	GET_REVISION = 0x05,

	FFC_MODE_SELECT = 0x0B
	/*etc */
};
typedef enum tauCmd tauCmd;

typedef int tauHandler;

/** Opens the communication with a Tau camera over the specified 
 * device.
 * This function takes care of setting the serial port settings to the
 * right configuration
 * \param device is a string with the path to the RS232 device connected
 *   to the Tau camera. Ej. "/dev/ttyS0"
 * \returns a tauHandler to use with the rest of the library, or negative
 *  number in case of error
 */
tauHandler tauOpenFromSerial(char *device);

/** Creates a tauHandler from a standard file descriptior
 * \param fd a file descriptor
 * \returns a tauHandler to use with the rest of the library, or negative
 * number in case on error
 */
tauHandler tauOpenFromFd(int fd);

/** Returns the file discriptor assoicated with the tauHandler
 * \param handler a tau handler used to exchange data with a Tau camera
 * \returns a file descriptor
 * number in case on error
 */
int tauFd(tauHandler handler);

/** Sends a cmd to the Tau device and receives the response.
 * \param handler the handler for the Tau camera returned by tauOpen* functions
 * \param cmd the command to send to the camera
 * \param input (optional, may be NULL) the array of input data sent to camera
 * \param input_size if input is not NULL, the size of the input array
 * \param output (optional, may be NULL) the array of output data sent from camera
 * \param output_count on entry if output is not NULL, pointer to the size of the output array, on exit contains amount of valid data in output
 * \returns the status of the camerastatus;
 */
tauStatus tauDoCmd(tauHandler handler,tauCmd cmd, 
		   char *input, short input_size,
		   char* output, short *output_count);

/** Verifies Tau camera responds to NO-OP (0x00)
 * \param handler the handler for the Tau camera returned by tauOpen* functions
 * \returns zero on success.  On error, -1 is returned, and errno is set appropriately.
 */
tauStatus tauVerifyCommunication(tauHandler handler);

/** Closes the communication handler with the camera
 * \param handler the handler for the Tau camera returned by tauOpen* functions
 * \returns zero on success.  On error, -1 is returned, and errno is set appropriately.
 */
int tauClose(tauHandler handler);

#endif
