//---------------------------------------------------------------------------
// Common logging macros
//---------------------------------------------------------------------------
#ifndef _COMMON_H
#define _COMMON_H

#define PRINT_DEBUG 	debugmsg
#define PRINT_INFO      information
#define PRINT           information

void debugmsg(char *format, ...);   //  Show debug mesasges
void error(char *format, ...);       // report an error
void warning(char *format, ...);        // report a warning
void information(char *format, ...);    // report information

#define usleep(us) Sleep(us/1000);
//#define strtok_r strtok_s

//---------------------------------------------------------------------------
// Common defines
//---------------------------------------------------------------------------
#define DEFAULT_VID				0x03E7
#define DEFAULT_PID				0x2150	// Myriad2v2 ROM

#define DEFAULT_OPEN_VID		DEFAULT_VID
#define DEFAULT_OPEN_PID		0xf63b	// Once opened in VSC mode, VID/PID change


#endif
//---------------------------------------------------------------------------

