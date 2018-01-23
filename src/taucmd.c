/* FLIR Tau 320 camera configuration and status utility
 * Copyright 2010 RidgeRun LLC
 * Covered by BSD 2-Clause License
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

#define MAX_COMMAND_LENGTH   512
#define MAX_ARGUMENTS        10
#define MAX_ARGUMENT_LENGTH  128
#define MAX_FILENAME_LENGTH  256
#define MAX_TAU_DATA_LEN     64

/************************************************************************
 * Data types
 ************************************************************************/

/************************************************************************
 * Public Data
 ************************************************************************/

/************************************************************************
 * Private Data
 ************************************************************************/

static char filename[MAX_FILENAME_LENGTH];
static char tau_host[MAX_FILENAME_LENGTH];
static unsigned int tau_port;
static char tau_host[MAX_FILENAME_LENGTH];

/************************************************************************
 * Forward Function Declarations
 ************************************************************************/

/************************************************************************
 * Private Functions
 ************************************************************************/

/** Does nothing if there is no error, otherwise prints a message and exits
 * \param msg string to print if there is an error
 * \param ret return code, non-zero means error
 */
static inline void check_results(char *msg, int ret)
{
        if (!ret) {
                return;
        }

        fprintf(stderr, "%s\n", msg);
        exit(-1);
}


/** Displays application help message
 * \param progname program name
 * \param e_help non-zero if extended help should be displayed
 */
static void show_usage(const char *progname, int e_help)
{
        fprintf(stderr, "Usage: %s [-h|-H] [-d <debug level>] [-f <device filename> | -n <IP:port>] <command> [<command parameters>]\n", progname);

        fprintf(stderr, "-h                           Display this help information.\n");
        fprintf(stderr, "-H                           Display this help information along with list of all <commands>.\n");
        fprintf(stderr, "-d <debug level>             Set the debug level.  Default is 0, off.  1 is enabled. 2 is verbose.\n"); 
        fprintf(stderr, "-f <device filename>         Exchange data with tau device over specified filename\n");
        fprintf(stderr, "-n <IP:port>                 Exchange data with tau via a TCP connection to the specified IP address and port\n"); 
        fprintf(stderr, "<command>                    two digit hex number\n");
        fprintf(stderr, "<command parameters>         zero or more sets of two digit hex numbers\n");

	if (e_help) {
		/* TODO, add support for <command> as text strings in addition to two digit hex numbers */
	}

        fprintf(stderr, "\n");

        fprintf(stderr, "Examples:\n");
        fprintf(stderr, "          1) Send no-op command in raw format to tau connected via serial on /dev/ttyS0\n");
        fprintf(stderr, "             %s -f /dev/ttyS0 00\n", progname);
        fprintf(stderr, "          2) Get serial number using raw format with tau connected remotely via telnetd on machine sdk.ridgerun.net port 5471\n");
        fprintf(stderr, "             %s -n sdk.ridgerun.net:5471 04\n", progname);
        fprintf(stderr, "          3) Set gain mode to manual with tau connected via serial on /dev/ttyS0\n");
        fprintf(stderr, "             %s -f /dev/ttyS0 GAIN_MODE 0000\n", progname);
        fprintf(stderr, "\n");
        fprintf(stderr, "\n");
}


/** Parses command line options, setting application global variables 
 * holding user specified preferences.
 * \param argc number of command line options
 * \param argv array of options
 * \returns number of parameters that follow the options
 */
static int parse_options(int argc, char *argv[])
{
        int option;
	char *ptr;
	unsigned int len;
	int level;

        /* Parse for other options */
        while ((option=getopt(argc,argv,"hHd:f:n:")) != EOF) {
                switch (option){
                case 'h' :
			show_usage(argv[0], 0);
			exit(0);
                        break;
		case 'H' :
			show_usage(argv[0], 1);
			exit(0);
                        break;
                case 'd' :
                        level = atoi(optarg);
			setDebugLevel(level);
                        vdbg("Program debug level set to %d", level);
                        break;

                case 'f' :
			strncpy(filename, optarg, MAX_FILENAME_LENGTH);
			filename[MAX_FILENAME_LENGTH-1]='\0';
			vdbg("Tau data exchanged over file %s", filename);
                        break;

                case 'n' :
		  
			ptr = index(optarg, ':');

			if (! ptr) {
				show_usage(argv[0], 0);
				fprintf(stderr, "\nERROR: when using -n option need colon to separate host address from  port number\n\n");
				exit(-1);
			}

			len = (ptr - optarg) < MAX_FILENAME_LENGTH ? (ptr-optarg) : MAX_FILENAME_LENGTH - 1;

			if (! len) {
				show_usage(argv[0], 0);
				fprintf(stderr, "\nERROR: when using -n option need to separate host address before the colon\n\n");
				exit(-1);
			}

			memcpy(tau_host, optarg, len);
			tau_host[ptr-optarg] = '\0';

			ptr++;

			if (! ptr[0]) {
				show_usage(argv[0], 0);
				fprintf(stderr, "\nERROR: when using -n option need to specify port number after the colon\n\n");
				exit(-1);
			}

			tau_port = atoi(ptr);
		 
			if (! tau_port) {
				show_usage(argv[0], 0);
				fprintf(stderr, "\nERROR: when using -n option port number has to be a number greater than zero\n\n");
				exit(-1);
			}

			vdbg("Network connection to %s, port %d", tau_host, tau_port);
			break;

		default :
			show_usage(argv[0], 0);
			fprintf(stderr, "\nERROR: unknown option '%c'\n\n", option);
			exit(-1);
		}
	}

	return optind;
}


/***************************************************************************
 * Public Functions
 ***************************************************************************/

int main(int argc, char **argv, char **envp)
{
	int fd;
	tauHandler handle;
	int ret = 0;
	int idx;
	char cmd;
	char raw_buffer[MAX_TAU_DATA_LEN];
	short raw_buffer_count;
	char result_buffer[MAX_TAU_DATA_LEN];
	short result_count;

        idx = parse_options(argc, argv);

	if ( !filename[0] && !tau_host[0]) {
		fprintf(stderr, "ERROR: must specify means to communication with Tau - either a file name or network address:port\n");
		exit(-1);
	}

	if (filename[0]) {
		dbg("Opening tau communication file: %s", filename);
		handle = tauOpenFromSerial(filename);
		if (handle < 0) {
			perror("ERROR: could not open tau communication file");
			exit(-1);
		}
	} else if (tau_host[0]) {
		fprintf(stderr, "TODO ERROR: network communication not implemented\n");
		exit(-1);
		handle = tauOpenFromFd(fd);
	} else {
		fprintf(stderr, "ERROR: must specify means to communication with Tau - either a file name or network address:port\n");
		exit(-1);
	}

	vdbg("Attempting to communication with Tau camera");
        ret = tauVerifyCommunication(handle);
	check_results("ERROR: Failed to get a response from Tau camera", ret);

	if (idx < argc) {
		ret = asciiHexToBinary(raw_buffer, MAX_TAU_DATA_LEN, argv[idx++]);
		if (ret != 1) {
			fprintf(stderr, "\nERROR: <command> must be two ASCII digits\n\n");
			exit(-1);
		}
		
		cmd = raw_buffer[0];
		dbg("<command>: 0x%X", cmd);

		if (idx < argc) {
			raw_buffer_count = asciiHexToBinary(raw_buffer, MAX_TAU_DATA_LEN, argv[idx++]);
			if (raw_buffer_count > 0) {
				hexDump("raw data", raw_buffer, raw_buffer_count);
			}
		} else {
			raw_buffer_count = 0;
		}

		if (idx != argc) {
			fprintf(stderr, "ERROR: unexpected parameter after <command parameter>: '%s'\n\n", argv[idx]);
			exit(-1);
		}

		result_count = MAX_TAU_DATA_LEN;
		ret = tauDoCmd(handle, cmd, raw_buffer, raw_buffer_count, result_buffer, &result_count);
		check_results("ERROR: command failed", ret);
	}

	return tauClose(handle);
}

/***************************************************************************
 * Command line parser test cases

  243  ./src/taucmd -d 2
  244  ./src/taucmd -d 2 -f foo
  245  ./src/taucmd -d 2 -n foo:bar
  246  ./src/taucmd -d 2 -n foo:1
  247  ./src/taucmd -d 2 -n :1
  248  ./src/taucmd -d 2 -n foo:
  249  ./src/taucmd -d 2 -n foo
  297  ./src/taucmd -d 2 -f /dev/ttyUSB0 1
  297  ./src/taucmd -d 2 -f /dev/ttyUSB0 00
  297  ./src/taucmd -d 2 -f /dev/ttyUSB0 1234
  297  ./src/taucmd -d 2 -f /dev/ttyUSB0 0A 0000

***************************************************************************/
