/* tau-utils.h
 * Copyright 2010 RidgeRun LLC
 * Covered by BSD 2-Clause License
 */

#ifndef TAU_UTILS_H
#define TAU_UTILS_H

#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <memory.h>
#include <stdio.h>
#include <stdint.h>

/***************************************************************************
 * Debug Macros
 ***************************************************************************/

# define real_dbg_time do {						\
		struct timeval time;					\
		int rc;							\
		rc=gettimeofday(&time, NULL);				\
		if(rc==0) {						\
			fprintf(stderr, "%lu.%06lu: ", time.tv_sec, time.tv_usec); \
		}							\
	} while (0)

#define dbg_time do { } while (0)

# define  dbg(format, arg...) if (debug_level > 0)			\
		do { dbg_time ; fprintf(stderr, "%s: " format "\n", __FUNCTION__, ##arg); } while (0);

# define vdbg(format, arg...) if (debug_level > 1)			\
		do { dbg_time ; fprintf(stderr, "%s: " format "\n", __FUNCTION__, ##arg); } while (0);

# define  qdbg(format, arg...) if (debug_level > 0) fprintf(stderr, format, ##arg);

# define PASSERT(truth, msg) do { if (!truth) { fprintf(stderr, "ASSERT: failed, %s\n", msg); } } while (0);


/************************************************************************
 * Definitions
 ************************************************************************/

/************************************************************************
 * Public variables
 ************************************************************************/

extern int debug_level;

/************************************************************************
 * Public Functions
 ************************************************************************/

/** Prints the contents of a buffer as human readable data in hexidecimal
 *  format.  Also prints an optional title string.
 * \param title string to print before printing buffer contents
 * \param data buffer holding data to print
 * \param data_count number of bytes of data to print
 */
void hexDump(const char *title, const void *data, const long data_count);


/** Converts pairs of ASCII hex characters to binary stopping at end of
 *  string or when an illegal character is encountered with blanks
 *  being skipped
 * \param buffer holds the bytes of binary data as each pair of
 *        ASCII hex characters are processed
 * \param buffer_len maximum number of bytes of ASCII data that can be
 *        stored
 * \param ascii_hex NULL terminated string of ASCII hex characters
 * \return number of bytes of binary data stored in buffer
 */
int asciiHexToBinary(char *buffer, const int buffer_len, char *ascii_hex);

/** Sets the debug level
 * \param level 0, debug off, 1 normal debug, 2, verbose debug
 */
void setDebugLevel(int level);

/** Calculates the CCITT16 CRC (using a starting value of 0x0000)
 * \param buffer contains bytes of binary data
 * \param length number of bytes of data in buffer
 * \return CCITT16 CRC value for the data in buffer
 */
unsigned short crcCcitt16(char *buffer, unsigned short length);
#endif
