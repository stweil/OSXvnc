/*
 *  rfbserver.h
 *  Echoware
 *
 *  Created by admin on 2/15/07.
 *  Copyright 2007 __MyCompanyName__. All rights reserved.
 *
 */


// This structure represents the entire state of the RFB server
// We use it for passing off to the bundles

//#include <Cocoa/Cocoa.h>
#include <Carbon/Carbon.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#include <netdb.h>

typedef struct rfbserver {
	int vncServer;
	
	char *desktopName;
	int rfbPort;
	int rfbLocalhostOnly;

	pthread_mutex_t listenerAccepting;
	pthread_cond_t listenerGotNewClient;

    CGKeyCode *keyTable;
    unsigned char *keyTableMods;
    int *pressModsForKeys;
} rfbserver;

