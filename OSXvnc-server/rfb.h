/*
 * rfb.h - header file for RFB DDX implementation.
 */

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

#include "scrnintstr.h"

/* trying to replace the above with some more minimal set of includes */
#include "misc.h"
#include "Xmd.h"
#include "regionstr.h"

#include <rfbproto.h>
#include <vncauth.h>
#include <zlib.h>

#define MAX_ENCODINGS 17


/*
 * Per-screen (framebuffer) structure.  There is only one of these, since we
 * don't allow the X server to have multiple screens.
 */

typedef struct
{
    int width;
    int paddedWidthInBytes;
    int height;
    int depth;
    int bitsPerPixel;
    int sizeInBytes;
} rfbScreenInfo, *rfbScreenInfoPtr;


/*
 * rfbTranslateFnType is the type of translation functions.
 */

struct rfbClientRec;
typedef void (*rfbTranslateFnType)(char *table, rfbPixelFormat *in,
                                   rfbPixelFormat *out,
                                   char *iptr, char *optr,
                                   int bytesBetweenInputLines,
                                   int width, int height);


/*
 * Per-client structure.
 */

typedef struct rfbClientRec {

    int sock;
    char *host;
    pthread_mutex_t outputMutex;

                                /* Possible client states: */
    enum {
        RFB_PROTOCOL_VERSION,   /* establishing protocol version */
        RFB_AUTHENTICATION,     /* authenticating */
        RFB_INITIALISATION,     /* sending initialisation messages */
        RFB_NORMAL              /* normal protocol messages */
    } state;

    Bool reverseConnection;

    Bool readyForSetColourMapEntries;

    Bool useCopyRect;
    int preferredEncoding;
    int correMaxWidth, correMaxHeight;
    void* zrleData;
    void* mosData;

    /* The following member is only used during VNC authentication */

    CARD8 authChallenge[CHALLENGESIZE];

    /* The following members represent the update needed to get the client's
       framebuffer from its present state to the current state of our
       framebuffer.

       The update is simply represented as the region of the screen
       which has been modified (modifiedRegion).  CopyRect is not
       supported, because there's no way I can get the OS to tell me
       something has been copied.

       updateMutex should be held, and updateCond signalled, whenever
       either modifiedRegion or requestedRegion is changed (as either
       of these may trigger sending an update out to the client). */

    pthread_mutex_t updateMutex;
    pthread_cond_t updateCond;

    RegionRec modifiedRegion;

    /* As part of the FramebufferUpdateRequest, a client can express interest
       in a subrectangle of the whole framebuffer.  This is stored in the
       requestedRegion member.  In the normal case this is the whole
       framebuffer if the client is ready, empty if it's not. */

    RegionRec requestedRegion;

    /* The following members represent the state of the "deferred update" timer
        - when the framebuffer is modified and the client is ready, in most
        cases it is more efficient to defer sending the update by a few
        milliseconds so that several changes to the framebuffer can be combined
        into a single update. */

    Bool deferredUpdateScheduled;
    OsTimerPtr deferredUpdateTimer;

    /* translateFn points to the translation function which is used to copy
       and translate a rectangle from the framebuffer to an output buffer. */

    rfbTranslateFnType translateFn;

    char *translateLookupTable;

    rfbPixelFormat format;

    /* statistics */

    int rfbBytesSent[MAX_ENCODINGS];
    int rfbRectanglesSent[MAX_ENCODINGS];
    int rfbLastRectMarkersSent;
    int rfbLastRectBytesSent;
    int rfbFramebufferUpdateMessagesSent;
    int rfbRawBytesEquivalent;
    int rfbKeyEventsRcvd;
    int rfbPointerEventsRcvd;

  /* zlib encoding -- necessary compression state info per client */

    struct z_stream_s compStream;
    Bool compStreamInited;

    CARD32 zlibCompressLevel;

    struct z_stream_s compStreamRaw;
    struct z_stream_s compStreamHex;

    /*
     * zlibBeforeBuf contains pixel data in the client's format.
     * zlibAfterBuf contains the zlib (deflated) encoding version.
     * If the zlib compressed/encoded version is
     * larger than the raw data or if it exceeds zlibAfterBufSize then
     * raw encoding is used instead.
     */
    
    int client_zlibBeforeBufSize;
    char *client_zlibBeforeBuf;

    int client_zlibAfterBufSize;
    char *client_zlibAfterBuf;
    int client_zlibAfterBufLen;

    // These defines will "hopefully" allow us to keep the rest of the code looking roughly the same
    // but reference them out of the client record pointer, where they need to be, instead of as globals
#define zlibBeforeBufSize cl->client_zlibBeforeBufSize
#define zlibBeforeBuf     cl->client_zlibBeforeBuf

#define zlibAfterBufSize  cl->client_zlibAfterBufSize
#define zlibAfterBuf      cl->client_zlibAfterBuf
#define zlibAfterBufLen   cl->client_zlibAfterBufLen
    
    /* tight encoding -- preserve zlib streams' state for each client */

    z_stream zsStruct[4];
    Bool zsActive[4];
    int zsLevel[4];
    int tightCompressLevel;
    int tightQualityLevel;

    Bool enableLastRectEncoding;   /* client supports LastRect encoding */
    Bool enableCursorShapeUpdates; /* client supports cursor shape updates */
    Bool useRichCursorEncoding;    /* rfbEncodingRichCursor is preferred */
    Bool cursorWasChanged;         /* cursor shape update should be sent */

    /* ZRLE -- Zlib Run Length Encoding Variable Space */

    char client_zrleBeforeBuf[rfbZRLETileWidth * rfbZRLETileHeight * 4 + 4];

    /* REDSTONE - Adding some features */

    Bool disableRemoteEvents;      // Ignore PB, Keyboard and Mouse events
    Bool swapMouseButtons23;       // How to interpret mouse buttons 2 & 3
    Bool immediateUpdate;       // To request that we get immediate updates (even 0 rects)

    int pasteBoardLastChange;      // Used to see if we need to send the latest PB
    
    /* REDSTONE - These (updateBuf, ublen) need to be in the CL, not global, for multiple clients */

    /*
     * UPDATE_BUF_SIZE must be big enough to send at least one whole line of the
     * framebuffer.  So for a max screen width of say 2K with 32-bit pixels this
     * means 8K minimum.
     */

#define UPDATE_BUF_SIZE 30000
    char updateBuf[UPDATE_BUF_SIZE];
    int ublen;
    
    struct rfbClientRec *prev;
    struct rfbClientRec *next;
} rfbClientRec, *rfbClientPtr;


/*
 * This macro is used to test whether there is a framebuffer update needing to
 * be sent to the client.
 */

#define FB_UPDATE_PENDING(cl)                           \
     REGION_NOTEMPTY(&hackScreen,&(cl)->copyRegion) ||  \
     REGION_NOTEMPTY(&hackScreen,&(cl)->modifiedRegion)

/*
 * This macro creates an empty region (ie. a region with no areas) if it is
 * given a rectangle with a width or height of zero. It appears that 
 * REGION_INTERSECT does not quite do the right thing with zero-width
 * rectangles, but it should with completely empty regions.
 */

#define SAFE_REGION_INIT(pscreen, preg, rect, size)          \
{                                                            \
      if ( ( (rect) ) &&                                     \
           ( ( (rect)->x2 == (rect)->x1 ) ||                 \
             ( (rect)->y2 == (rect)->y1 ) ) ) {              \
          REGION_INIT( (pscreen), (preg), NullBox, 0 );      \
      } else {                                               \
          REGION_INIT( (pscreen), (preg), (rect), (size) );  \
      }                                                      \
}


/*
 * Macros for endian swapping.
 */

#define Swap16(s) ((((s) & 0xff) << 8) | (((s) >> 8) & 0xff))

#define Swap32(l) (((l) >> 24) | \
                   (((l) & 0x00ff0000) >> 8)  | \
                   (((l) & 0x0000ff00) << 8)  | \
                   ((l) << 24))


/* At this point in time OS X only runs on PowerPCs, so we're
   big-endian. */
static const int rfbEndianTest = 0;

#define Swap16IfLE(s) (*(const char *)&rfbEndianTest ? Swap16(s) : (s))

#define Swap32IfLE(l) (*(const char *)&rfbEndianTest ? Swap32(l) : (l))

/* main.c */

extern char *rfbGetFramebuffer();

extern ScreenRec hackScreen;
extern rfbScreenInfo rfbScreen;

extern char *desktopName;
extern char rfbThisHost[];

extern Bool rfbAlwaysShared;
extern Bool rfbNeverShared;
extern Bool rfbDontDisconnect;
extern Bool rfbReverseMods;

/* REDSTONE - Additions */
extern Bool rfbSwapButtons;
extern Bool rfbDisableRemote;

extern Bool rfbLocalBuffer;
extern Bool currentlyRefreshing;

extern void rfbLog(char *format, ...);
extern void rfbLogPerror(char *str);


/* sockets.c */

extern int rfbMaxClientWait;

extern void rfbCloseClient(rfbClientPtr cl);
extern int ReadExact(rfbClientPtr cl, char *buf, int len);
extern int WriteExact(rfbClientPtr cl, char *buf, int len);


/* cutpaste.c */

extern void rfbSetXCutText(char *str, int len);
extern void rfbCheckForPasteboardChange();
extern void rfbClientUpdatePasteboard(rfbClientPtr cl);

/* kbdptr.c */

extern char *keymapFile;

extern void PtrAddEvent(int buttonMask, int x, int y, rfbClientPtr cl);
extern void KbdAddEvent(Bool down, KeySym keySym, rfbClientPtr cl);
extern void loadKeyTable();

/* rfbserver.c */

//extern char updateBuf[UPDATE_BUF_SIZE];
//extern int ublen;

extern rfbClientPtr pointerClient;


/* Routines to iterate over the client list in a thread-safe way.
   Only a single iterator can be in use at a time process-wide. */
typedef struct rfbClientIterator *rfbClientIteratorPtr;

extern void rfbClientListInit(void);
extern rfbClientIteratorPtr rfbGetClientIterator(void);
extern rfbClientPtr rfbClientIteratorNext(rfbClientIteratorPtr iterator);
extern void rfbReleaseClientIterator(rfbClientIteratorPtr iterator);
extern Bool rfbClientsConnected();

extern void rfbNewClientConnection(int sock);
extern rfbClientPtr rfbNewClient(int sock);
extern rfbClientPtr rfbReverseConnection(char *host, int port);
extern void rfbClientConnectionGone(rfbClientPtr cl);
extern void rfbProcessClientMessage(rfbClientPtr cl);
extern void rfbClientConnFailed(rfbClientPtr cl, char *reason);
extern void rfbNewUDPConnection(int sock);
extern void rfbProcessUDPInput(int sock);
extern Bool rfbSendFramebufferUpdate(rfbClientPtr cl, RegionRec updateRegion);
extern Bool rfbSendRectEncodingRaw(rfbClientPtr cl, int x,int y,int w,int h);
extern Bool rfbSendUpdateBuf(rfbClientPtr cl);
extern void rfbSendServerCutText(rfbClientPtr cl, char *str, int len);


/* translate.c */

extern Bool rfbEconomicTranslate;
extern rfbPixelFormat rfbServerFormat;

extern void rfbTranslateNone(char *table, rfbPixelFormat *in,
                             rfbPixelFormat *out,
                             char *iptr, char *optr,
                             int bytesBetweenInputLines,
                             int width, int height);
extern Bool rfbSetTranslateFunction(rfbClientPtr cl);


/* httpd.c */

extern int httpPort;
extern char *httpDir;

extern void httpInitSockets();
extern void httpCheckFds();



/* auth.c */

extern char *rfbAuthPasswdFile;
extern Bool rfbAuthenticating;

extern void rfbAuthNewClient(rfbClientPtr cl);
extern void rfbAuthProcessClientMessage(rfbClientPtr cl);


/* rre.c */

extern Bool rfbSendRectEncodingRRE(rfbClientPtr cl, int x,int y,int w,int h);


/* corre.c */

extern Bool rfbSendRectEncodingCoRRE(rfbClientPtr cl, int x,int y,int w,int h);


/* hextile.c */

extern Bool rfbSendRectEncodingHextile(rfbClientPtr cl, int x, int y, int w,
                                       int h);

/* zlib.c */

/* Minimum zlib rectangle size in bytes.  Anything smaller will
 * not compress well due to overhead.
 */
#define VNC_ENCODE_ZLIB_MIN_COMP_SIZE (17)

/* Set maximum zlib rectangle size in pixels.  Always allow at least
 * two scan lines.
 */
#define ZLIB_MAX_RECT_SIZE (128*256)
#define ZLIB_MAX_SIZE(min) ((( min * 2 ) > ZLIB_MAX_RECT_SIZE ) ? \
                            ( min * 2 ) : ZLIB_MAX_RECT_SIZE )

extern Bool rfbSendRectEncodingZlib(rfbClientPtr cl, int x, int y, int w,
                                    int h);

/* tight.c */

#define TIGHT_DEFAULT_COMPRESSION  6

extern Bool rfbTightDisableGradient;

extern int rfbNumCodedRectsTight(rfbClientPtr cl, int x,int y,int w,int h);
extern Bool rfbSendRectEncodingTight(rfbClientPtr cl, int x,int y,int w,int h);


/* zlibhex.c */

/* Minimum zlibhex tile size in bytes.  Anything smaller will
 * not compress well due to overhead.
 */
#define VNC_ENCODE_ZLIBHEX_MIN_COMP_SIZE (17)

/* Compressor state uninitialized value.  Changed after one time
 * initialization.
 */
#define ZLIBHEX_COMP_UNINITED (-1)

extern Bool rfbSendRectEncodingZlibHex(rfbClientPtr cl, int x, int y, int w,
                                          int h);

/* zrle.c */

extern Bool rfbSendRectEncodingZRLE(rfbClientPtr cl, int x, int y, int w,
                                    int h);
extern void FreeZrleData(rfbClientPtr cl);

/* stats.c */

extern void rfbResetStats(rfbClientPtr cl);
extern void rfbPrintStats(rfbClientPtr cl);


/* dimming.c */

extern Bool rfbNoDimming;
extern Bool rfbNoSleep;

extern int rfbDimmingInit(void);
extern int rfbUndim(void);
extern int rfbDimmingShutdown(void);
