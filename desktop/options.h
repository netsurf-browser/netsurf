
#ifndef _NETSURF_DESKTOP_OPTIONS_H_
#define _NETSURF_DESKTOP_OPTIONS_H_

struct options;

#include "netsurf/riscos/options.h"

struct options
{
	/* global options */
	int http;
	char* http_proxy;
	int http_port;
	
	/* platform specific options */
	PLATFORM_OPTIONS
};

extern struct options OPTIONS;

void options_init(struct options* opt);
void options_write(struct options*, char* filename);
void options_read(struct options*, char* filename);

#endif

