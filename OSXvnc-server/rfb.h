/*
 * rfb.h - header file for RFB DDX implementation.
 */

/*
 *  OSXvnc Copyright (C) 2001  *  OSXvnc Copyright (C) 2002-2004 Redstone Software osxvnc@redstonesoftware.comGuirk <mcguirk@incompleteness.net>.
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

#include <pthread.h>
#include <machine/types.h>
#include <rfbproto.h>
#include <vncauth.h>
#include <zlib.h>
#include "tight.h"

#import "screencapture.h"

//#include "Keyboards.h"
//#import <Carbon/Carbon.h>
//#include <ApplicationServices/ApplicationServices.h>

#include "CoreGraphics/CGGeometry.h"

#include <IOKit/pwr_mgt/IOPMLib.h>

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
typedef void (*rfbTranslateFnType)(char *table,
                                   rfbPixelFormat *in,
                                   rfbPixelFormat *out,
                                   char *iptr,
                                   char *optr,
                                   int bytesBetweenInputLines,
                                   int width,
                                   int height);


/*
 * Per-client structure.
 */

enum client_state {
	RFB_PROTOCOL_VERSION,   /* establishing protocol version */
	RFB_AUTH_VERSION,       /* establishing authentication version (3.7) */
	RFB_AUTHENTICATION,     /* authenticating */
	RFB_INITIALISATION,     /* sending initialisation messages */
	RFB_NORMAL              /* normal protocol messages */
} ;

typedef struct rfbClientRec {

    int sock;
    char *host;
    // Version
    int major, minor;

    pthread_mutex_t outputMutex;

                                /* Possible client states: */
    enum client_state state;

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

    /* translateFn points to the translation function which is used to copy
       and translate a rectangle from the framebuffer to an output buffer. */

    rfbTranslateFnType translateFn;

    char *translateLookupTable;

    rfbPixelFormat format;

    /* SERVER SCALING EXTENSIONS */
    int	  scalingFactor;
	char* screenBuffer;
    char* scalingFrameBuffer;
    int   scalingPaddedWidthInBytes;

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
    Bool enableXCursorShapeUpdates; /* client supports XCursor shape updates - Tight */
    Bool useRichCursorEncoding;    /* rfbEncodingRichCursor is preferred : Tight and RFB - 3.7 */
    Bool enableCursorPosUpdates;   /* client supports PointerPos updates - Tight */
    Bool desktopSizeUpdate;        /* client supports dynamic desktop updates - Tight and RFB 3.7 */

    Bool reverseConnection;
    Bool readyForSetColourMapEntries;
    Bool useCopyRect;

    int preferredEncoding;

    /* tight encoding -- This variable is set on every rfbSendRectEncodingTight() call. */
    Bool usePixelFormat24;

    /* tight encoding -- Compression level stuff. */

    int compressLevel;
    int qualityLevel;

    /* tight encoding -- Stuff dealing with palettes. */

    int paletteNumColors, paletteMaxColors;
    CARD32 monoBackground, monoForeground;
    PALETTE palette;

    /* tight encoding -- Pointers to dynamically-allocated buffers. */

    int tightBeforeBufSize;
    char *tightBeforeBuf;

    int tightAfterBufSize;
    char *tightAfterBuf;

    int *prevRowBuf;

    /* tight encoding -- JPEG compression stuff. */

    struct jpeg_destination_mgr jpegDstManager;
    Bool jpegError;
    int jpegDstDataLen;

    j_compress_ptr cinfo;  /* tight encoding -- GetClient() uses this to map cinfos to client records */

    // These defines will "hopefully" allow us to keep the rest of the code looking roughly the same
    // but reference them out of the client record pointer, where they need to be, instead of as globals
#define usePixelFormat24   cl->usePixelFormat24

#define compressLevel      cl->compressLevel
#define qualityLevel       cl->qualityLevel

#define paletteNumColors   cl->paletteNumColors
#define paletteMaxColors   cl->paletteMaxColors
#define monoBackground     cl->monoBackground
#define monoForeground     cl->monoForeground

#define tightBeforeBufSize cl->tightBeforeBufSize
#define tightBeforeBuf     cl->tightBeforeBuf
#define tightAfterBufSize  cl->tightAfterBufSize
#define tightAfterBuf      cl->tightAfterBuf
#define prevRowBuf         cl->prevRowBuf

#define jpegDstManager     cl->jpegDstManager
#define jpegError          cl->jpegError
#define jpegDstDataLen     cl->jpegDstDataLen

    /* ZRLE -- Zlib Run Length Encoding Variable Space */

    char client_zrleBeforeBuf[rfbZRLETileWidth * rfbZRLETileHeight * 4 + 4];

    /* REDSTONE - Adding some features */

    Bool disableRemoteEvents;      // Ignore PB, Keyboard and Mouse events
    Bool swapMouseButtons23;       // How to interpret mouse buttons 2 & 3
    Bool immediateUpdate;          // To request that we get immediate updates (even 0 rects)

	Bool richClipboardSupport;     // Client has indicated they support rich clipboards
	void *richClipboardChangeCounts; // Dictionary of local ChangeCount NSNumbers stored by PB Name

	/* These store temporary values during a rich clipboard transfer (one at a time per client) */
	void *clipboardProxy;
	char *richClipboardName;
	char *richClipboardType;
	void *richClipboardNSData;
	int   richClipboardDataChangeCount;

	void *richClipboardReceivedName;
	void *richClipboardReceivedType;
	void *richClipboardReceivedNSData;
	void *receivedFileTempFolder;
	int   richClipboardReceivedChangeCount;


    int generalPBLastChange;      // Used to see if we need to send the latest general PB

	// Cursor Info

    int currentCursorSeed;         // Used to see if we need to send a new cursor
    CGPoint clientCursorLocation;  // The last location the client left the mouse at

    BOOL needNewScreenSize;        // Flag to indicate we must send a new screen resolution
    BOOL modiferKeys[256];         // BOOL Array to record which keys THIS user has down, if they disconnect we will release those keys

    /* REDSTONE - These (updateBuf, ublen) need to be in the CL, not global, for multiple clients */
	screen_data_t * p_data;

    /*
     * UPDATE_BUF_SIZE must be big enough to send at least one whole line of the
     * framebuffer.  So for a max screen width of say 2K with 32-bit pixels this
     * means 8K minimum.
     */

#define UPDATE_BUF_SIZE 30000
    char updateBuf[UPDATE_BUF_SIZE];
    uint32_t ublen;

    struct rfbClientRec *prev;
    struct rfbClientRec *next;
} rfbClientRec, *rfbClientPtr;


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


// NSByteOrder.h
// kCGBitmapByteOrder32Host __i386__ __ppc__
// kCGBitmapByteOrder16Big AC_C_BIGENDIAN
// #define rfbEndianTest (0)

#include <arpa/inet.h>

#define Swap16IfLE(s) htons(s)
#define Swap32IfLE(l) htonl(l)

/* main.c */

extern unsigned rfbProtocolMajorVersion;
extern unsigned rfbProtocolMinorVersion;

extern unsigned rfbPort;

extern char *rfbGetFramebuffer(void);
extern void rfbGetFramebufferUpdateInRect(int x, int y, int w, int h);

extern void rfbStartClientWithFD(int client_fd);
extern void connectReverseClient(char *hostName, int portNum);

extern rfbScreenInfo rfbScreen;

extern char desktopName[256];

extern BOOL littleEndian;
extern int  rfbMaxBitDepth;
extern Bool rfbAlwaysShared;
extern Bool rfbNeverShared;
extern Bool rfbDontDisconnect;
extern Bool rfbReverseMods;

extern Bool rfbSwapButtons;
extern Bool rfbDisableRemote;
extern Bool rfbDisableRichClipboards;

extern Bool rfbLocalBuffer;

extern void rfbLog(char *format, ...);
extern void rfbDebugLog(char *format, ...);
extern void rfbLogPerror(char *str);

extern void rfbShutdown(void);

/* sockets.c */

extern int rfbMaxClientWait;

extern void rfbCloseClient(rfbClientPtr cl);
extern int ReadExact(rfbClientPtr cl, void *buf, size_t len);
extern int WriteExact(rfbClientPtr cl, const void *buf, size_t len);

/* cutpaste.c */

extern void initPasteboard(void);
extern void initPasteboardForClient(rfbClientPtr cl);
extern void freePasteboardForClient(rfbClientPtr cl);

extern void rfbSetCutText(rfbClientPtr cl, char *str, int len);
extern void rfbCheckForPasteboardChange(void);
extern void rfbClientUpdatePasteboard(rfbClientPtr cl);

extern void rfbReceiveRichClipboardAvailable(rfbClientPtr cl);
extern void rfbReceiveRichClipboardRequest(rfbClientPtr cl);
extern void rfbReceiveRichClipboardData(rfbClientPtr cl);

/* kbdptr.c */

extern void PtrAddEvent(int buttonMask, int x, int y, rfbClientPtr cl);
extern void KbdAddEvent(Bool down, KeySym keySym, rfbClientPtr cl);
extern void keyboardReleaseKeysForClient(rfbClientPtr cl);

/* rfbserver.c */

//extern char updateBuf[UPDATE_BUF_SIZE];
//extern int ublen;

extern rfbClientPtr pointerClient;

extern rfbClientPtr rfbClientHead;  /* tight encoding -- GetClient() in tight.c accesses this list, so make it global */

extern void rfbProcessClientProtocolVersion(rfbClientPtr cl);
extern void rfbProcessClientNormalMessage(rfbClientPtr cl);
extern void rfbProcessClientInitMessage(rfbClientPtr cl);
extern Bool rfbSendScreenUpdateEncoding(rfbClientPtr cl);
extern Bool rfbSendLastRectMarker(rfbClientPtr cl);


/* Routines to iterate over the client list in a thread-safe way.
   Only a single iterator can be in use at a time process-wide. */
typedef struct rfbClientIterator *rfbClientIteratorPtr;

extern void rfbClientListInit(void);
extern rfbClientIteratorPtr rfbGetClientIterator(void);
extern rfbClientPtr rfbClientIteratorNext(rfbClientIteratorPtr iterator);
extern void rfbReleaseClientIterator(rfbClientIteratorPtr iterator);
extern Bool rfbClientsConnected(void);

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
extern void rfbSendServerCutText(rfbClientPtr cl, char *str, size_t len);

extern void setScaling (rfbClientPtr cl);

/* translate.c */

/*
 * Macro to compare pixel formats.
 */

#define PF_EQ(x,y)                                                  \
((x.bitsPerPixel == y.bitsPerPixel) &&                          \
 (x.depth == y.depth) &&                                        \
 ((x.bigEndian == y.bigEndian) || (x.bitsPerPixel == 8)) &&     \
 (x.trueColour == y.trueColour) &&                              \
 (!x.trueColour || ((x.redMax == y.redMax) &&                   \
                    (x.greenMax == y.greenMax) &&               \
                    (x.blueMax == y.blueMax) &&                 \
                    (x.redShift == y.redShift) &&               \
                    (x.greenShift == y.greenShift) &&           \
                    (x.blueShift == y.blueShift))))



extern Bool rfbEconomicTranslate;
extern rfbPixelFormat rfbServerFormat;

extern void rfbTranslateNone(char *table, rfbPixelFormat *in,
                             rfbPixelFormat *out,
                             char *iptr, char *optr,
                             int bytesBetweenInputLines,
                             int width, int height);
extern Bool rfbSetTranslateFunction(rfbClientPtr cl);
extern Bool rfbSetTranslateFunctionUsingFormat(rfbClientPtr cl, rfbPixelFormat inFormat);
extern void PrintPixelFormat(rfbPixelFormat *pf);


/* httpd.c */

extern int httpPort;
extern char *httpDir;

extern void httpInitSockets(void);
extern void httpCheckFds(void);



/* auth.c */

extern Bool allowNoAuth;
extern char *rfbAuthPasswdFile;
extern Bool rfbAuthenticating;
extern int rfbMaxLoginAttempts;

extern void rfbAuthInit(void);
extern void rfbAuthNewClient(rfbClientPtr cl);
extern void rfbProcessAuthVersion(rfbClientPtr cl);
extern void rfbAuthProcessClientMessage(rfbClientPtr cl);

extern bool enterSuppliedPassword(char *passIn);

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

extern char* encNames[];

extern void rfbResetStats(rfbClientPtr cl);
extern void rfbPrintStats(rfbClientPtr cl);


/* dimming.c */

extern Bool rfbNoDimming;
extern Bool rfbNoSleep;
extern IOPMAssertionID userActivityLastAssertionId;

extern int rfbDimmingInit(void);
extern int rfbUndim(void);
extern int rfbDimmingShutdown(void);

/* mousecursor.c */

extern void GetCursorInfo(void);
extern void rfbCheckForCursorChange(void);
extern Bool rfbShouldSendNewCursor(rfbClientPtr cl);
extern Bool rfbShouldSendNewPosition(rfbClientPtr cl);

extern Bool rfbSendRichCursorUpdate(rfbClientPtr cl);
extern Bool rfbSendCursorPos(rfbClientPtr cl);

/* screencapture.c */

screen_data_t *screen_InitCapture(void);
extern char *screen_Capture (screen_data_t *p_data);
extern void screen_CloseCapture (screen_data_t *p_data);

