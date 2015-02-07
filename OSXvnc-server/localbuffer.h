/*
 *  localbuffer.h
 *  OSXvnc
 *
 *  Created by Mihai Parparita on Sun Jun 16 2002.
 *  Copyright (c) 2002 Mihai Parparita. All rights reserved.
 *
 */

#pragma once

#include <Carbon/Carbon.h>

extern void rfbLocalBufferInit();
extern void rfbLocalBufferShutdown();

extern CGRect rfbLocalBufferGetMouseRect();
extern void rfbLocalBufferSync(int rectCount, const CGRect *rectArray);
extern void rfbLocalBufferSyncAll(void);

extern void rfbLocalBufferInfo(int* width, int* height, int* bpp, int *depth, int* rowBytes);
extern char *rfbFrontBufferBaseAddress(void);