/*
 *  OSXvnc Copyright (C) 2002-2004 Redstone Software osxvnc@redstonesoftware.com
 *  Original Xvnc code Copyright (C) 1999 AT&T Laboratories Cambridge.
 *  All Rights Reserved.
 *
 *  This is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This software is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this software; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307,
 *  USA.
 */

#include <ApplicationServices/ApplicationServices.h>
#include <Cocoa/Cocoa.h>
#include <Carbon/Carbon.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <fcntl.h>
#include <pthread.h>
#include <unistd.h>
#include <signal.h>

#include <sys/sysctl.h>

#include "rfb.h"

#include "rfbserver.h"
#import "VNCServer.h"

static ScreenRec hackScreen;
rfbScreenInfo rfbScreen;

unsigned rfbProtocolMajorVersion = 3;
unsigned rfbProtocolMinorVersion = 8;

char desktopName[256];

BOOL keepRunning = TRUE;

BOOL littleEndian = FALSE;
unsigned rfbPort = 0; //5900;
int  rfbMaxBitDepth = 0;
Bool rfbAlwaysShared = FALSE;
Bool rfbNeverShared = FALSE;
Bool rfbDontDisconnect = FALSE;
Bool rfbLocalhostOnly = FALSE;
Bool rfbInhibitEvents = FALSE;
Bool rfbReverseMods = FALSE;

Bool rfbSwapButtons = TRUE;
Bool rfbDisableRemote = FALSE;
Bool rfbDisableRichClipboards = FALSE;
Bool rfbRemapShortcuts = FALSE;
BOOL rfbShouldSendUpdates = TRUE;
BOOL registered = FALSE;
BOOL restartOnUserSwitch = FALSE;
BOOL useIP4 = TRUE;
BOOL unregisterWhenNoConnections = FALSE;
BOOL nonBlocking = FALSE;
BOOL logEnable = TRUE;

static BOOL didSupplyPass;
static CGFloat displayScale;

// OSXvnc 0.8 This flag will use a local buffer which will allow us to display the mouse cursor
// Bool rfbLocalBuffer = FALSE;

static pthread_mutex_t logMutex;
pthread_mutex_t listenerAccepting;
pthread_cond_t listenerGotNewClient;
pthread_t listener_thread;

/* OSXvnc 0.8 for screensaver .... */
// setup screen saver disabling timer
// Not sure we want or need this...
static EventLoopTimerUPP  screensaverTimerUPP;
static EventLoopTimerRef screensaverTimer;
Bool rfbDisableScreenSaver = FALSE;

// Display ID of main display
static CGDirectDisplayID displayID;

// Deprecated in 10.6 but required since the new api doesn't work for off-screen sessions
size_t CGDisplayBytesPerRow ( CGDirectDisplayID display );
void * CGDisplayBaseAddress ( CGDirectDisplayID display );

extern void rfbScreensaverTimer(EventLoopTimerRef timer, void *userData);

int rfbDeferUpdateTime = 40; /* in ms */

static char reverseHost[256] = "";
static int reversePort = 5500;

CGDisplayErr displayErr;

// Server Data
rfbserver thisServer;

VNCServer *vncServerObject = nil;

static bool rfbScreenInit(void);

/*
 * rfbLog prints a time-stamped message to the log file (stderr).
 */

void rfbLog(char *format, ...) {
    if (logEnable && format != NULL) {
        va_list args;
        NSString *nsFormat = [[NSString alloc] initWithUTF8String:format];
        pthread_mutex_lock(&logMutex);
        NS_DURING {
            va_start(args, format);
            NSLogv(nsFormat, args);
            va_end(args);
        };
        NS_HANDLER
        NS_ENDHANDLER
        pthread_mutex_unlock(&logMutex);
        [nsFormat release];
    }
}

void rfbDebugLog(char *format, ...) {
#ifdef __DEBUGGING__
    va_list args;
    NSString *nsFormat = [[NSString alloc] initWithUTF8String:format];

    pthread_mutex_lock(&logMutex);
    va_start(args, format);
    NSLogv(nsFormat, args);
    va_end(args);

    [nsFormat release];
    pthread_mutex_unlock(&logMutex);
#endif
}


void rfbLogPerror(char *str) {
    rfbLog("%s: %s", str, strerror(errno));
}

// Some calls fail under older OS X'es so we will do some detected loading
void loadDynamicBundles(BOOL startup) {
    NSAutoreleasePool *startPool = [[NSAutoreleasePool alloc] init];

    // Setup thisServer structure
    thisServer.vncServer = vncServerObject;
    thisServer.desktopName = desktopName;
    thisServer.rfbPort = rfbPort;
    thisServer.rfbLocalhostOnly = rfbLocalhostOnly;
    thisServer.listenerAccepting = listenerAccepting;
    thisServer.listenerGotNewClient = listenerGotNewClient;

    [[VNCServer sharedServer] rfbStartup: &thisServer];

    [startPool release];
}

void refreshCallback(CGRectCount count, const CGRect *rectArray, void *ignore) {
    BoxRec box;
    RegionRec region;
    rfbClientIteratorPtr iterator;
    rfbClientPtr cl;
    int i;

    for (i = 0; i < count; i++) {
        box.x1 = rectArray[i].origin.x;
        box.y1 = rectArray[i].origin.y;
        box.x2 = box.x1 + rectArray[i].size.width;
        box.y2 = box.y1 + rectArray[i].size.height;

        SAFE_REGION_INIT(&hackScreen, &region, &box, 0);

        iterator = rfbGetClientIterator();
        while ((cl = rfbClientIteratorNext(iterator)) != NULL) {
            pthread_mutex_lock(&cl->updateMutex);
            REGION_UNION(&hackScreen,&cl->modifiedRegion,&cl->modifiedRegion,&region);
            pthread_mutex_unlock(&cl->updateMutex);
            pthread_cond_signal(&cl->updateCond);
        }
        rfbReleaseClientIterator(iterator);

        REGION_UNINIT(&hackScreen, &region);
    }
}

//CGError screenUpdateMoveCallback(CGScreenUpdateMoveDelta delta, CGRectCount count, const CGRect * rectArray, void * userParameter) {
//    //NSLog(@"Moved Callback");
//    return 0;
//}

static int bitsPerPixelForDisplay(CGDirectDisplayID dispID) {
    int bitsPerPixel = 0;
    CGDisplayModeRef mode = CGDisplayCopyDisplayMode(dispID);
    CFStringRef pixelEncoding = CGDisplayModeCopyPixelEncoding(mode);

    if (!pixelEncoding) {
        // When off-screen the BPP is not accessible -- 32 is default and works.
        bitsPerPixel = 32;
    } else if (CFStringCompare(pixelEncoding, CFSTR(IO32BitDirectPixels),
                               kCFCompareCaseInsensitive) == kCFCompareEqualTo) {
        bitsPerPixel = 32;
    } else if (CFStringCompare(pixelEncoding, CFSTR(IO16BitDirectPixels),
                               kCFCompareCaseInsensitive) == kCFCompareEqualTo) {
        bitsPerPixel = 16;
    } else if (CFStringCompare(pixelEncoding, CFSTR(IO8BitIndexedPixels),
                               kCFCompareCaseInsensitive) == kCFCompareEqualTo) {
        bitsPerPixel = 8;
    }
    [(id)pixelEncoding release];
    CGDisplayModeRelease(mode);
    return bitsPerPixel;
}

static CGFloat scalingFactor()
{
    CGFloat scale = 1.0;
    NSScreen *myScreen = [NSScreen mainScreen];
    if ([myScreen respondsToSelector:@selector(backingScaleFactor)]) {
        scale = myScreen.backingScaleFactor;
    }
    return scale;
}

void rfbCheckForScreenResolutionChange() {
    BOOL sizeChange = (rfbScreen.width != CGDisplayPixelsWide(displayID) ||
                       rfbScreen.height != CGDisplayPixelsHigh(displayID));
    BOOL colorChange = (bitsPerPixelForDisplay(displayID) > 0 && rfbScreen.bitsPerPixel != bitsPerPixelForDisplay(displayID));

    // See if screen changed
    if (sizeChange || colorChange) {
        rfbClientIteratorPtr iterator;
        rfbClientPtr cl = NULL;
        BOOL screenOK = TRUE;
        int maxTries = 12;

        // Block listener from accepting new connections while we restart
        pthread_mutex_lock(&listenerAccepting);

        iterator = rfbGetClientIterator();
        // Disconnect Existing Clients
        while ((cl = rfbClientIteratorNext(iterator))) {
            pthread_mutex_lock(&cl->updateMutex);
            // Keep locked until after screen change
        }
        rfbReleaseClientIterator(iterator);

        do {
            screenOK = rfbScreenInit();
        } while (!screenOK && maxTries-- && usleep(2000000)==0);
        if (!screenOK)
            exit(1);

        rfbLog("Screen geometry changed - (%d,%d) depth: %d",
               CGDisplayPixelsWide(displayID),
               CGDisplayPixelsHigh(displayID),
               bitsPerPixelForDisplay(displayID));


        iterator = rfbGetClientIterator();
        while ((cl = rfbClientIteratorNext(iterator))) {
            // Only need to notify them on a SIZE change - other changes just make us re-init
            if (sizeChange) {
                if (cl->desktopSizeUpdate) {
                    BoxRec box;
                    rfbFramebufferUpdateMsg *fu = (rfbFramebufferUpdateMsg *)cl->updateBuf;
                    fu->type = rfbFramebufferUpdate;
                    fu->nRects = Swap16IfLE(1);
                    cl->ublen = sz_rfbFramebufferUpdateMsg;

                    rfbSendScreenUpdateEncoding(cl);

                    cl->screenBuffer = rfbGetFramebuffer();
                    if (cl->scalingFactor == 1) {
                        cl->scalingFrameBuffer = cl->screenBuffer;
                        cl->scalingPaddedWidthInBytes = rfbScreen.paddedWidthInBytes;
                    }
                    else {
                        const unsigned long csh = (rfbScreen.height+cl->scalingFactor-1)/ cl->scalingFactor;
                        const unsigned long csw = (rfbScreen.width +cl->scalingFactor-1)/ cl->scalingFactor;

                        // Reset Frame Buffer
                        free(cl->scalingFrameBuffer);
                        cl->scalingFrameBuffer = malloc( csw*csh*rfbScreen.bitsPerPixel/8 );
                        cl->scalingPaddedWidthInBytes = csw * rfbScreen.bitsPerPixel/8;
                    }

                    box.x1 = box.y1 = 0;
                    box.x2 = rfbScreen.width;
                    box.y2 = rfbScreen.height;
                    REGION_INIT(pScreen,&cl->modifiedRegion,&box,0);
                    //cl->needNewScreenSize = TRUE;
                }
                else
                    rfbCloseClient(cl);
            }
            else {
                // In theory we shouldn't need to disconnect them but some state in the cl record seems to cause a problem
                rfbCloseClient(cl);
                rfbSetTranslateFunction(cl);
            }

            sleep(2); // We may detect the new depth before OS X has quite finished getting everything ready for it.
            pthread_mutex_unlock(&cl->updateMutex);
            pthread_cond_signal(&cl->updateCond);
        }
        rfbReleaseClientIterator(iterator);

        // Accept new connections again
        pthread_mutex_unlock(&listenerAccepting);
    }
}

static void *clientOutput(void *data) {
    rfbClientPtr cl = (rfbClientPtr)data;
    RegionRec updateRegion;
    Bool haveUpdate = false;

    while (1) {
        haveUpdate = false;

        pthread_mutex_lock(&cl->updateMutex);
        while (!haveUpdate) {
            if (cl->sock == -1) {
                /* Client has disconnected. */
                pthread_mutex_unlock(&cl->updateMutex);
                return NULL;
            }

            // Check for (and send immediately) pending PB changes
            rfbClientUpdatePasteboard(cl);

            // Only do checks if we HAVE an outstanding request
            if (REGION_NOTEMPTY(&hackScreen, &cl->requestedRegion)) {
                /* REDSTONE */
                if (rfbDeferUpdateTime > 0 && !cl->immediateUpdate) {
                    // Compare Request with Update Area
                    REGION_INIT(&hackScreen, &updateRegion, NullBox, 0);
                    REGION_INTERSECT(&hackScreen, &updateRegion, &cl->modifiedRegion, &cl->requestedRegion);
                    haveUpdate = REGION_NOTEMPTY(&hackScreen, &updateRegion);

                    REGION_UNINIT(&hackScreen, &updateRegion);
                }
                else {
                    /*  If we've turned off deferred updating
                    We are going to send an update as soon as we have a requested,
                    regardless of if we have a "change" intersection */
                    haveUpdate = TRUE;
                }

                if (rfbShouldSendNewCursor(cl))
                    haveUpdate = TRUE;
                else if (rfbShouldSendNewPosition(cl))
                    // Could Compare with the request area but for now just always send it
                    haveUpdate = TRUE;
                else if (cl->needNewScreenSize)
                    haveUpdate = TRUE;
            }

            if (!haveUpdate)
                pthread_cond_wait(&cl->updateCond, &cl->updateMutex);
        }

        // OK, now, to save bandwidth, wait a little while for more updates to come along.
        /* REDSTONE - Lets send it right away if no rfbDeferUpdateTime */
        if (rfbDeferUpdateTime > 0 && !cl->immediateUpdate && !cl->needNewScreenSize) {
            pthread_mutex_unlock(&cl->updateMutex);
            usleep(rfbDeferUpdateTime * 1000);
            pthread_mutex_lock(&cl->updateMutex);
        }

        /* Now, get the region we're going to update, and remove
            it from cl->modifiedRegion _before_ we send the update.
            That way, if anything that overlaps the region we're sending
            is updated, we'll be sure to do another update later. */
        REGION_INIT(&hackScreen, &updateRegion, NullBox, 0);
        REGION_INTERSECT(&hackScreen, &updateRegion, &cl->modifiedRegion, &cl->requestedRegion);
        REGION_SUBTRACT(&hackScreen, &cl->modifiedRegion, &cl->modifiedRegion, &updateRegion);
        /* REDSTONE - We also want to clear out the requested region, so we don't process
            graphic updates in previously requested regions */
        REGION_UNINIT(&hackScreen, &cl->requestedRegion);
        REGION_INIT(&hackScreen, &cl->requestedRegion,NullBox,0);

        /*  This does happen but it's asynchronous (and slow to occur)
            what we really want to happen is to just temporarily hide the cursor (while sending to the remote screen)
            -- It's not even usually there (as it's handled by the display driver - but under certain occasions it does appear
         displayErr = CGDisplayHideCursor(displayID);
         if (displayErr != 0)
         rfbLog("Error Hiding Cursor %d", displayErr);
         CGDisplayMoveCursorToPoint(displayID, CGPointZero);
                                            */

        /* Now actually send the update. */
        rfbSendFramebufferUpdate(cl, updateRegion);
        /* If we were hiding it before make it reappear now
            displayErr = CGDisplayShowCursor(displayID);
        if (displayErr != 0)
            rfbLog("Error Showing Cursor %d", displayErr);
        */

        REGION_UNINIT(&hackScreen, &updateRegion);
        pthread_mutex_unlock(&cl->updateMutex);
    }

    return NULL;
}

void *clientInput(void *data) {
    rfbClientPtr cl = (rfbClientPtr)data;
    pthread_t output_thread;

    pthread_create(&output_thread, NULL, clientOutput, cl);

    while (1) {
        [[VNCServer sharedServer] rfbReceivedClientMessage];
        rfbProcessClientMessage(cl);

        // Some people will connect but not request screen updates - just send events, this will delay registering the CG callback until then
        if (rfbShouldSendUpdates && REGION_NOTEMPTY(&hackScreen, &cl->requestedRegion)) {
            @synchronized([VNCServer sharedServer]) { // Registering twice sometimes prevents getting notice on 10.6
                if (!registered) {
                    CGError result = CGRegisterScreenRefreshCallback(refreshCallback, NULL);
                    if (result == kCGErrorSuccess) {
                        rfbLog("Client connected - registering screen update notification");
                        [[VNCServer sharedServer] rfbConnect];
                        //CGScreenRegisterMoveCallback(screenUpdateMoveCallback, NULL);
                        registered = TRUE;
                    }
                    else {
                        NSLog(@"Error (%d) registering for Screen Update Notification", result);
                    }
                }
            }
        }

        if (cl->sock == -1) {
            /* Client has disconnected. */
            break;
        }
    }

    /* Get rid of the output thread. */
    //pthread_mutex_lock(&cl->updateMutex);
    pthread_cond_signal(&cl->updateCond);
    //pthread_mutex_unlock(&cl->updateMutex);
    pthread_join(output_thread, NULL);

    rfbClientConnectionGone(cl);

    return NULL;
}

void rfbStartClientWithFD(int client_fd) {
    rfbClientPtr cl;
    pthread_t client_thread;
    int one=1;

    if (!rfbClientsConnected())
        rfbCheckForScreenResolutionChange();

    pthread_mutex_lock(&listenerAccepting);

    if (setsockopt(client_fd, IPPROTO_TCP, TCP_NODELAY, (void *)&one, sizeof(one)) < 0)
        rfbLogPerror("setsockopt TCP_NODELAY failed");

    rfbUndim();
    cl = rfbNewClient(client_fd);

    pthread_create(&client_thread, NULL, clientInput, (void *)cl);

    pthread_mutex_unlock(&listenerAccepting);
    pthread_cond_signal(&listenerGotNewClient);
}

static void *listenerRun(void *ignore) {
    int listen_fd4=0, client_fd=0;
    int value=1;  // Need to pass a ptr to this
    struct sockaddr_in sin4, peer4;
    unsigned int len4=sizeof(sin4);

    // Must register IPv6 first otherwise it seems to clear our unique binding for IPv4 portNum
    [[VNCServer sharedServer] rfbRunning];

    // Ok, we are leaving IPv4 binding on even with IPv6 on so that OSXvnc will bind up the port regardless
    // When both are enabled you can't have another VNC server "steal" the IPv4 port
    if (useIP4) {
        bzero(&sin4, sizeof(sin4));
        sin4.sin_len = sizeof(sin4);
        sin4.sin_family = AF_INET;
        sin4.sin_port = htons(rfbPort);
        if (rfbLocalhostOnly)
            sin4.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        else
            sin4.sin_addr.s_addr = htonl(INADDR_ANY);

        if ((listen_fd4 = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
            rfbLogPerror("Unable to open socket");
        }
        else if (nonBlocking && (fcntl(listen_fd4, F_SETFL, O_NONBLOCK) < 0)) {
            rfbLogPerror("fcntl O_NONBLOCK failed");
        }
        else if (setsockopt(listen_fd4, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(value)) < 0) {
            rfbLogPerror("setsockopt SO_REUSEADDR failed");
        }
        else if (bind(listen_fd4, (struct sockaddr *) &sin4, len4) < 0) {
            rfbLog("Failed to bind socket: port %d maybe in use by another VNC", rfbPort);
        }
        else if (listen(listen_fd4, 5) < 0) {
            rfbLogPerror("Listen failed");
        }
        else {
            rfbLog("Started listener thread on IPv4 port %d", rfbPort);

            // Thread stays here forever unless something goes wrong
            while (keepRunning) {
                client_fd = accept(listen_fd4, (struct sockaddr *) &peer4, &len4);
                if (client_fd != -1)
                    rfbStartClientWithFD(client_fd);
                else {
                    if (errno == EWOULDBLOCK) {
                        usleep(100000);
                    }
                    else {
                        rfbLog("Accept failed %d", errno);
                        exit(1);
                    }
                }
            }

            rfbLog("Listener thread exiting");
            return NULL;
        }

        if (reverseHost[0] != '\0') {
            rfbLog("Listener disabled");
        }
        else {
            exit(250);
        }
    }
    return NULL;
}

void connectReverseClient(char *hostName, int portNum) {
    pthread_t client_thread;
    rfbClientPtr cl;

    pthread_mutex_lock(&listenerAccepting);
    rfbUndim();
    cl = rfbReverseConnection(hostName, portNum);
    if (cl) {
        pthread_create(&client_thread, NULL, clientInput, (void *)cl);
        pthread_mutex_unlock(&listenerAccepting);
        pthread_cond_signal(&listenerGotNewClient);
    }
    else {
        pthread_mutex_unlock(&listenerAccepting);
    }
}

static NSMutableData *frameBufferData;
static size_t frameBufferBytesPerRow;
static size_t frameBufferBitsPerPixel;

char *rfbGetFramebuffer(void) {
    if (floor(NSAppKitVersionNumber) > floor(NSAppKitVersionNumber10_6)) {
        if (!frameBufferData) {
            CGImageRef imageRef;
            if (displayScale > 1.0) {
                // Retina display.
                size_t width = rfbScreen.width;
                size_t height = rfbScreen.height;
                CGImageRef image = CGDisplayCreateImage(displayID);
                CGColorSpaceRef colorspace = CGColorSpaceCreateWithName(kCGColorSpaceGenericRGB);
                CGContextRef context = CGBitmapContextCreate(NULL, width, height,
                                                             CGImageGetBitsPerComponent(image),
                                                             CGImageGetBytesPerRow(image),
                                                             colorspace,
                                                             kCGImageAlphaNoneSkipLast);

                CGColorSpaceRelease(colorspace);
                if (context == NULL) {
                    rfbLog("There was an error getting screen shot");
                    return nil;
                }
                CGContextDrawImage(context, CGRectMake(0, 0, width, height), image);
                imageRef = CGBitmapContextCreateImage(context);
                CGContextRelease(context);
                CGImageRelease(image);
            } else {
                imageRef = CGDisplayCreateImage(displayID);
            }
            CGDataProviderRef dataProvider = CGImageGetDataProvider (imageRef);
            CFDataRef dataRef = CGDataProviderCopyData(dataProvider);
            frameBufferBytesPerRow = CGImageGetBytesPerRow(imageRef);
            frameBufferBitsPerPixel = CGImageGetBitsPerPixel(imageRef);
            frameBufferData = [(NSData *)dataRef mutableCopy];
            CFRelease(dataRef);

            if (imageRef != NULL)
                CGImageRelease(imageRef);
        }
        return frameBufferData.mutableBytes;
    }
    else { // Old API is required for off screen user sessions
        int maxWait =   5000000;
        int retryWait =  500000;

        char *returnValue = (char *)CGDisplayBaseAddress(displayID);
        while (!returnValue && maxWait > 0) {
            NSLog(@"Unable to obtain base address");
            usleep(retryWait); // Buffer goes away while screen is "switching", it'll be back
            maxWait -= retryWait;
            returnValue = (char *)CGDisplayBaseAddress(displayID);
        }
        if (!returnValue) {
            NSLog(@"Unable to obtain base address -- Giving up");
            exit(1);
        }

        return returnValue;
    }
}

// Called to get record updates of the requested region into our framebuffer
void rfbGetFramebufferUpdateInRect(int x, int y, int w, int h) {
    if (frameBufferData) {
        CGRect rect = CGRectMake (x,y,w,h);
        CGImageRef imageRef;
        if (displayScale > 1.0) {
            // Retina display.
            CGImageRef image = CGDisplayCreateImageForRect(displayID, rect);
            CGColorSpaceRef colorspace = CGColorSpaceCreateDeviceRGB();
            CGBitmapInfo bitmapInfo = kCGBitmapByteOrder32Little | kCGImageAlphaNoneSkipFirst;
            CGContextRef context = CGBitmapContextCreate(NULL, w, h, 8, w * 4,colorspace, bitmapInfo);
            CGColorSpaceRelease(colorspace);
            if (context == NULL) {
                rfbLog("There was an error getting scaled images");
                return;
            }
            CGContextDrawImage(context, CGRectMake(0, 0, w, h), image);
            imageRef = CGBitmapContextCreateImage(context);
            CGContextRelease(context);
        } else {
            imageRef = CGDisplayCreateImageForRect(displayID, rect);
        }
        CGDataProviderRef dataProvider = CGImageGetDataProvider (imageRef);
        CFDataRef dataRef = CGDataProviderCopyData(dataProvider);
        size_t imgBytesPerRow = CGImageGetBytesPerRow(imageRef);
        size_t imgBitsPerPixel = CGImageGetBitsPerPixel(imageRef);
        if (imgBitsPerPixel != frameBufferBitsPerPixel)
            NSLog(@"BitsPerPixel MISMATCH: frameBuffer %zu, rect image %zu", frameBufferBitsPerPixel, imgBitsPerPixel);

        char *dest = (char *)frameBufferData.mutableBytes + frameBufferBytesPerRow * y + x * (frameBufferBitsPerPixel/8);
        const char *source = ((NSData *)dataRef).bytes;

        while (h--) {
            memcpy(dest, source, w*(imgBitsPerPixel/8));
            dest += frameBufferBytesPerRow;
            source += imgBytesPerRow;
        }

        if (imageRef != NULL)
            CGImageRelease(imageRef);
        [(id)dataRef release];
    }
}

static bool rfbScreenInit(void) {
    /* Note: As of 10.7 there doesn't appear to be an easy way to get the bitsPerSample or samplesPerPixel of the screen buffer. It looks like that information may be in the bitsPerComponent and componentCount elements of the IOPixelInformation structure. But we'd have to get into poorly-documented IOKit functions to get it. It seems very unlikely that it will be anything other than 8 bits and 3 samples, and in any case we're not really prepared to handle anything else, so the best we could do is die gracefully. Now we are likely to die ungracefully (or maybe just produce garbage) if the screen buffer is in a different format.
     */
    int bitsPerSample = 8; // Let's presume 8 bits x 3 samples and hope for the best.....
    int samplesPerPixel = 3;

    [frameBufferData release]; // release previous screen buffer, if any
    frameBufferData = nil;

    if (displayID == 0) {
        // The display was not selected up to now, so choose the main display.
        displayID = CGMainDisplayID();
    }

    if (samplesPerPixel != 3) {
        rfbLog("screen format not supported");
        return FALSE;
    }

    displayScale = scalingFactor();
    if (displayScale > 1.0) {
        // Retina display.
        rfbLog("Detected HiDPI Display with scaling factor of %f", displayScale);
    }

    rfbScreen.width = CGDisplayPixelsWide(displayID);
    rfbScreen.height = CGDisplayPixelsHigh(displayID);
    rfbScreen.bitsPerPixel = bitsPerPixelForDisplay(displayID);
    rfbScreen.depth = samplesPerPixel * bitsPerSample;
    //Fix for Yosemite and above
    if (floor(NSAppKitVersionNumber) > floor(NSAppKitVersionNumber10_6)) {
        CGImageRef imageRef;
        // Check to see if retina display.
        if (displayScale > 1.0) {
            size_t width = rfbScreen.width;
            size_t height = rfbScreen.height;
            CGImageRef image = CGDisplayCreateImage(displayID);
            CGColorSpaceRef colorspace = CGColorSpaceCreateWithName(kCGColorSpaceGenericRGB);
            CGContextRef context = CGBitmapContextCreate(NULL, width, height,
                                                         CGImageGetBitsPerComponent(image),
                                                         CGImageGetBytesPerRow(image),
                                                         colorspace,
                                                         kCGImageAlphaNoneSkipLast);

            CGColorSpaceRelease(colorspace);
            if (context == NULL) {
                rfbLog("There was an error getting screen shot");
                return nil;
            }
            CGContextDrawImage(context, CGRectMake(0, 0, width, height), image);
            imageRef = CGBitmapContextCreateImage(context);
            CGContextRelease(context);
        } else {
            imageRef = CGDisplayCreateImage(displayID);
        }
        rfbScreen.paddedWidthInBytes = CGImageGetBytesPerRow(imageRef);
        if (imageRef != NULL)
            CGImageRelease(imageRef);
    }
    else {
        rfbScreen.paddedWidthInBytes = CGDisplayBytesPerRow(displayID);
    }
    rfbServerFormat.bitsPerPixel = rfbScreen.bitsPerPixel;
    rfbServerFormat.depth = rfbScreen.depth;
    rfbServerFormat.trueColour = TRUE;

    rfbServerFormat.redMax = (1 << bitsPerSample) - 1;
    rfbServerFormat.greenMax = (1 << bitsPerSample) - 1;
    rfbServerFormat.blueMax = (1 << bitsPerSample) - 1;

    if (littleEndian)
        rfbLog("Running in Little Endian");
    else
        rfbLog("Running in Big Endian");

    rfbServerFormat.bigEndian = !littleEndian;
    rfbServerFormat.redShift = bitsPerSample * 2;
    rfbServerFormat.greenShift = bitsPerSample * 1;
    rfbServerFormat.blueShift = bitsPerSample * 0;

    /* We want to use the X11 REGION_* macros without having an actual
        X11 ScreenPtr, so we do this.  Pretty ugly, but at least it lets us
        avoid hacking up regionstr.h, or changing every call to REGION_* */
    hackScreen.RegionCreate = miRegionCreate;
    hackScreen.RegionInit = miRegionInit;
    hackScreen.RegionCopy = miRegionCopy;
    hackScreen.RegionDestroy = miRegionDestroy;
    hackScreen.RegionUninit = miRegionUninit;
    hackScreen.Intersect = miIntersect;
    hackScreen.Union = miUnion;
    hackScreen.Subtract = miSubtract;
    hackScreen.Inverse = miInverse;
    hackScreen.RegionReset = miRegionReset;
    hackScreen.TranslateRegion = miTranslateRegion;
    hackScreen.RectIn = miRectIn;
    hackScreen.PointInRegion = miPointInRegion;
    hackScreen.RegionNotEmpty = miRegionNotEmpty;
    hackScreen.RegionEmpty = miRegionEmpty;
    hackScreen.RegionExtents = miRegionExtents;
    hackScreen.RegionAppend = miRegionAppend;
    hackScreen.RegionValidate = miRegionValidate;

    return TRUE;
}

static void usage(void) {
    printf(
        "\nAvailable options:\n\n"

        "-rfbport port          TCP port for RFB protocol (0=autodetect first open port 5900-5909)\n"
        "-rfbwait time          Maximum time in ms to wait for RFB client\n"
        "-rfbnoauth             Run the server without password protection\n"
        "-rfbauth passwordFile  Use this password file for VNC authentication\n"
        "                       (use 'storepasswd' to create a password file)\n"
        "-rfbpass               Supply a password directly to the server\n"
        "-maxauthattempts num   Maximum number of auth tries before disabling access from a host\n"
        "                       (default: 5), zero disables\n"
        "-deferupdate time      Time in ms to defer updates (default %d)\n"
        "-desktop name          VNC desktop name (default: host name)\n"
        "-alwaysshared          Always treat new clients as shared\n"
        "-nevershared           Never treat new clients as shared\n"
        "-dontdisconnect        Don't disconnect existing clients when a new non-shared\n"
        "                       connection comes in (refuse new connection instead)\n"
        "-nodimming             Never allow the display to dim\n"
        "                       (default: display can dim, input undims)\n"
        "-maxdepth bits         Maximum allowed bit depth for connecting clients (32, 16, 8).\n"
        "                       (default: bit depth of display)\n"
#if 0
        "-reversemods           reverse the interpretation of control\n"
        "                       and command (for windows clients)\n"
#endif
        "-allowsleep            Allow machine to sleep\n"
        "                       (default: sleep is disabled)\n"
        "-disableScreenSaver    Disable screen saver while users are connected\n"
        "                       (default: no, allow screen saver to engage)\n"
        "-swapButtons           Swap mouse buttons 2 & 3\n"
        "                       (default: yes)\n"
        "-dontswapButtons       Disable swap mouse buttons 2 & 3\n"
        "                       (default: no)\n"
        "-disableRemoteEvents   Ignore remote keyboard, pointer, and clipboard event\n"
        "                       (default: no, process them)\n"
        "-disableRichClipboards Don't share rich clipboard events\n"
        "                       (default: no, process them)\n"
        "-connectHost host      Host name or IP of listening client to establish a reverse connect\n"
        "-connectPort port      TCP port of listening client to establish a reverse connect\n"
        "                       (default: 5500)\n"
        "-noupdates             Prevent registering for screen updates, for use with x2vnc or win2vnc\n"
        "-protocol protocol     Force a particular RFB protocol version (eg 3.3)\n"
        "                       (default: %u.%u)\n"
        "-bigEndian             Force big-endian mode (PPC)\n"
        "                       (default: detect)\n"
        "-littleEndian          Force little-endian mode (INTEL)\n"
        "                       (default: detect)\n"

        "-display DisplayID     displayID to indicate which display to serve\n",
        rfbDeferUpdateTime,
        rfbProtocolMajorVersion, rfbProtocolMinorVersion
    );
    {
        CGDisplayCount displayCount;
        CGDirectDisplayID activeDisplays[100];
        int index;

        CGGetActiveDisplayList(100, activeDisplays, &displayCount);

        for (index = 0; index < displayCount; index++) {
            printf("                       %d = (%zu, %zu)\n",
                   index,
                   CGDisplayPixelsWide(activeDisplays[index]),
                   CGDisplayPixelsHigh(activeDisplays[index]));
        }
    }

    printf(
        "-localhost             Only allow connections from the same machine,\n"
        "                       literally localhost (127.0.0.1)\n"
        "                       If you use SSH and want to stop non-SSH connections from any other hosts\n"
        "                       (default: no, allow remote connections)\n"
        "-restartonuserswitch flag\n"
        "                       For use on Panther 10.3 systems, this will cause the\n"
        "                       server to restart when a fast user switch occurs\n"
        "                       (default: no)\n"
        "-disableLog            Don't log anything in console\n"
    );
    [[VNCServer sharedServer] rfbUsage];
    printf("\n");

    exit(255);
}

static void checkForUsage(int argc, char *argv[]) {
    int i;

    for (i = 1; i < argc; i++) {
        if (strncmp(argv[i], "-h", 2) == 0 ||
            strncmp(argv[i], "-H", 2) == 0 ||
            strncmp(argv[i], "--h", 3) == 0 ||
            strncmp(argv[i], "--H", 3) == 0 ||
            strncmp(argv[i], "-usage", 6) == 0 ||
            strncmp(argv[i], "--usage", 7) == 0 ||
            strcmp(argv[i], "-?") == 0) {
            loadDynamicBundles(FALSE);
            usage();
        }
    }
}

static void processArguments(int argc, char *argv[]) {
    char argString[1024] = "Arguments: ";
    int i, j;

    for (i = 1; i < argc; i++) {
        strcat(argString, argv[i]);
        strcat(argString, " ");
    }
    rfbLog(argString);

    for (i = 1; i < argc; i++) {
        // Lowercase it
        for (j=0;j<strlen(argv[i]);j++)
            argv[i][j] = tolower(argv[i][j]);

        if (strcmp(argv[i], "-rfbport") == 0) { // -rfbport port
            if (i + 1 >= argc) usage();
            rfbPort = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-protocol") == 0) { // -rfbport port
            double protocol;
            if (i + 1 >= argc) usage();
            protocol = atof(argv[++i]);
            rfbProtocolMajorVersion = MIN(rfbProtocolMajorVersion, floor(protocol));
            protocol = protocol-floor(protocol); // Now just the fractional part
                                                 // Ok some folks think of it as 3.3 others as 003.003, so let's repeat...
            while (protocol > 0 && protocol < 1)
                protocol *= 10;
            rfbProtocolMinorVersion = MIN(rfbProtocolMinorVersion, rint(protocol));
            rfbLog("Forcing: %u.%u",
                   rfbProtocolMajorVersion, rfbProtocolMinorVersion);
        } else if (strcmp(argv[i], "-rfbwait") == 0) {  // -rfbwait ms
            if (i + 1 >= argc) usage();
            rfbMaxClientWait = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-rfbnoauth") == 0) {
            allowNoAuth = TRUE;
            rfbLog("Warning: No Auth specified, running with no password protection");
        } else if (strcmp(argv[i], "-rfbauth") == 0) {  // -rfbauth passwd-file
            if (i + 1 >= argc) usage();
            rfbAuthPasswdFile = argv[++i];
        } else if (strcmp(argv[i], "-rfbpass") == 0) {  // -rfbauth passwd-file
            if (i + 1 >= argc) usage();
            if (!enterSuppliedPassword(argv[++i])) {
                rfbLog("ERROR: The supplied password failed to initialize.  Now exiting!!");
                exit (255);
            }else{
                didSupplyPass=TRUE;
            }
        } else if (strcmp(argv[i], "-maxauthattempts") == 0) {
            if (i + 1 >= argc) usage();
            rfbMaxLoginAttempts = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-connecthost") == 0) {  // -connect host
            if (i + 1 >= argc) usage();
            strncpy(reverseHost, argv[++i], 255);
            if (reverseHost[0] == '\0') usage();
        } else if (strcmp(argv[i], "-connectport") == 0) {  // -connect host
            if (i + 1 >= argc) usage();
            reversePort = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-deferupdate") == 0) {  // -deferupdate ms
            if (i + 1 >= argc) usage();
            rfbDeferUpdateTime = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-maxdepth") == 0) {  // -maxdepth
            if (i + 1 >= argc) usage();
            rfbMaxBitDepth = atoi(argv[++i]);
            switch (rfbMaxBitDepth) {
                case 24:
                    rfbMaxBitDepth = 32;
                    break;
                case 32:
                case 16:
                case 8:
                    break;
                default:
                    rfbLog("Invalid maxDepth");
                    exit(-1);
                    break;
            }
        } else if (strcmp(argv[i], "-desktop") == 0) {  // -desktop desktop-name
            if (i + 1 >= argc) usage();
            strncpy(desktopName, argv[++i], 255);
        } else if (strcmp(argv[i], "-display") == 0) {  // -display DisplayID
            CGDisplayCount displayCount;
            CGDirectDisplayID activeDisplays[100];

            CGGetActiveDisplayList(100, activeDisplays, &displayCount);

            if (i + 1 >= argc || atoi(argv[i+1]) >= displayCount)
                usage();

            displayID = activeDisplays[atoi(argv[++i])];
        } else if (strcmp(argv[i], "-alwaysshared") == 0) {
            rfbAlwaysShared = TRUE;
        } else if (strcmp(argv[i], "-nevershared") == 0) {
            rfbNeverShared = TRUE;
        } else if (strcmp(argv[i], "-dontdisconnect") == 0) {
            rfbDontDisconnect = TRUE;
        } else if (strcmp(argv[i], "-nodimming") == 0) {
            rfbNoDimming = TRUE;
        } else if (strcmp(argv[i], "-allowsleep") == 0) {
            rfbNoSleep = FALSE;
        } else if (strcmp(argv[i], "-reversemods") == 0) {
            rfbReverseMods = TRUE;
        } else if (strcmp(argv[i], "-disablescreensaver") == 0) {
            rfbDisableScreenSaver = TRUE;
        } else if (strcmp(argv[i], "-swapbuttons") == 0) {
            rfbSwapButtons = TRUE;
        } else if (strcmp(argv[i], "-dontswapbuttons") == 0) {
            rfbSwapButtons = FALSE;
        } else if (strcmp(argv[i], "-disableremoteevents") == 0) {
            rfbDisableRemote = TRUE;
        } else if (strcmp(argv[i], "-disablerichclipboards") == 0) {
            rfbDisableRichClipboards = TRUE;
        } else if (strcmp(argv[i], "-localhost") == 0) {
            rfbLocalhostOnly = TRUE;
        } else if (strcmp(argv[i], "-inhibitevents") == 0) {
            rfbInhibitEvents = TRUE;
        } else if (strcmp(argv[i], "-noupdates") == 0) {
            rfbShouldSendUpdates = FALSE;
        } else if (strcmp(argv[i], "-littleendian") == 0) {
            littleEndian = TRUE;
        } else if (strcmp(argv[i], "-bigendian") == 0) {
            littleEndian = FALSE;
        } else if (strcmp(argv[i], "-ipv6") == 0) { // Ok so the code to enable is in the Bundle, but this disables 4
            useIP4 = FALSE;
        } else if (strcmp(argv[i], "-keepregistration") == 0) {
            unregisterWhenNoConnections = FALSE;
        } else if (strcmp(argv[i], "-dontkeepregistration") == 0) {
            unregisterWhenNoConnections = TRUE;
        } else if (strcmp(argv[i], "-restartonuserswitch") == 0) {
            if (i + 1 >= argc)
                usage();
            else {
                char *argument = argv[++i];
                restartOnUserSwitch = (argument[0] == 'y' || argument[0] == 'Y' || argument[0] == 't' || argument[0] == 'T' || atoi(argument));
            }
        } else if (strcmp(argv[i], "-disablelog") == 0) {
            logEnable = FALSE;
        } else if (strcmp(argv[i], "-useopengl") == 0) {
            rfbLog("OpenGL no longer supported");
        }
    }

    if (!rfbAuthPasswdFile && !allowNoAuth && reverseHost[0] == '\0' && !didSupplyPass) {
        rfbLog("ERROR: No authentication specified, use -rfbauth passwordfile OR -rfbnoauth");
        exit (255);
    }
}

void rfbShutdown(void) {
    [[VNCServer sharedServer] rfbShutdown];

    CGUnregisterScreenRefreshCallback(refreshCallback, NULL);
    //CGDisplayShowCursor(displayID);
    rfbDimmingShutdown();

    rfbDebugLog("Removing Observers");
    [[NSWorkspace sharedWorkspace].notificationCenter removeObserver: vncServerObject];
    [[NSNotificationCenter defaultCenter] removeObserver:vncServerObject];
    [[NSDistributedNotificationCenter defaultCenter] removeObserver:vncServerObject];

    if (rfbDisableScreenSaver) {
        /* remove the screensaver timer */
        RemoveEventLoopTimer(screensaverTimer);
        DisposeEventLoopTimerUPP(screensaverTimerUPP);
    }

    if (nonBlocking) {
        keepRunning = NO;
        pthread_join(listener_thread,NULL);
    }

    rfbDebugLog("RFB shutdown complete");
}

static void executeEventLoop (int signal) {
    pthread_cond_signal(&listenerGotNewClient);
}

static void rfbShutdownOnSignal(int signal) {
    rfbLog("OSXvnc-server received signal: %d", signal);
    rfbShutdown();

    if (signal == SIGTERM)
        exit (0);
    else
        exit (signal);
}

void daemonize( void ) {
    int i;

    // Fork New Process
    if ( fork() != 0 )
        exit( 0 );

    // Become session leader
    setsid();

    // Ignore signals here
    signal(SIGHUP, SIG_IGN);
    signal(SIGPIPE, SIG_IGN);
    // Shutdown on these
    signal(SIGTERM, rfbShutdownOnSignal);
    signal(SIGINT, rfbShutdownOnSignal);
    signal(SIGQUIT, rfbShutdownOnSignal);

    // Fork a second new process
    if ( fork( ) != 0 )
        exit( 0 );

    // chdir ( "/" );
    umask( 0 );

    // Close open FDs
    for ( i = getdtablesize( ) - 1; i > STDERR_FILENO; i-- )
        close( i );

    /* from this point on we should only send output to server log or syslog */
}

int scanForOpenPort() {
    int tryPort = 5900;
    int listen_fd4=0;
    int value=1;
    struct sockaddr_in sin4;

    bzero(&sin4, sizeof(sin4));
    sin4.sin_len = sizeof(sin4);
    sin4.sin_family = AF_INET;

//    if ([[NSUserDefaults standardUserDefaults] boolForKey:@"localhostOnly"])
//        sin4.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
//    else
    sin4.sin_addr.s_addr = htonl(INADDR_ANY);

    while (tryPort < 5910) {
        sin4.sin_port = htons(tryPort);

        if ((listen_fd4 = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
            //NSLog(@"Socket init failed %d", tryPort);
        }
        else if (fcntl(listen_fd4, F_SETFL, O_NONBLOCK) < 0) {
            //rfbLogPerror("fcntl O_NONBLOCK failed");
        }
        else if (setsockopt(listen_fd4, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(value)) < 0) {
            //NSLog(@"setsockopt SO_REUSEADDR failed %d", tryPort);
        }
        else if (bind(listen_fd4, (struct sockaddr *) &sin4, sizeof(sin4)) < 0) {
            //NSLog(@"Failed to bind socket: port %d may be in use by another VNC", tryPort);
        }
        else if (listen(listen_fd4, 5) < 0) {
            //NSLog(@"Listen failed %d", tryPort);
        }
        else {
            close(listen_fd4);

            return tryPort;
        }
        close(listen_fd4);

        tryPort++;
    }

    rfbLog("Unable to find open port 5900-5909");

    return 0;
}

BOOL runningLittleEndian ( void ) {
    return (CFByteOrderGetCurrent() == CFByteOrderLittleEndian);
    /*
     // rosetta is so complete that it obsucres even CFByteOrderGetCurrent
    int hasMMX = 0;
    size_t length = sizeof(hasMMX);
     // No Error and it does have MMX
     return (!sysctlbyname("hw.optional.mmx", &hasMMX, &length, NULL, 0) && hasMMX);
     */
}

int main(int argc, char *argv[]) {
    NSAutoreleasePool *tempPool = [[NSAutoreleasePool alloc] init];
    vncServerObject = [[VNCServer alloc] init];
    littleEndian = runningLittleEndian();
    checkForUsage(argc,argv);

    // The bug with unregistering from user updates may have been fixed in 10.4 Tiger
    if (floor(NSAppKitVersionNumber) > floor(NSAppKitVersionNumber10_3))
        unregisterWhenNoConnections = TRUE;

    // This guarantees separating us from any terminal -
    // Right now this causes problems with the keep-alive script and the GUI app (since it causes the process to return right away)
    // it allows you to survive when launched in SSH, etc but doesn't solves the problem of being killed on GUI logout.
    // It also doesn't help with any of the pasteboard security issues, those requires secure sessionID's, see:
    // http://developer.apple.com/documentation/MacOSX/Conceptual/BPMultipleUsers/index.html
    //
    // daemonize();

    // Let's not shutdown on a SIGHUP at some point perhaps we can use that to reload configuration
    signal(SIGHUP, SIG_IGN);
    signal(SIGPIPE, SIG_IGN);
    signal(SIGCONT, executeEventLoop);
    signal(SIGTERM, rfbShutdownOnSignal);
    signal(SIGINT, rfbShutdownOnSignal);
    signal(SIGQUIT, rfbShutdownOnSignal);

    pthread_mutex_init(&logMutex, NULL);
    pthread_mutex_init(&listenerAccepting, NULL);
    pthread_cond_init(&listenerGotNewClient, NULL);

    [[NSUserDefaults standardUserDefaults] addSuiteNamed:@"com.redstonesoftware.VineServer"];

    processArguments(argc, argv);

    if (rfbPort == 0)
        rfbPort = scanForOpenPort();

    loadDynamicBundles(TRUE);

    // If no desktop name is provided try to get it.
    if (desktopName[0] == '\0') {
        gethostname(desktopName, 256);
    }

    if (!rfbScreenInit())
        exit(1);

    rfbClientListInit();
    rfbDimmingInit();
    rfbAuthInit();
    initPasteboard();

    // Register for User Switch Notification
    // This works on pre-Panther systems since the notification just won't get called.
    if (restartOnUserSwitch) {
        [[NSWorkspace sharedWorkspace].notificationCenter addObserver:vncServerObject
                                                               selector:@selector(userSwitched:)
                                                                   name: NSWorkspaceSessionDidBecomeActiveNotification
                                                                 object:nil];
        [[NSWorkspace sharedWorkspace].notificationCenter addObserver:vncServerObject
                                                               selector:@selector(userSwitched:)
                                                                   name: NSWorkspaceSessionDidResignActiveNotification
                                                                 object:nil];
    }

    {
        // Setup Notifications so other Bundles can post user connect
        [[NSNotificationCenter defaultCenter] addObserver:vncServerObject
                                                 selector:@selector(clientConnected:)
                                                     name:@"NewRFBClient"
                                                   object:nil];

        // Setup Notifications so we can add listening hosts
        [[NSDistributedNotificationCenter defaultCenter] addObserver:vncServerObject
                                                            selector:@selector(connectHost:)
                                                                name:@"VNCConnectHost"
                                                              object:[NSString stringWithFormat:@"OSXvnc%d", rfbPort]];
    }

    // Does this need to be in 10.1 and greater (does any of this stuff work in 10.0?)
    if (!rfbInhibitEvents) {
        //NSLog(@"Core Graphics - Event Suppression Turned Off");
        // This seems to actually sometimes inhibit REMOTE events as well, but all the same let's let everything pass through for now
        //        CGSetLocalEventsFilterDuringSupressionState(kCGEventFilterMaskPermitAllEvents, kCGEventSupressionStateSupressionInterval);
        //        CGSetLocalEventsFilterDuringSupressionState(kCGEventFilterMaskPermitAllEvents, kCGEventSupressionStateRemoteMouseDrag);
    }
    // Better to handle this at the event level, see kbdptr.c
    //CGEnableEventStateCombining(FALSE);

    if (rfbDisableScreenSaver || rfbNoSleep) {
        /* setup screen saver disabling timer */
        screensaverTimerUPP = NewEventLoopTimerUPP(rfbScreensaverTimer);
        InstallEventLoopTimer(GetMainEventLoop(),
                              kEventDurationSecond * 30,
                              kEventDurationSecond * 30,
                              screensaverTimerUPP,
                              NULL,
                              &screensaverTimer);
    }

    nonBlocking = [[NSUserDefaults standardUserDefaults] boolForKey:@"NonBlocking"];
    pthread_create(&listener_thread, NULL, listenerRun, NULL);

    if (reverseHost[0] != '\0')
        connectReverseClient(reverseHost, reversePort);

    // This segment is what is responsible for causing the server to shutdown when a user logs out
    // The problem being that OS X sends it first a SIGTERM and then a SIGKILL (un-trappable)
    // Presumable because it's running a Carbon Event loop
    if (1) {
        OSStatus resultCode = 0;

        while (keepRunning) {
            // No Clients - go into hibernation
            if (!rfbClientsConnected()) {
                pthread_mutex_lock(&listenerAccepting);

                // You would think that there is no point in getting screen updates with no clients connected
                // But it seems that unregistering but keeping the process (or event loop) around can cause a stuttering behavior in OS X.
                if (registered && unregisterWhenNoConnections) {
                    rfbLog("UnRegistering screen update notification - waiting for clients");
                    CGUnregisterScreenRefreshCallback(refreshCallback, NULL);
                    [[VNCServer sharedServer] rfbDisconnect];
                    registered = NO;
                }
                else
                    rfbLog("Waiting for clients");

                pthread_cond_wait(&listenerGotNewClient, &listenerAccepting);
                pthread_mutex_unlock(&listenerAccepting);
            }
            [[VNCServer sharedServer] rfbPoll];

            rfbCheckForPasteboardChange();
            rfbCheckForCursorChange();
            rfbCheckForScreenResolutionChange();
            // Run The Event loop a moment to see if we have a screen update or NSNotification
            // No better luck with RunApplicationEventLoop() avoiding the shutdown on logout problem
            resultCode = RunCurrentEventLoop(kEventDurationSecond/30); //EventTimeout
            if (resultCode != eventLoopTimedOutErr) {
                rfbLog("Received Result: %d during event loop, Shutting Down", resultCode);
                keepRunning = NO;
            }
        }
    }
#if 0
    else while (1) {
        // So this looks like it should fix it but I get no response on the CGWaitForScreenRefreshRect....
        // It doesn't seem to get called at all when not running an event loop
        CGRectCount rectCount;
        CGRect *rectArray;
        CGEventErr result;

        result = CGWaitForScreenRefreshRects( &rectArray, &rectCount );
        refreshCallback(rectCount, rectArray, NULL);
        CGReleaseScreenRefreshRects( rectArray );
    }
#endif

    [tempPool release];

    rfbShutdown();

    return 0;
}
