// ----------------------------------------------------------------------------
// common.c
// ----------------------------------------------------------------------------
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
#include "common.h"

// ----------------------------------------------------------------------------
//  Show debug mesasges
// ----------------------------------------------------------------------------
void debugmsg(char *format, ...)
{
 	 va_list ap;
     FILE *debugstream;

     debugstream = fopen("debug.log", "a");    // open a file for debugging
	 fprintf(debugstream, "DEBUG: ");
	 va_start(ap, format);
	 vfprintf(debugstream, format, ap);
	 va_end(ap);

     fclose(debugstream);        // close the file
}
// ----------------------------------------------------------------------------
// report an error
// ----------------------------------------------------------------------------
void error(char *format, ...)
{
 	 va_list ap;
     FILE *debugstream;

     debugstream = fopen("debug.log", "a");    // open a file for debugging
	 fprintf(debugstream, "ERROR: ");
	 va_start(ap, format);
	 vfprintf(debugstream, format, ap);
	 va_end(ap);
     fclose(debugstream);        // close the file
}

// ----------------------------------------------------------------------------
// report a warning
// ----------------------------------------------------------------------------
void warning(char *format, ...)
{
	 va_list ap;
     FILE *debugstream;

     debugstream = fopen("debug.log", "a");    // open a file for debugging
	 fprintf(debugstream, "WARNING: ");
	 va_start(ap, format);
	 vfprintf(debugstream, format, ap);
	 va_end(ap);
     fclose(debugstream);        // close the file
}

// ----------------------------------------------------------------------------
// report information
// ----------------------------------------------------------------------------
void information(char *format, ...)
{
	 va_list ap;
     FILE *debugstream;

     debugstream = fopen("debug.log", "a");    // open a file for debugging
	 fprintf(debugstream, "INFO: ");
	 va_start(ap, format);
	 vfprintf(debugstream, format, ap);
	 va_end(ap);
     fclose(debugstream);        // close the file
}

// ----------------------------------------------------------------------------
