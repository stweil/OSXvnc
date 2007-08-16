/*
 *  EchoToVNC.h
 *  Echoware
 *
 *  Created by Jonathan on 3/3/06.
 *  Copyright 2006 __MyCompanyName__. All rights reserved.
 *
 */

#include <sys/socket.h>

// Some basic redifines

#undef TCP_NODELAY
#define TCP_NODELAY 0x01

#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define LPHOSTENT struct hostent *
#define LPIN_ADDR struct in_addr *

#define WSAEALREADY EALREADY
#define WSAEINVAL EINVAL
#define WSAEWOULDBLOCK EAGAIN
#define WSAEISCONN EISCONN
#define WSAECONNRESET ECONNRESET
#define WSAECONNABORTED ECONNABORTED
#define WSAESHUTDOWN ESHUTDOWN
#define WSAENOTSOCK ENOTSOCK

#define WSAGetLastError()   errno
#define closesocket    close
#define ZeroMemory     bzero

#define Sleep          usleep

// Nothing
#define FAR   

#define UINT unsigned int

#define CHAR        unsigned char
#ifndef DWORD
#define DWORD       long unsigned int
#endif
#define WORD  		unsigned short
#define BYTE        unsigned char
#define INFINITE 2^16

#define BOOL bool

#define	min(a,b)    (((a)<(b))?(a):(b))

#define TRUE 1
#define FALSE 0

#define __stdcall
#define LPVOID      void *

#ifndef MAKEWORD
#define MAKEWORD(b1, b2) ((WORD)(((BYTE)(b1)) | ((WORD)((BYTE)(b2))) << 8))
#endif

#ifndef LOBYTE
#define LOBYTE(w) ((BYTE)(w))
#endif

#ifndef HIBYTE
#define HIBYTE(w) ((BYTE)(((WORD)(w) >> 8) & 0xFF))
#endif

#define SD_RECEIVE SHUT_RD
#define SD_SEND    SHUT_WR
#define SD_BOTH    SHUT_RDWR

#define WM_USER 1024

// Functions which need to be implemented or the callers reworked
DWORD GetTickCount();

#define WPARAM void *
void *CreateThread(int, int, long unsigned int (*)(void *), void *, int, long unsigned int *);

bool ShutdownThread(void *, unsigned long, bool *, bool *);

bool PostThreadMessage(unsigned long , int, void *, int);	
bool PeekMessage(void *, int,int,int, int);

//#define STILL_ACTIVE 1
//#define WAIT_TIMEOUT 18

#define HANDLE void **
	
#ifndef ECHOWARE_API
#ifdef ECHOWARE_EXPORTS
#define ECHOWARE_API __attribute__ ((dllexport))
#else
//#define ECHOWARE_API __attribute__ ((dllimport))
#define ECHOWARE_API
#endif
#endif
