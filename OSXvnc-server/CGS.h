/*
 *  CGS.h
 *  OSXvnc
 *
 *  Created by Mihai Parparita on Sat Jun 15 2002.
 *  Copyright (c) 2002 Mihai Parparita. All rights reserved.
 *
 */

#pragma once

#include <Carbon/Carbon.h>

typedef int CGSConnectionRef;

extern CGError CGSNewConnection(void* unknown, CGSConnectionRef* newConnection);
extern CGError CGSReleaseConnection(CGSConnectionRef connection);

extern CGError CGSGetGlobalCursorDataSize(CGSConnectionRef connection, int* size);
extern CGError CGSGetGlobalCursorData(CGSConnectionRef connection,
                                      unsigned char* cursorData,
                                      int* size,
                                      int* bytesPerRow,
                                      CGRect* cursorRect,
                                      CGPoint* hotspot,
                                      int* depth,
                                      int* components,
                                      int* bitsPerComponent);

extern CGError CGSGetCurrentCursorLocation(CGSConnectionRef connection, CGPoint* point);
extern int CGSCurrentCursorSeed(void);
extern int CGSHardwareCursorActive(); // flaky? doesn't seem to work after DM is initialized

#ifndef SAVECOLORS
#define SAVECOLORS\
	RGBColor		oldForeColor, oldBackColor;\
	PenState		oldState;\
	GetForeColor(&oldForeColor);\
	GetBackColor(&oldBackColor);\
	GetPenState(&oldState);\
	ForeColor(blackColor);\
	BackColor(whiteColor);
#endif

#ifndef RESTORECOLORS
#define RESTORECOLORS\
	SetPenState(&oldState);\
	RGBForeColor(&oldForeColor);\
	RGBBackColor(&oldBackColor);
#endif