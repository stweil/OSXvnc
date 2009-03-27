/*
 *  mousecursor.c
 *  OSXvnc
 *
 *  Created by Jonathan Gillaspie on Wed Nov 20 2002.
 *  Copyright (c) 2002 Redstone Software. All rights reserved.
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
static int maxFailsRemaining = 8;
static CGPoint lastCursorPosition;

// Cursor Info
static unsigned char *cursorData=NULL;
static unsigned char *cursorMaskData=NULL;
static int cursorRowBytes;
static int cursorDataSize; // Size to be sent to client
static int cursorMaskSize; // Mask Size to be sent to client
static int cursorDepth;
static int cursorBitsPerComponent;
static CGPoint hotspot;
static CGRect cursorRect;
static int components; // Cursor Components
static rfbPixelFormat cursorFormat;

static pthread_mutex_t cursorMutex;


// We are only going to access cursor data from the main thread now
static CGSConnectionRef sharedConnection = 0;

inline CGSConnectionRef getConnection() {
    if (!sharedConnection && maxFailsRemaining > 0) {
		CGError result = CGSNewConnection(NULL, &sharedConnection);
        if (result != kCGErrorSuccess)
            rfbLog("Error obtaining CGSConnection (%u)%s\n", result, (--maxFailsRemaining ? "" : " -- giving up"));
		else {
			maxFailsRemaining = 8;
			pthread_mutex_init(&cursorMutex, NULL);
		}
    }
	
	return sharedConnection;
}

CGPoint currentCursorLoc() {
    CGPoint cursorLoc={0.0, 0.0};
	CGSConnectionRef connection = getConnection();

    if (connection) {
		if (CGSGetCurrentCursorLocation(connection, &cursorLoc) != kCGErrorSuccess)
			rfbLog("Error obtaining cursor location\n");
    }
	
    return cursorLoc;
}

void loadCurrentCursorData() {
    CGError err;
    CGSConnectionRef connection = getConnection();
	
	if (!connection) {
		if (!maxFailsRemaining)
			return;
	}
	
    if (CGSGetGlobalCursorDataSize(connection, &cursorDataSize) != kCGErrorSuccess) {
        rfbLog("Error obtaining cursor data - cursor not sent\n");
        return;
    }
	
	if (cursorData)
		free(cursorData);
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
	
    //CGSReleaseConnection(connection);
    if (err != kCGErrorSuccess) {
		free(cursorData);
		cursorData = NULL;
        rfbLog("Error obtaining cursor data - cursor not sent\n");
        return;
    }
	
    cursorFormat.depth = (cursorDepth == 32 ? 24 : cursorDepth);
    cursorFormat.bitsPerPixel = cursorDepth;
    cursorFormat.bigEndian = TRUE;
    cursorFormat.trueColour = TRUE;
    cursorFormat.redMax = cursorFormat.greenMax = cursorFormat.blueMax = (unsigned short) ((1<<cursorBitsPerComponent) - 1);
	cursorFormat.bigEndian = !littleEndian;
	cursorFormat.redShift   = (unsigned char) (cursorBitsPerComponent * 2);
	cursorFormat.greenShift = (unsigned char) (cursorBitsPerComponent * 1);
	cursorFormat.blueShift  = (unsigned char) (cursorBitsPerComponent * 0);
	    
    cursorMaskSize = floor((cursorRect.size.width+7)/8) * cursorRect.size.height;

	if (cursorMaskData)
		free(cursorMaskData);
	cursorMaskData = (unsigned char*)malloc(sizeof(unsigned char) * cursorMaskSize);
	
	// Apple Cursors can use a full Alpha channel.
    // Since we can only send a bit mask - to get closer we will compose the full color with a white
	
    // For starters we'll set mask to OFF (transparent) everywhere)
    memset(cursorMaskData, 0, cursorMaskSize);
    // This algorithm assumes the Alpha channel is the first component
    {
		unsigned char *maskPointer = cursorMaskData;
        unsigned char *cursorRowData = cursorData;
        unsigned char *cursorColumnData = cursorData;
        unsigned int cursorBytesPerPixel = (cursorDepth/8);
        unsigned char mask = 0;
		unsigned int alphaShift = (8 - cursorBitsPerComponent);
        unsigned char fullOn = (0xFF) >> alphaShift;
        unsigned char alphaThreshold = (0x60) >> alphaShift; // Only include the pixel if it's coverage is greater than this
        int dataX, dataY, componentIndex;
		
        for (dataY = 0; dataY < cursorRect.size.height; dataY++) {
            cursorColumnData = cursorRowData;
            for (dataX = 0; dataX < cursorRect.size.width; dataX++) {
				if (littleEndian)
					mask = (unsigned char)(*(cursorColumnData+(cursorBytesPerPixel-1))) >> alphaShift;
				else
					mask = (unsigned char)(*cursorColumnData) >> alphaShift;
                if (mask > alphaThreshold) {
                    // Write the Bit For The Mask to be ON (opaque)
					maskPointer[(dataX/8)] |= (0x0080 >> (dataX % 8));
                    // Composite Alpha into the cursors other channels - only for 32 bit
                    if (cursorDepth == 32 && mask != fullOn) {
                        for (componentIndex = 0; componentIndex < components; componentIndex++) {
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
			
            maskPointer += (int) floor(((int)cursorRect.size.width+7)/8);
            cursorRowData += cursorRowBytes;
        }
    }
}

// Just for logging
void GetCursorInfo() {
	CGSConnectionRef connection = getConnection();
    CGError err = noErr;
    int cursorDataSize, depth, components, bitsPerComponent, cursorRowSize;
    unsigned char*		cursorData;
    CGPoint				location, hotspot;
    CGRect				cursorRect;
    int i, j;

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
	Bool sendNotice = FALSE;
    CGPoint cursorLoc = currentCursorLoc();
	int currentSeed = CGSCurrentCursorSeed();
	
	pthread_mutex_lock(&cursorMutex);
	if (!CGPointEqualToPoint(lastCursorPosition, cursorLoc)) {
		lastCursorPosition = cursorLoc;
		sendNotice = TRUE;
	}
	if (lastCursorSeed != currentSeed) {
        // Record first in case another change occurs after notifying clients
        lastCursorSeed = currentSeed;
		loadCurrentCursorData();	
		sendNotice = TRUE;
	}
	pthread_mutex_unlock(&cursorMutex);
	
    //rfbLog("Check For Cursor Change");
    if (sendNotice) {
        rfbClientIteratorPtr iterator = rfbGetClientIterator();
        rfbClientPtr cl;
		
        // Notify each client
        while ((cl = rfbClientIteratorNext(iterator)) != NULL) {
            if (rfbShouldSendNewCursor(cl) || (rfbShouldSendNewPosition(cl)))
                pthread_cond_signal(&cl->updateCond);
        }
        rfbReleaseClientIterator(iterator);
    }
}

#pragma mark -
#pragma mark VNCClientOutput

Bool rfbShouldSendNewCursor(rfbClientPtr cl) {
    if (!cl->useRichCursorEncoding)
        return FALSE;
    else
        return (cl->currentCursorSeed != lastCursorSeed);
}

Bool rfbShouldSendNewPosition(rfbClientPtr cl) {
    if (!cl->enableCursorPosUpdates)
        return FALSE;
    else {
        return (!CGPointEqualToPoint(cl->clientCursorLocation,lastCursorPosition));
    }
}

Bool rfbSendCursorPos(rfbClientPtr cl) {
    rfbFramebufferUpdateRectHeader rect;

    if (cl->ublen + sz_rfbFramebufferUpdateRectHeader > UPDATE_BUF_SIZE) {
        if (!rfbSendUpdateBuf(cl))
            return FALSE;
    }

    cl->clientCursorLocation = lastCursorPosition;

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

/* Still To Do

Problems with occasional artifacts - turning off the cursor didn't seem to help
    Perhaps if we resend the area where the cursor just was..

// QDGetCursorData
*/

Bool rfbSendRichCursorUpdate(rfbClientPtr cl) {
    BOOL cursorIsDifferentFormat = TRUE;
    BOOL returnValue = TRUE;
	int cursorSize = 0;

	pthread_mutex_lock(&cursorMutex);

	cursorIsDifferentFormat = !(PF_EQ(cursorFormat,rfbServerFormat));
	cursorSize = (cursorRect.size.width * cursorRect.size.height * (cl->format.bitsPerPixel / 8));
	
	if (!cursorData || cursorRect.size.height > 128 || cursorRect.size.width > 128) {
		// Wow That's one big cursor! We don't handle cursors this big 
		// (they are probably cursors with lots of states and that doesn't work so good for VNC.
		// For now just ignore them
		cl->currentCursorSeed = lastCursorSeed;
		returnValue = FALSE;
	}
	    
    // Make Sure we have space on the buffer (otherwise push the data out now)

    if (returnValue && 
		cl->ublen + sz_rfbFramebufferUpdateRectHeader + cursorSize + cursorMaskSize > UPDATE_BUF_SIZE) {
        if (!rfbSendUpdateBuf(cl))
            returnValue = FALSE;
    }

	if (returnValue) {
		rfbFramebufferUpdateRectHeader rect;

		// Send The Header
		rect.r.x = Swap16IfLE((short) hotspot.x);
		rect.r.y = Swap16IfLE((short) hotspot.y);
		rect.r.w = Swap16IfLE((short) cursorRect.size.width);
		rect.r.h = Swap16IfLE((short) cursorRect.size.height);
		rect.encoding = Swap32IfLE(rfbEncodingRichCursor);
		
		memcpy(&cl->updateBuf[cl->ublen], (char *)&rect,sz_rfbFramebufferUpdateRectHeader);
		cl->ublen += sz_rfbFramebufferUpdateRectHeader;
		
		// Temporarily set it to the cursor format
		if (cursorIsDifferentFormat)
			rfbSetTranslateFunctionUsingFormat(cl, cursorFormat);
		
		// Now Send The Cursor
		(*cl->translateFn)(cl->translateLookupTable, // The Lookup Table
						   &cursorFormat, // Our Cursor format
						   &cl->format, // Client Format
						   (char *)cursorData, // Data we're sending
						   &cl->updateBuf[cl->ublen], // where to write it
						   cursorRowBytes, // bytesBetweenInputLines
						   cursorRect.size.width,
						   cursorRect.size.height);
		cl->ublen += cursorSize;
		
		if (cursorIsDifferentFormat)
			rfbSetTranslateFunctionUsingFormat(cl, rfbServerFormat);
		
		// Now Send The Cursor Bitmap (1 for on, 0 for clear)
		memcpy(&cl->updateBuf[cl->ublen], cursorMaskData, cursorMaskSize);
		cl->ublen += cursorMaskSize;
		
		// Update Stats
		cl->rfbRectanglesSent[rfbStatsRichCursor]++;
		cl->rfbBytesSent[rfbStatsRichCursor] += sz_rfbFramebufferUpdateRectHeader + cursorSize + cursorMaskSize;
		cl->currentCursorSeed = lastCursorSeed;
	}
	
	pthread_mutex_unlock(&cursorMutex);

	return returnValue;
}
