/*
 *  OSXvnc Copyright (C) 2001 Dan McGuirk <mcguirk@incompleteness.net>.
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

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <unistd.h>
#include <signal.h>

#include "rfb.h"
#include "localbuffer.h"

ScreenRec hackScreen;
rfbScreenInfo rfbScreen;

char *desktopName = "MacOS X";
char rfbThisHost[255];

static int rfbPort = 5900;
int  rfbMaxBitDepth = 0;
Bool rfbAlwaysShared = FALSE;
Bool rfbNeverShared = FALSE;
Bool rfbDontDisconnect = FALSE;
Bool rfbLocalhostOnly = FALSE;
Bool rfbInhibitEvents = FALSE;
Bool rfbReverseMods = FALSE;

Bool rfbSwapButtons = FALSE;
Bool rfbDisableRemote = FALSE;
Bool rfbRemapShortcuts = FALSE;

// OSXvnc 0.8 This flag will use a local buffer which will allow us to display the mouse cursor
Bool rfbLocalBuffer = FALSE;
static pthread_mutex_t logMutex;

static pthread_mutex_t listenerAccepting;
static pthread_cond_t listenerGotNewClient;

// OSXvnc 0.8 - Local Buffer (for mouse cursor)
Bool currentlyRefreshing = FALSE;

/* OSXvnc 0.8 for screensaver .... */
// setup screen saver disabling timer
// Not sure we want or need this...
static EventLoopTimerUPP  screensaverTimerUPP;
static EventLoopTimerRef screensaverTimer;
Bool rfbDisableScreenSaver = FALSE;

extern void rfbScreensaverTimer(EventLoopTimerRef timer, void *userData);

int rfbDeferUpdateTime = 40; /* ms */

static void rfbScreenInit(void);

/*
 * rfbLog prints a time-stamped message to the log file (stderr).
 */

@interface RestartServer : NSApplication

- screenChanged: (NSNotification *) aNotificaiton;

@end

void
rfbLog(char *format, ...)
{
    va_list args;
    char buf[256];
    time_t clock;

    pthread_mutex_lock(&logMutex);
    va_start(args, format);

    time(&clock);
    strftime(buf, 255, "%d/%m/%Y %T ", localtime(&clock));
    fprintf(stderr, buf);

    vfprintf(stderr, format, args);
    fflush(stderr);

    va_end(args);
    pthread_mutex_unlock(&logMutex);
}

void rfbLogPerror(char *str)
{
    rfbLog("%s: %s\n", str, strerror(errno));
}

void 
refreshCallback(CGRectCount count, const CGRect *rectArray, void *ignore)
{
    BoxRec box;
    RegionRec region;
    rfbClientIteratorPtr iterator;
    rfbClientPtr cl = NULL;                                                          
    int i;

    if (rfbLocalBuffer && rfbClientsConnected()) {
        CGRect*		newRectArray = NULL;

        newRectArray = (CGRect*)malloc(sizeof(CGRect) * (count + 2));
        assert(newRectArray);

        if (count)
            memcpy(newRectArray, rectArray, sizeof(CGRect) * count);

        newRectArray[count] = rfbLocalBufferGetMouseRect();

        rfbLocalBufferSync(count, rectArray);

        newRectArray[count + 1] = rfbLocalBufferGetMouseRect();

        count += 2;
        rectArray = newRectArray;

        free(newRectArray);
    }

    currentlyRefreshing = TRUE;
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
            pthread_cond_signal(&cl->updateCond);
            pthread_mutex_unlock(&cl->updateMutex);
        }
        rfbReleaseClientIterator(iterator);

        REGION_UNINIT(&hackScreen, &region);
    }
    currentlyRefreshing = FALSE;

    // See if screen changed
    if (rfbScreen.width != CGDisplayPixelsWide(kCGDirectMainDisplay) ||
        rfbScreen.height != CGDisplayPixelsHigh(kCGDirectMainDisplay) ||
        rfbScreen.depth != CGDisplayBitsPerPixel(kCGDirectMainDisplay)) {

        // Block listener from accepting new connections while we restart
        pthread_mutex_lock(&listenerAccepting);
        iterator = rfbGetClientIterator();
        // Disconnect Existing Clients
        rfbLog("Screen Geometry Changed - Disconnecting All clients\n");
        while ((cl = rfbClientIteratorNext(iterator))) {            
            pthread_mutex_lock(&cl->updateMutex);
            rfbCloseClient(cl);
            pthread_cond_signal(&cl->updateCond);
            pthread_mutex_unlock(&cl->updateMutex);
        }
        rfbReleaseClientIterator(iterator);
        while (rfbClientsConnected()) {
            usleep(200 * 1000); // 50 ms
        }
        rfbLog("Screen Re-Init\n");
        rfbScreenInit();
        pthread_mutex_unlock(&listenerAccepting);
    }    
}

static void *
clientOutput(void *data)
{
    rfbClientPtr cl = (rfbClientPtr)data;
    Bool haveUpdate;
    RegionRec updateRegion;

    while (1) {
        haveUpdate = false;
        pthread_mutex_lock(&cl->updateMutex);
        while (!haveUpdate) {            
            if (cl->sock == -1) {
                /* Client has disconnected. */
                pthread_mutex_unlock(&cl->updateMutex);
                return NULL;
            }

            // Check for pending PB changes
            rfbClientUpdatePasteboard(cl);
            
            /* REDSTONE */
            if (rfbDeferUpdateTime > 0 && !cl->immediateUpdate) {
                REGION_INIT(&hackScreen, &updateRegion, NullBox, 0);
                REGION_INTERSECT(&hackScreen, &updateRegion, &cl->modifiedRegion, &cl->requestedRegion);
                haveUpdate = REGION_NOTEMPTY(&hackScreen, &updateRegion);

                REGION_UNINIT(&hackScreen, &updateRegion);
            }
            else {
                /*
                 If we've turned off deferred updating
                 We are going to send an update as soon as we have a requested,
                 regardless of if we have a "change" intersection
                 */
                haveUpdate = REGION_NOTEMPTY(&hackScreen, &cl->requestedRegion);
            }

            if (!haveUpdate)
                pthread_cond_wait(&cl->updateCond, &cl->updateMutex);
        }
            
        // OK, now, to save bandwidth, wait a little while for more updates to come along.
        /* REDSTONE - Lets send it right away if no rfbDeferUpdateTime */
        if (rfbDeferUpdateTime > 0 && !cl->immediateUpdate) {
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
        pthread_mutex_unlock(&cl->updateMutex);

        /* Now actually send the update. */
        rfbSendFramebufferUpdate(cl, updateRegion);

        REGION_UNINIT(&hackScreen, &updateRegion);
    }

    return NULL;
}

static void *
clientInput(void *data)
{
    rfbClientPtr cl = (rfbClientPtr)data;
    pthread_t output_thread;

    pthread_create(&output_thread, NULL, clientOutput, (void *)cl);

    while (1) {
        rfbProcessClientMessage(cl);
        if (cl->sock == -1) {
            /* Client has disconnected. */
            break;
        }
    }

    /* Get rid of the output thread. */
    pthread_mutex_lock(&cl->updateMutex);
    pthread_cond_signal(&cl->updateCond);
    pthread_mutex_unlock(&cl->updateMutex);
    pthread_join(output_thread, NULL);

    rfbClientConnectionGone(cl);

    return NULL;
}

static void *
listenerRun(void *ignore)
{
    int listen_fd, client_fd;
    struct sockaddr_in sin, peer;
    pthread_t client_thread;
    rfbClientPtr cl;
    int len, value;

    bzero(&sin, sizeof(sin));
    sin.sin_len = sizeof(sin);
    sin.sin_family = AF_INET;
    if (rfbLocalhostOnly)
        sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    else
        sin.sin_addr.s_addr = htonl(INADDR_ANY);
    sin.sin_port = htons(rfbPort);

    if ((listen_fd = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
        return NULL;
    }
    value = 1;
    if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, 
                   &value, sizeof(value)) < 0) {
        rfbLog("setsockopt SO_REUSEADDR failed\n");
    }
                                                                   
    if (bind(listen_fd, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
        rfbLog("failed to bind socket\n");
        exit(1);
    }

    if (listen(listen_fd, 5) < 0) {
        rfbLog("listen failed\n");
        exit(1);
    }

    len = sizeof(peer);
    rfbLog("Started Listener Thread on port %d\n", rfbPort);

    while ((client_fd = accept(listen_fd, (struct sockaddr *)&peer, &len)) >= 0) {
        pthread_mutex_lock(&listenerAccepting);
        if (rfbLocalBuffer && !rfbClientsConnected())
            rfbLocalBufferSyncAll();
        
        rfbUndim();
        cl = rfbNewClient(client_fd);

        pthread_create(&client_thread, NULL, clientInput, (void *)cl);
        len = sizeof(peer);
        pthread_mutex_unlock(&listenerAccepting);
        pthread_cond_signal(&listenerGotNewClient);
    }

    rfbLog("accept failed\n");
    exit(1);
}

char *
rfbGetFramebuffer(void)
{
    if (rfbLocalBuffer)
        return rfbFrontBufferBaseAddress();
    else
        return (char *)CGDisplayBaseAddress(kCGDirectMainDisplay);
}

static void 
rfbScreenInit(void)
{
    (void) GetMainDevice();
    // necessary to init the display manager,
    // otherwise CGDisplayBitsPerPixel doesn't
    // always works correctly after a resolution change

    if (CGDisplaySamplesPerPixel(kCGDirectMainDisplay) != 3) {
        rfbLog("screen format not supported.  exiting.\n");
        exit(1);
    }
    
    if (rfbLocalBuffer) {
        rfbLocalBufferInfo(&rfbScreen.width, &rfbScreen.height,
                           &rfbScreen.bitsPerPixel, &rfbScreen.depth,
                           &rfbScreen.paddedWidthInBytes);
    }
    else {
        rfbScreen.width = CGDisplayPixelsWide(kCGDirectMainDisplay);
        rfbScreen.height = CGDisplayPixelsHigh(kCGDirectMainDisplay);
        rfbScreen.bitsPerPixel = CGDisplayBitsPerPixel(kCGDirectMainDisplay);
        rfbScreen.depth = CGDisplayBitsPerPixel(kCGDirectMainDisplay);
        rfbScreen.paddedWidthInBytes = CGDisplayBytesPerRow(kCGDirectMainDisplay);
    }
    rfbServerFormat.bitsPerPixel = rfbScreen.bitsPerPixel;
    rfbServerFormat.depth = rfbScreen.depth;
    rfbServerFormat.bigEndian = !(*(char *)&rfbEndianTest);
    gethostname(rfbThisHost, 255);

    if ((rfbScreen.bitsPerPixel) == 8) {
        rfbServerFormat.trueColour = FALSE;
    }
    else {
        int bitsPerSample = CGDisplayBitsPerSample(kCGDirectMainDisplay);

        rfbServerFormat.trueColour = TRUE;

        rfbServerFormat.redMax = (1 << bitsPerSample) - 1;
        rfbServerFormat.greenMax = (1 << bitsPerSample) - 1;
        rfbServerFormat.blueMax = (1 << bitsPerSample) - 1;
        rfbServerFormat.redShift = bitsPerSample * 2;
        rfbServerFormat.greenShift = bitsPerSample;
        rfbServerFormat.blueShift = 0;
    }
    
    /* We want to use the X11 REGION_* macros without having an actual
       X11 ScreenPtr, so we do this.  Pretty ugly, but at least it lets us
       avoid hacking up regionstr.h, or changing every call to REGION_*
       (which actually I should probably do eventually). */
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
}

static void
usage(void)
{
    fprintf(stderr, "\nAvailable options:\n\n");

    fprintf(stderr, "-rfbport port          TCP port for RFB protocol\n");
    fprintf(stderr, "-rfbwait time          max time in ms to wait for RFB client\n");
    fprintf(stderr, "-rfbauth passwd-file   use authentication on RFB protocol\n"
                    "                       (use 'storepasswd' to create a password file)\n");
    fprintf(stderr, "-deferupdate time      time in ms to defer updates (default 0)\n");
    fprintf(stderr, "-desktop name          VNC desktop name (default \"MacOS X\")\n");
    fprintf(stderr, "-alwaysshared          always treat new clients as shared\n");
    fprintf(stderr, "-nevershared           never treat new clients as shared\n");
    fprintf(stderr, "-dontdisconnect        don't disconnect existing clients when a new non-shared\n"
                    "                       connection comes in (refuse new connection instead)\n");
    fprintf(stderr, "-nodimming             never allow the display to dim\n"
                    "                       (default: display can dim, input undims)\n");
    fprintf(stderr, "-maxdepth bits         maximum allowed bit depth for connecting clients.\n"
                    "                       (default: bit depth of display)\n");
    /*
    fprintf(stderr, "-reversemods           reverse the interpretation of control\n");
    fprintf(stderr, "                       and command (for windows clients)\n");
     */
    fprintf(stderr, "-allowsleep            allow machine to sleep\n"
                    "                       (default: sleep is disabled)\n");
    fprintf(stderr, "-disableScreenSaver    Disable screen saver while users are connected\n"
                    "                       (default: no, allow screen saver to engage)\n");
    fprintf(stderr, "-swapButtons           swap mouse buttons 2 & 3\n"
                    "                       (default: no)\n");
    fprintf(stderr, "-disableRemoteEvents   ignore remote keyboard, pointer, and pasteboard event\n"
                    "                       (default: no, process them)\n");
    fprintf(stderr, "-rfbLocalBuffer        run the screen through a local buffer, thereby enabling the cursor\n"
                    "                       (default: no, it's slow and causes more artifacts)\n");
    fprintf(stderr, "-localhost             Only allow connections from the same machine\n");
    fprintf(stderr, "                       If you use SSH and want to stop non-SSH connections from any other hosts \n");
    fprintf(stderr, "                       (default: allows remote connections)\n");
    fprintf(stderr, "\n");

    exit(1);
}

static void 
processArguments(int argc, char *argv[])
{
    int i;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-rfbport") == 0) { /* -rfbport port */
            if (i + 1 >= argc) usage();
            rfbPort = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-rfbwait") == 0) {  /* -rfbwait ms */
            if (i + 1 >= argc) usage();
            rfbMaxClientWait = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-rfbauth") == 0) {  /* -rfbauth passwd-file */
            if (i + 1 >= argc) usage();
            rfbAuthPasswdFile = argv[++i];
        } else if (strcmp(argv[i], "-deferupdate") == 0) {      /* -deferupdate ms */
            if (i + 1 >= argc) usage();
            rfbDeferUpdateTime = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-maxdepth") == 0) {      /* -deferupdate ms */
            if (i + 1 >= argc) usage();
            rfbMaxBitDepth = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-desktop") == 0) {  /* -desktop desktop-name */
            if (i + 1 >= argc) usage();
            desktopName = argv[++i];
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
        } else if (strcmp(argv[i], "-disableScreenSaver") == 0) {
            rfbDisableScreenSaver = TRUE;
        } else if (strcmp(argv[i], "-swapButtons") == 0) {
            rfbSwapButtons = TRUE;
        } else if (strcmp(argv[i], "-disableRemoteEvents") == 0) {
            rfbDisableRemote = TRUE;
        } else if (strcmp(argv[i], "-rfbLocalBuffer") == 0) {
            rfbLocalBuffer = TRUE;
        } else if (strcmp(argv[i], "-localhost") == 0) {
            rfbLocalhostOnly = TRUE;
        } else if (strcmp(argv[i], "-inhibitevents") == 0) {
            rfbInhibitEvents = TRUE;
        } else {
            usage();
        }
    }
}

static void
rfbShutdown(void)
{
    CGUnregisterScreenRefreshCallback(refreshCallback, NULL);
    rfbDimmingShutdown();

    if (rfbDisableScreenSaver) {
        /* remove the screensaver timer */
        RemoveEventLoopTimer(screensaverTimer);
        DisposeEventLoopTimerUPP(screensaverTimerUPP);
    }

    if (rfbLocalBuffer)
        rfbLocalBufferShutdown();
}

static void
rfbShutdownOnSignal(int signal)
{
    rfbLog("OSXvnc-server received signal: %d\n", signal);
    rfbShutdown();
    
    exit (signal);
}

int 
main(int argc, char *argv[])
{
    pthread_t listener_thread;

    pthread_mutex_init(&logMutex, NULL);
    pthread_mutex_init(&listenerAccepting, NULL);
    pthread_cond_init(&listenerGotNewClient, NULL);
    
    processArguments(argc, argv);

    if (rfbLocalBuffer)
        rfbLocalBufferInit();

    rfbScreenInit();
    rfbClientListInit();
    rfbDimmingInit();

    loadKeyTable();
    
    if (rfbDisableScreenSaver) {
        /* setup screen saver disabling timer */
        screensaverTimerUPP = NewEventLoopTimerUPP(rfbScreensaverTimer);
        InstallEventLoopTimer(GetMainEventLoop(),
                              kEventDurationSecond * 30,
                              kEventDurationSecond * 30,
                              screensaverTimerUPP,
                              NULL,
                              &screensaverTimer);
    }

    // Let's not shutdown on a SIGHUP at some point perhaps we can use that to reload configuration
    
    // signal(SIGHUP, rfbShutdownOnSignal);

    signal(SIGPIPE, SIG_IGN);
    signal(SIGTERM, rfbShutdownOnSignal);
    signal(SIGINT, rfbShutdownOnSignal);
    signal(SIGQUIT, rfbShutdownOnSignal);

    pthread_create(&listener_thread, NULL, listenerRun, NULL);

    // This seems to actually sometimes inhibit REMOTE events as well, but all the same let's let everything pass through for now
    if (!rfbInhibitEvents) {
        CGSetLocalEventsFilterDuringSupressionState(kCGEventFilterMaskPermitAllEvents, kCGEventSupressionStateSupressionInterval);
        CGSetLocalEventsFilterDuringSupressionState(kCGEventFilterMaskPermitAllEvents, kCGEventSupressionStateRemoteMouseDrag);
    }
    
    // This segment is what is responsible for causing the server to shutdown when a user logs out
    // The problem being that OS X sends it first a SIGTERM and then a SIGKILL (un-trappable)
    // Presumable because it's running a Carbon Event loop
    if (1) {
        BOOL keepRunning = YES;
        OSStatus resultCode = 0;
        
        rfbLog("Registering Screen Update Notification\n");
        CGRegisterScreenRefreshCallback(refreshCallback, NULL);
        // No better luck with this one
        // RunApplicationEventLoop();
        while (keepRunning) {
            if (!rfbClientsConnected()) { // No clients
                pthread_mutex_lock(&listenerAccepting);
                pthread_cond_wait(&listenerGotNewClient, &listenerAccepting);
                pthread_mutex_unlock(&listenerAccepting);
            }
            rfbCheckForPasteboardChange();
            resultCode = RunCurrentEventLoop(kEventDurationMillisecond); //EventTimeout
            if (resultCode != eventLoopTimedOutErr) {
                rfbLog("Received Result: %d during event loop, Shutting Down", resultCode);
                keepRunning = NO;
            }
        }
    }
    // So this looks like it should fix it but I get no response on the CGWaitForScreenRefreshRect....
    else while (1) {
        CGRectCount rectCount;
        CGRect *rectArray;
        CGEventErr result;

        // This doesn't seem to get called at all when not running an event loop
        result = CGWaitForScreenRefreshRects( &rectArray, &rectCount );
        refreshCallback(rectCount, rectArray, NULL);
        CGReleaseScreenRefreshRects( rectArray );
    }
    rfbShutdown();

    return 0;
}
