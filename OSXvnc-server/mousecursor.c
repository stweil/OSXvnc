/*
 *  untitled.c
 *  OSXvnc
 *
 *  Created by Jonathan Gillaspie on Wed Nov 20 2002.
 *  Copyright (c) 2002 __MyCompanyName__. All rights reserved.
 *
 */

#include <ApplicationServices/ApplicationServices.h>
#include <Carbon/Carbon.h>

#include <netinet/in.h>
#include <unistd.h>

#include "rfb.h"
#include "localbuffer.h"

extern void refreshCallback(CGRectCount count, const CGRect *rectArray, void *ignore);

void rfbRefreshMouse(int x, int y)
{
#pragma unused (x, y)
    if (rfbLocalBuffer) {
	if (currentlyRefreshing)
		rfbLog("didn't send mouse update becase we were refreshing already.\n");
	else
            refreshCallback(0, NULL, NULL);
    }

    /*
	int					rectCount = 0;
	CGRect				updateRects[2];
	CGSConnectionRef	connection;
	int					cursorDataSize, depth, componentCount, bitsPerComponent, unknown;
	CGPoint				hotspot;
	CGRect				cursorRect;
	
	if (!CGRectEqualToRect(gCurrentMouseRect, CGRectZero))
		updateRects[rectCount++] = gCurrentMouseRect;
	
	if (CGSNewConnection(NULL, &connection) == kCGErrorSuccess &&
		 CGSGetGlobalCursorDataSize(connection, &cursorDataSize) == kCGErrorSuccess)
	{
		if (cursorDataSize != gLastCursorDataSize || gLastCursorDataSize == 0)
		{
			if (gCursorData)
				free(gCursorData);
			gCursorData = (unsigned char*)malloc(sizeof(unsigned char) * cursorDataSize);
			gLastCursorDataSize = cursorDataSize;
		}
		
		if (CGSGetGlobalCursorData(connection, gCursorData, &cursorDataSize, &unknown,
								   &cursorRect, &hotspot, &depth, &componentCount, &bitsPerComponent) == kCGErrorSuccess)
		{
			gCurrentMouseRect.origin.x = x - hotspot.x;
			gCurrentMouseRect.origin.y = y - hotspot.y;
			gCurrentMouseRect.size.width = cursorRect.size.width;
			gCurrentMouseRect.size.height = cursorRect.size.height;
			
			updateRects[rectCount++] = gCurrentMouseRect;
			
			if (rfbScreen.bitsPerPixel != 8) // there's probably a better way to see if hardware cursor are enabled
			{
				GWorldPtr		saveGW, cursorGW, maskGW;
				PixMapHandle	savePix, cursorPix, maskPix, devicePix;
				Rect			saveGWRect, targetRect;
				GDHandle		mainDevice;
				char			deviceState;
				int				dataX, dataY, oldDefer;
				
				SAVECOLORS;
				
				SetRect(&saveGWRect, 0, 0, cursorRect.size.width, cursorRect.size.height);
				targetRect = saveGWRect;
				OffsetRect(&targetRect, x - hotspot.x, y - hotspot.y);
				
				NewGWorld(&saveGW, 32, &saveGWRect, NULL, NULL, 0);
				savePix = GetGWorldPixMap(saveGW);
				LockPixels(savePix);
				
				NewGWorld(&cursorGW, 32, &saveGWRect, NULL, NULL, 0);
				cursorPix = GetGWorldPixMap(cursorGW);
				LockPixels(cursorPix);
				
				NewGWorld(&maskGW, 8, &saveGWRect, GetCTable(40), NULL, 0);
				maskPix = GetGWorldPixMap(maskGW);
				LockPixels(maskPix);
				
				mainDevice = GetMainDevice();
				deviceState = HGetState((Handle)mainDevice);
		
				HLock((Handle)mainDevice);
				
				devicePix = (**mainDevice).gdPMap;
				
				CopyBits((BitMap*)*devicePix, (BitMap*)*savePix,
						  &targetRect, &saveGWRect,
						  srcCopy, NULL);
				
				for (dataY = 0; dataY < saveGWRect.bottom; dataY++)
					memcpy(&(**cursorPix).baseAddr[dataY * GetPixRowBytes(cursorPix)],
						   &gCursorData[dataY * unknown],
						   unknown);
				
				for (dataX = 0; dataX < saveGWRect.right; dataX++)
					for (dataY = 0; dataY < saveGWRect.bottom; dataY++)
						(**maskPix).baseAddr[dataY * GetPixRowBytes(maskPix) + dataX] =
							gCursorData[dataY * unknown + dataX * depth/8];
							
				CopyDeepMask((BitMap*)*cursorPix, (BitMap*)*maskPix, (BitMap*)*devicePix,
							 &saveGWRect, &saveGWRect, &targetRect,
							 srcCopy, NULL);
							 
				//CopyBits((BitMap*)*cursorPix, (BitMap*)*devicePix, &saveGWRect, &targetRect, srcCopy, NULL);
				
				updateCount = rfbClientCount();
				
				oldDefer = rfbDeferUpdateTime;
				rfbDeferUpdateTime = 0;
														 			 
				refreshCallback(rectCount, updateRects, NULL);

				rfbDeferUpdateTime = oldDefer;
				
				while (updateCount) {;} // spin lock, inefficient
				
				CopyBits((BitMap*)*savePix, (BitMap*)*devicePix,
						 &saveGWRect, &targetRect,
						 srcCopy, NULL);
				
				RESTORECOLORS;
				
				HSetState((Handle)mainDevice, deviceState);
				
				UnlockPixels(maskPix);
				DisposeGWorld(maskGW);
				
				UnlockPixels(cursorPix);
				DisposeGWorld(cursorGW);
				
				UnlockPixels(savePix);
				DisposeGWorld(saveGW);
				
				return;
			}
		}
	}
	
	refreshCallback(rectCount, updateRects, NULL);*/
}

/*static void GetCursorInfo()
{
    CGError				err;
    CGSConnectionRef	connection;
    int					i, cursorDataSize, depth, components, bitsPerComponent;
    unsigned char*		cursorData;
    CGPoint				location, hotspot;
    CGRect				cursorRect;

    //printf("get active connection returns: %d, %d, %d\n", CGSGetActiveConnection(&temp, &connection), temp, connection);
    printf("new connection (err %d) = %d\n", CGSNewConnection(NULL, &connection), connection);

    err = CGSGetCurrentCursorLocation(connection, &location);
    printf("location (err %d) = %d, %d\n", err, (int)location.x, (int)location.y);

    err = CGSGetGlobalCursorDataSize(connection, &cursorDataSize);
    printf("data size (err %d) = %d\n", err, cursorDataSize);

    cursorData = (unsigned char*)calloc(cursorDataSize * 10, sizeof(unsigned char));

    err = CGSGetGlobalCursorData(connection,
                                 cursorData,
                                 &cursorDataSize,
                                 &cursorData[1024],
                                 &cursorRect,
                                 &hotspot,
                                 &depth,
                                 &components,
                                 &bitsPerComponent);
    printf("data (err %d):\n", err);

    for (i=0; i < cursorDataSize; i++)
    {
        if (i % 64 == 0)
            printf("\n");
        if (cursorData[i])
            printf("%02x", (int)cursorData[i]);
        else
            printf("  ");
    }

    printf("\n\n");

    for (i=0; i < 128; i++)
        printf("%02x", cursorData[1024 + i]);
    printf("\n");

    printf("rect origin (%g, %g), dimensions (%g, %g)\n", cursorRect.origin.x, cursorRect.origin.y, cursorRect.size.width, cursorRect.size.height);

    printf("hotspot (%g, %g)\n", hotspot.x, hotspot.y);

    printf("depth: %d\n", depth);

    printf("components: %d\n", components);

    printf("bits per component: %d\n", bitsPerComponent);

    printf("released connection (err %d)\n", CGSReleaseConnection(connection));
} */
