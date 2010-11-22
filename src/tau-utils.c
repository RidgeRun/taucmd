/* Utility routines for FLIR Tau 320 camera configuration and status tool
 * Copyright 2010 RidgeRun LLC
 */

#include <stdio.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <assert.h>
#include <fcntl.h>
#include <termio.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "tau.h"
#include "tau-utils.h"

/************************************************************************
 * Constants
 ************************************************************************/

#define BYTES_PER_ROW 8

/************************************************************************
 * Data types
 ************************************************************************/

/************************************************************************
 * Public Data
 ************************************************************************/
int debug_level = 0;

/************************************************************************
 * Private Data
 ************************************************************************/

/************************************************************************
 * Forward Function Declarations
 ************************************************************************/

/************************************************************************
 * Private Functions
 ************************************************************************/

/************************************************************************
 * Public Functions
 ************************************************************************/

void hexDump(const char *title, const void *data, const long data_count)
{
	int i, j, k, l;
	unsigned char *buffer = (unsigned char *)data;

	qdbg("\n%s (len: %ld)\n", title, data_count);

	for (i = 0, k = 0; i < data_count; i += BYTES_PER_ROW) {
		qdbg("  0x%04X: ", i);
		for (j = 0, l = k; j < BYTES_PER_ROW; j++, l++) {
			if (l < data_count) {
				qdbg("0x%02X ", buffer[l]);
			} else {
				qdbg("     ");
			}
		}
		for (j = 0, l = k; j < BYTES_PER_ROW; j++, k++, l++) {
			if (l < data_count) {
				int c = buffer[k];
				if ((c >= 0x20) && (c < 0x7f)) {
					qdbg("%c", c);
				} else {
					qdbg(".");
				}
			} else {
				qdbg(" ");
			}
		}
		qdbg("\n");
	}
}


int asciiHexToBinary(char *buffer, const int buf_len, char *ascii_hex)
{
	char c;
	char *ptr = ascii_hex;
	int count = 0;
	int nibbles = 0; /* number of nibbles stored in val */
	char val = 0;

	while (*ptr) {
		c = toupper(*ptr);

		if (isblank(c)) {
			ptr++;
			continue;
		}

		if (isdigit(c)) {
			val = (val << 4) + (c - '0');
			nibbles++;
		} else if ((c >= 'A') && (c <= 'F')) {
			val = (val << 4) + (c - 'A' + 10);
			nibbles++;
		} else {
			break;
		}

		if (nibbles == 2) {
			buffer[count++] = val;
			val = 0;
			nibbles = 0;
		}

		ptr++;
	}
       
	return count;
}


void setDebugLevel(int level)
{
	debug_level = level;
}



/* crcCcitt16() from http://www.lammertbies.nl/comm/software/index.html */

#define                 P_CCITT     0x1021

static unsigned short   crc_tabccitt[256];
static int              crc_tabccitt_init       = 0;

/** Fills the array of 256 values with the pre-calculated CRC value
 */
static void initCrcCcitt16Tab( void ) {

	int i, j;
	unsigned short crc, c;

	for (i=0; i<256; i++) {

		crc = 0;
		c   = ((unsigned short) i) << 8;

		for (j=0; j<8; j++) {

			if ( (crc ^ c) & 0x8000 ) {
				crc = ( crc << 1 ) ^ P_CCITT;
			} else {
				crc =   crc << 1;
			}

			c = c << 1;
		}

		crc_tabccitt[i] = crc;
	}

	crc_tabccitt_init = 1;
} 


/** Returns a new CCITT16 CRC value calcualted using an existing CRC 
 *  value and adding another character to the CRC
 * \param crc existing CCITT16 CRC value
 * \param c character to add to the CRC
 * \return updated CCITT16 CRC value
 */
static unsigned short updateCrcCcitt16( unsigned short crc, char c )
{

	unsigned short tmp, short_c;

	short_c  = 0x00ff & (unsigned short) c;

	if ( ! crc_tabccitt_init )
	{
		initCrcCcitt16Tab();
	}

	tmp = (crc >> 8) ^ short_c;
	crc = (crc << 8) ^ crc_tabccitt[tmp];

	return crc;

}


unsigned short crcCcitt16(char *data, unsigned short length)
{
	short crc = 0x0000; /* newer ccitt 16 uses 0xFFFF */

	while (length-- > 0) {
		crc = updateCrcCcitt16(crc, *data++);
	}

	return crc;
}
