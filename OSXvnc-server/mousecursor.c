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
//#import "rfbproto.h"
//#include "localbuffer.h"
#include "pthread.h"

#include "CGS.h"

static int lastCursorSeed = 0;
static CGPoint lastCursorPosition;

void GetCursorInfo() {
    CGError				err;
    CGSConnectionRef	connection;
    int cursorDataSize, depth, components, bitsPerComponent, cursorRowSize;
    unsigned char*		cursorData;
    CGPoint				location, hotspot;
    CGRect				cursorRect;
    int i, j;


    //printf("get active connection returns: %d, %d, %d\n", CGSGetActiveConnection(&temp, &connection), temp, connection);
    printf("new connection (err %d) = %d\n", CGSNewConnection(NULL, &connection), connection);

    err = CGSGetCurrentCursorLocation(connection, &location);
    printf("location (err %d) = %d, %d\n", err, (int)location.x, (int)location.y);

    err = CGSGetGlobalCursorDataSize(connection, &cursorDataSize);
    printf("data size (err %d) = %d\n", err, cursorDataSize);

    cursorData = (unsigned char*)calloc(cursorDataSize, sizeof(unsigned char));

    err = CGSGetGlobalCursorData(connection,
                                 cursorData,
                                 &cursorDataSize,
                                 &cursorRowSize,
                                 &cursorRect,
                                 &hotspot,
                                 &depth,
                                 &components,
                                 &bitsPerComponent);

    printf("rect origin (%g, %g), dimensions (%g, %g)\n", cursorRect.origin.x, cursorRect.origin.y, cursorRect.size.width, cursorRect.size.height);

    printf("hotspot (%g, %g)\n", hotspot.x, hotspot.y);

    printf("depth: %d\n", depth);

    printf("components: %d\n", components);

    printf("bits per component: %d\n", bitsPerComponent);

    printf("Bytes Per Row: %d\n", cursorRowSize);


    printf("Components (err %d):\n", err);

    // Print Colors
    for (j=0; j < components; j++) {
        printf("\n");
        for (i=0; i < cursorDataSize; i++) {
            if (i % cursorRowSize == 0)
                printf("\n");
            if (i % components == j)
                printf("%02x", (int)cursorData[i]);
        }
    }

    printf("released connection (err %d)\n", CGSReleaseConnection(connection));
}

// We call this to see if we have a new cursor and should notify clients to do an update
// Or if cursor has moved
void rfbCheckForCursorChange() {
    CGSConnectionRef connection;
    CGPoint cursorLoc;

    if (CGSNewConnection(NULL, &connection) != kCGErrorSuccess ||
        CGSGetCurrentCursorLocation(connection, &cursorLoc) != kCGErrorSuccess) {
        rfbLog("Error obtaining cursor data - cursor position not sent\n");
        return;
    }
    CGSReleaseConnection(connection);

    //rfbLog("Check For Cursor Change");
    // First Let's see if we have new info on the pasteboard - if so we'll send an update to each client
    if (lastCursorSeed != CGSCurrentCursorSeed() || !CGPointEqualToPoint(lastCursorPosition, cursorLoc)) {
        rfbClientIteratorPtr iterator = rfbGetClientIterator();
        rfbClientPtr cl;

        // Record first in case another change occurs after notifying clients
        lastCursorSeed = CGSCurrentCursorSeed();
        lastCursorPosition = cursorLoc;

        // Notify each client
        while ((cl = rfbClientIteratorNext(iterator)) != NULL) {
            if (rfbShouldSendNewCursor(cl) || (rfbShouldSendNewPosition(cl)))
                pthread_cond_signal(&cl->updateCond);
        }
        rfbReleaseClientIterator(iterator);
    }
}

Bool rfbShouldSendNewCursor(rfbClientPtr cl) {
    if (!cl->useRichCursorEncoding)
        return FALSE;
    else
        return (cl->currentCursorSeed != CGSCurrentCursorSeed());
}

Bool rfbShouldSendNewPosition(rfbClientPtr cl) {
    if (!cl->enableCursorPosUpdates)
        return FALSE;
    else {
        CGSConnectionRef connection;
        CGPoint cursorLoc;

        if (CGSNewConnection(NULL, &connection) != kCGErrorSuccess ||
            CGSGetCurrentCursorLocation(connection, &cursorLoc) != kCGErrorSuccess) {
            rfbLog("Error obtaining cursor data - cursor position not sent\n");
            return FALSE;
        }
        CGSReleaseConnection(connection);

        return (!CGPointEqualToPoint(cl->clientCursorLocation,cursorLoc));
    }
}
/* Still To Do

Problems with occasional artifacts - turning off the cursor didn't seem to help
Problems in formats besides 32bit (local)
Problem with Alpha channel

*/

Bool rfbSendRichCursorUpdate(rfbClientPtr cl) {
    rfbFramebufferUpdateRectHeader rect;
    rfbPixelFormat cursorFormat;
    char *cursorData;
    int bufferMaskOffset;
    int cursorSize; // Size of cursor data from size
    int cursorRowBytes;
    int cursorDataSize; // Size to be sent to client
    int cursorMaskSize; // Mask Size to be sent to client
    int cursorDepth;
    int cursorBitsPerComponent;
    BOOL cursorIsDifferentFormat = FALSE;

    CGError err;
    CGSConnectionRef connection;
    int components; // Cursor Components

    CGPoint hotspot;
    CGRect cursorRect;

    //rfbLog("Sending Cursor To Client");
    //GetCursorInfo();

    if (CGSNewConnection(NULL, &connection) != kCGErrorSuccess ||
        CGSGetGlobalCursorDataSize(connection, &cursorDataSize) != kCGErrorSuccess) {
        rfbLog("Error obtaining cursor data - cursor not sent\n");
        return FALSE;
    }

    // For This We Don't send location just the cursor shape (and Hot Spot)

    cursorData = (unsigned char*)malloc(sizeof(unsigned char) * cursorDataSize);
    err = CGSGetGlobalCursorData(connection,
                                 cursorData,
                                 &cursorDataSize,
                                 &cursorRowBytes,
                                 &cursorRect,
                                 &hotspot,
                                 &cursorDepth,
                                 &components,
                                 &cursorBitsPerComponent);

    CGSReleaseConnection(connection);

    if (err != kCGErrorSuccess) {
        rfbLog("Error obtaining cursor data - cursor not sent\n");
        return FALSE;
    }

    cursorFormat.depth = cursorDepth;
    cursorFormat.bitsPerPixel = cursorDepth;
    cursorFormat.bigEndian = TRUE;
    cursorFormat.trueColour = TRUE;
    cursorFormat.redMax = cursorFormat.greenMax = cursorFormat.blueMax = (unsigned short) ((1<<cursorBitsPerComponent) - 1);
    cursorFormat.redShift = (unsigned char) cursorBitsPerComponent * 2;
    cursorFormat.greenShift = (unsigned char) cursorBitsPerComponent;
    cursorFormat.blueShift = 0;
    //GetCursorInfo();
    //PrintPixelFormat(&cursorFormat);
    cursorIsDifferentFormat = !(PF_EQ(cursorFormat,rfbServerFormat));
    
    cursorSize = (cursorRect.size.width * cursorRect.size.height * (cl->format.bitsPerPixel / 8));
    cursorMaskSize = floor((cursorRect.size.width+7)/8) * cursorRect.size.height;

    // Make Sure we have space on the buffer (otherwise push the data out now)

    if (cl->ublen + sz_rfbFramebufferUpdateRectHeader + cursorSize + cursorMaskSize > UPDATE_BUF_SIZE) {
        if (!rfbSendUpdateBuf(cl))
            return FALSE;
    }

    // Send The Header
    rect.r.x = Swap16IfLE((short) hotspot.x);
    rect.r.y = Swap16IfLE((short) hotspot.y);
    rect.r.w = Swap16IfLE((short) cursorRect.size.width);
    rect.r.h = Swap16IfLE((short) cursorRect.size.height);
    rect.encoding = Swap32IfLE(rfbEncodingRichCursor);

    memcpy(&cl->updateBuf[cl->ublen], (char *)&rect,sz_rfbFramebufferUpdateRectHeader);
    cl->ublen += sz_rfbFramebufferUpdateRectHeader;

    // Apple Cursors can use a full Alpha channel.
    // Since we can only send a bit mask - to get closer we will compose the full color with a white
    // This requires us to jump ahead to write in the update buffer
    bufferMaskOffset = cl->ublen + cursorSize;

    // For starters we'll set it all off
    memset(&cl->updateBuf[bufferMaskOffset], 0, cursorMaskSize);
    // This algorithm assumes the Alpha channel is the first component
    {
        unsigned char *cursorRowData = cursorData;
        unsigned char *cursorColumnData = cursorData;
        unsigned int cursorBytesPerPixel = (cursorDepth/8);
        unsigned int alphaShift = 8 - cursorBitsPerComponent;
        unsigned char mask = 0;
        unsigned char fullOn = 0x00FF >> alphaShift;
        unsigned char alphaThreshold = 0x60 >> alphaShift; // Only include the pixel if it's coverage is greater than this
        int dataX, dataY, componentIndex;

        for (dataY = 0; dataY < cursorRect.size.height; dataY++) {
            cursorColumnData = cursorRowData;
            for (dataX = 0; dataX < cursorRect.size.width; dataX++) {
                mask = (unsigned char)(*cursorColumnData) >> alphaShift;
                if (mask > alphaThreshold) {
                    // Write the Bit For The Mask to be ON
                    cl->updateBuf[bufferMaskOffset+(dataX/8)] |= 0x0080 >> (dataX % 8);
                    // Composite Alpha into real cursors other channels - only for 32 bit
                    if (cursorDepth == 32 && mask != fullOn) {
                        // Set Alpha Pixel
                        *cursorColumnData = (unsigned char) 0x00;
                        cursorColumnData++;
                        for (componentIndex = 1; componentIndex < components; componentIndex++) {
                            *cursorColumnData = (unsigned char) (fullOn - mask + ((*cursorColumnData * mask)/fullOn)) & 0xFF;
                            cursorColumnData++;
                        }
                    }
                    else
                        cursorColumnData += cursorBytesPerPixel;
                }
                else
                    cursorColumnData += cursorBytesPerPixel;
            }

            bufferMaskOffset += floor((cursorRect.size.width+7)/8);
            cursorRowData += cursorRowBytes;
        }
    }

    // Temporarily set it to the cursor format
    if (cursorIsDifferentFormat)
        rfbSetTranslateFunctionUsingFormat(cl, cursorFormat);
    
    // Now Send The Cursor
    (*cl->translateFn)(cl->translateLookupTable, // The Lookup Table
                       &cursorFormat, // Our Cursor format
                       &cl->format, // Client Format
                       cursorData, // Data we're sending
                       &cl->updateBuf[cl->ublen], // where to write it
                       cursorRowBytes, // bytesBetweenInputLines
                       cursorRect.size.width,
                       cursorRect.size.height);
    cl->ublen += cursorSize;

    if (cursorIsDifferentFormat)
        rfbSetTranslateFunctionUsingFormat(cl, rfbServerFormat);
    
    // Now Send The Cursor Bitmap (1 for on, 0 for clear)
    // We already wrote in the bitmap, see above, just increment the ublen
    cl->ublen += cursorMaskSize;

    // Update Stats
    cl->rfbRectanglesSent[rfbStatsRichCursor]++;
    cl->rfbBytesSent[rfbStatsRichCursor] += sz_rfbFramebufferUpdateRectHeader + cursorSize + cursorMaskSize;
    cl->currentCursorSeed = CGSCurrentCursorSeed();

    return TRUE;
}

Bool rfbSendCursorPos(rfbClientPtr cl) {
    rfbFramebufferUpdateRectHeader rect;
    CGSConnectionRef connection;

    if (cl->ublen + sz_rfbFramebufferUpdateRectHeader > UPDATE_BUF_SIZE) {
        if (!rfbSendUpdateBuf(cl))
            return FALSE;
    }

    if (CGSNewConnection(NULL, &connection) != kCGErrorSuccess ||
        CGSGetCurrentCursorLocation(connection, &cl->clientCursorLocation) != kCGErrorSuccess) {
        rfbLog("Error obtaining cursor data - cursor position not sent\n");
        return FALSE;
    }
    CGSReleaseConnection(connection);

    rect.encoding = Swap32IfLE(rfbEncodingPointerPos);
    rect.r.x = Swap16IfLE((CARD16)cl->clientCursorLocation.x);
    rect.r.y = Swap16IfLE((CARD16)cl->clientCursorLocation.y);
    rect.r.w = 0;
    rect.r.h = 0;

    memcpy(&cl->updateBuf[cl->ublen], (char *)&rect, sz_rfbFramebufferUpdateRectHeader);
    cl->ublen += sz_rfbFramebufferUpdateRectHeader;

    cl->rfbRectanglesSent[rfbStatsCursorPosition]++;
    cl->rfbBytesSent[rfbStatsCursorPosition] += sz_rfbFramebufferUpdateRectHeader;

    if (!rfbSendUpdateBuf(cl))
        return FALSE;

    return TRUE;
}

/*

 void rfbRefreshMouse(int x, int y) {
#pragma unused (x, y)
     if (rfbLocalBuffer) {
         refreshCallback(0, NULL, NULL);
     }

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

     refreshCallback(rectCount, updateRects, NULL);

 }*/


