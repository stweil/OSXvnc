/*
 *  localbuffer.c
 *  OSXvnc
 *
 *  Created by Mihai Parparita on Sat Jun 15 2002.
 *  Copyright (c) 2002 Mihai Parparita. All rights reserved.
 *
 */

#include "CGS.h"
#include "localbuffer.h"

#define CGRect2QDRect(rectCG, rectQD) SetRect(rectQD, \
                                              rectCG.origin.x, \
                                              rectCG.origin.y, \
                                              rectCG.origin.x + rectCG.size.width, \
                                              rectCG.origin.y + rectCG.size.height)

GDHandle			gMainDevice;
char				gDeviceState;
PixMapHandle		gDevicePix;
CGSConnectionRef	gConnection;

GWorldPtr			rfbLocalBufferGW = NULL;
PixMapHandle		rfbLocalBufferPix = NULL;

// cursor stuff
int					gCursorSeed = 0;
CGPoint				gCursorHotspot;
CGRect				gCursorRect;	
CGRect				gCurrentMouseRect = {{0, 0}, {0, 0}};
GWorldPtr			gCursorGW = NULL, gCursorMaskGW = NULL;
PixMapHandle		gCursorPix = NULL, gCursorMaskPix = NULL;
CTabHandle			gGrayscaleTable = NULL;

static void rfbLocalBufferDrawCursor();

void StartFrontBufferAccess(void)
{
	gMainDevice = GetMainDevice();
	gDeviceState = HGetState((Handle)gMainDevice);

	HLock((Handle)gMainDevice);
	
	gDevicePix = (**gMainDevice).gdPMap;
}

void FinishFrontBufferAccess(void)
{
	HSetState((Handle)gMainDevice, gDeviceState);
}

CGRect rfbLocalBufferGetMouseRect()
{
	return gCurrentMouseRect;
}

char *rfbFrontBufferBaseAddress(void)
{
	assert(rfbLocalBufferPix);
	
	return (**rfbLocalBufferPix).baseAddr;
}

void rfbLocalBufferInfo(int* width, int* height, int* bpp, int *depth, int* rowBytes)
{
	assert(rfbLocalBufferPix);
	
	*width = (**rfbLocalBufferPix).bounds.right - (**rfbLocalBufferPix).bounds.left;
	*height = (**rfbLocalBufferPix).bounds.bottom - (**rfbLocalBufferPix).bounds.top;

	*bpp = *depth = (**rfbLocalBufferPix).pixelSize;
	
	*rowBytes = GetPixRowBytes(rfbLocalBufferPix);
}

void rfbLocalBufferSyncAll(void)
{
	SAVECOLORS;
	
	StartFrontBufferAccess();
	
	CopyBits((BitMap*)*gDevicePix, (BitMap*)*rfbLocalBufferPix,
			&(**gDevicePix).bounds, &(**gDevicePix).bounds,
			srcCopy, NULL);
	
	FinishFrontBufferAccess();
	
	RESTORECOLORS;
}

void rfbLocalBufferSync(int rectCount, const CGRect *rectArray)
{
	Rect			copyRect;
	int				i;
	
	SAVECOLORS;
	
	StartFrontBufferAccess();
	
	for (i=0; i < rectCount; i++)
	{
		CGRect2QDRect(rectArray[i], &copyRect);
		CopyBits((BitMap*)*gDevicePix, (BitMap*)*rfbLocalBufferPix,
				  &copyRect, &copyRect,
				  srcCopy, NULL);
	}
	
	CGRect2QDRect(gCurrentMouseRect, &copyRect);
	CopyBits((BitMap*)*gDevicePix, (BitMap*)*rfbLocalBufferPix,
				&copyRect, &copyRect,
				srcCopy, NULL);
	
	rfbLocalBufferDrawCursor();
	
	FinishFrontBufferAccess();
	
	RESTORECOLORS;
}

void rfbLocalBufferDrawCursor()
{
	Rect	targetRect, sourceRect;
	CGPoint	location;
	
	SAVECOLORS;
	
	if (CGSCurrentCursorSeed() != gCursorSeed) // need to regrab the cursor data
	{
		int					cursorDataSize, depth, componentCount, bitsPerComponent, cursorRowBytes, dataX, dataY;
		unsigned char*		cursorData;
		Rect				cursorRect;
		
		assert(CGSGetGlobalCursorDataSize(gConnection, &cursorDataSize) == kCGErrorSuccess);
		
		cursorData = (unsigned char*)malloc(sizeof(unsigned char) * cursorDataSize);
		assert(cursorData);
		
		assert(CGSGetGlobalCursorData(gConnection,
                                cursorData,
                                &cursorDataSize,
                                &cursorRowBytes,
                                &gCursorRect,
                                &gCursorHotspot,
                                &depth,
                                &componentCount,
                                &bitsPerComponent) == kCGErrorSuccess);
		
		CGRect2QDRect(gCursorRect, &cursorRect);
		
		if (gCursorPix) UnlockPixels(gCursorPix);
		if (gCursorGW) DisposeGWorld(gCursorGW);
		NewGWorld(&gCursorGW, depth, &cursorRect, NULL, NULL, 0);
		gCursorPix = GetGWorldPixMap(gCursorGW);
		LockPixels(gCursorPix);
		
		if (gCursorMaskPix) UnlockPixels(gCursorMaskPix);
		if (gCursorMaskGW) DisposeGWorld(gCursorMaskGW);
		NewGWorld(&gCursorMaskGW, 8, &cursorRect, gGrayscaleTable, NULL, 0);
		gCursorMaskPix = GetGWorldPixMap(gCursorMaskGW);
		LockPixels(gCursorMaskPix);
		
		for (dataY = 0; dataY < gCursorRect.size.height; dataY++)
			memcpy(&(**gCursorPix).baseAddr[dataY * GetPixRowBytes(gCursorPix)],
					&cursorData[dataY * cursorRowBytes],
					cursorRowBytes);
				
		for (dataX = 0; dataX < gCursorRect.size.width; dataX++)
			for (dataY = 0; dataY < gCursorRect.size.height; dataY++)
				(**gCursorMaskPix).baseAddr[dataY * GetPixRowBytes(gCursorMaskPix) + dataX] =
					cursorData[dataY * cursorRowBytes + dataX * depth/8];
		
		gCursorSeed = CGSCurrentCursorSeed();
		
		free(cursorData);
	}
	
	CGSGetCurrentCursorLocation(gConnection, &location);
	
	CGRect2QDRect(gCursorRect, &sourceRect);
	gCurrentMouseRect = CGRectOffset(gCursorRect, location.x - gCursorHotspot.x, location.y - gCursorHotspot.y);
	CGRect2QDRect(gCurrentMouseRect, &targetRect);
	
	StartFrontBufferAccess();
	
	if ((**gDevicePix).pixelSize == 8) // should use CGSHardwareCursorActive here, but it's flaky
		CopyBits((BitMap*)*gDevicePix, (BitMap*)*rfbLocalBufferPix,
				 &targetRect, &targetRect,
				 srcCopy, NULL);
	else
		CopyDeepMask((BitMap*)*gCursorPix, (BitMap*)*gCursorMaskPix, (BitMap*)*rfbLocalBufferPix,
					&sourceRect, &sourceRect, &targetRect,
					srcCopy, NULL);
	FinishFrontBufferAccess();
	
	RESTORECOLORS;
}

void rfbLocalBufferInit()
{
	SAVECOLORS;
	
	StartFrontBufferAccess();
	
	NewGWorld(&rfbLocalBufferGW, (**gDevicePix).pixelSize, &(**gDevicePix).bounds, (**gDevicePix).pmTable, NULL, 0);
	rfbLocalBufferPix = GetGWorldPixMap(rfbLocalBufferGW);
	LockPixels(rfbLocalBufferPix);
	
	assert(CGSNewConnection(NULL, &gConnection) == kCGErrorSuccess);
	
	FinishFrontBufferAccess();
	
	gGrayscaleTable = GetCTable(40);
	
	RESTORECOLORS;
}

void rfbLocalBufferShutdown()
{
	assert(CGSReleaseConnection(gConnection) == kCGErrorSuccess);
	
	if (rfbLocalBufferPix) UnlockPixels(rfbLocalBufferPix); rfbLocalBufferPix = NULL;
	if (rfbLocalBufferGW) DisposeGWorld(rfbLocalBufferGW); rfbLocalBufferGW = NULL;
	if (gGrayscaleTable) DisposeCTable(gGrayscaleTable); gGrayscaleTable = NULL;
}