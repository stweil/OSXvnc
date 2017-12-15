/*
 * rfbserver.c - deal with server-side of the RFB protocol.
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

#include <Cocoa/Cocoa.h>
#include <Carbon/Carbon.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#include <netdb.h>

#include "rfb.h"

//char updateBuf[UPDATE_BUF_SIZE];
//int ublen;

rfbClientPtr pointerClient = NULL;  /* Mutex for pointer events with buttons down*/

rfbClientPtr rfbClientHead;  /* tight encoding -- GetClient() in tight.c accesses this list, so make it global */

struct rfbClientIterator {
    rfbClientPtr next;
};

static pthread_mutex_t rfbClientListMutex;
static struct rfbClientIterator rfbClientIteratorInstance;

static void CopyScalingRect(rfbClientPtr cl,
                            uint32_t* x, uint32_t* y, uint32_t* w, uint32_t* h,
                            bool bDoScaling);

void
rfbClientListInit(void)
{
    rfbClientHead = NULL;
    pthread_mutex_init(&rfbClientListMutex, NULL);
}

rfbClientIteratorPtr
rfbGetClientIterator(void)
{
    pthread_mutex_lock(&rfbClientListMutex);
    rfbClientIteratorInstance.next = rfbClientHead;

    return &rfbClientIteratorInstance;
}

rfbClientPtr
rfbClientIteratorNext(rfbClientIteratorPtr iterator)
{
    rfbClientPtr result = iterator->next;
    if (result)
        iterator->next = result->next;
    return result;
}

void
rfbReleaseClientIterator(rfbClientIteratorPtr iterator)
{
    pthread_mutex_unlock(&rfbClientListMutex);
}

Bool rfbClientsConnected()
{
    return (rfbClientHead != NULL);
}

void rfbSendClientList() {
    pthread_mutex_lock(&rfbClientListMutex);

	NSAutoreleasePool *pool=[[NSAutoreleasePool alloc] init];
	NSMutableArray *clientList = [[NSMutableArray alloc] init];
	rfbClientPtr myClient = rfbClientHead;

	while (myClient != NULL) {
		[clientList addObject:@{@"clientIP": @(myClient->host)}];
		myClient = myClient->next;
	}

    [[NSDistributedNotificationCenter defaultCenter] postNotificationName:@"VNCConnections"
                                                                    object:[NSString stringWithFormat:@"OSXvnc%d",rfbPort]
                                                                  userInfo:@{@"clientList": clientList}];

    [clientList dealloc];
	[pool release];

    pthread_mutex_unlock(&rfbClientListMutex);
}

/*
 * rfbNewClientConnection is called from sockets.c when a new connection
 * comes in.
 */

void rfbNewClientConnection(int sock) {
    rfbNewClient(sock);
}


/*
 * rfbReverseConnection is called to make an outward
 * connection to a "listening" RFB client.
 */
rfbClientPtr rfbReverseConnection(char *host, int port) {
    int sock= -1;
	struct addrinfo *res, *res0, hint;
	int errCode;
    rfbClientPtr cl;

	{
		// Old IPV4 stuff
		struct sockaddr_in sin;
	    bzero(&sin, sizeof(sin));
		sin.sin_len = sizeof(sin);
		sin.sin_family = AF_INET;
		sin.sin_addr.s_addr = inet_addr(host);
		sin.sin_port = htons(port);
		if ((int)sin.sin_addr.s_addr == -1) {
			struct hostent *hostinfo = gethostbyname(host);
			if (hostinfo && hostinfo->h_addr) {
				sin.sin_addr.s_addr = ((struct in_addr *)hostinfo->h_addr)->s_addr;
			}
			else {
				rfbLog("Error resolving reverse host %s", host);
				return NULL;
			}
		}

		if ((sock = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
			rfbLog("Error creating reverse socket");
		}
		if (connect(sock, (struct sockaddr *)&sin, sizeof(sin)) != 0) {
			rfbLog("Error connecting to reverse host %s:%d", host, port);
			sock = -1;
		}
	}

	if (sock == -1) {
		hint.ai_family = PF_UNSPEC;
		hint.ai_socktype = SOCK_STREAM;
		if ((errCode = getaddrinfo(host, NULL, &hint, &res0)) != 0) {
			rfbLog("Error resolving reverse host %s: %s", host, gai_strerror(errCode));
		}

		// Iterate over all the resources looking for one we can connect with with.
		for(res=res0; res && (sock < 0); res = res->ai_next) {
			if (res->ai_family == PF_INET6)
				((struct sockaddr_in6 *)(res->ai_addr))->sin6_port = port;
			else if (res->ai_family == PF_INET)
				((struct sockaddr_in *)(res->ai_addr))->sin_port = port;
			else {
				rfbLog("Error creating reverse socket: unrecognized protocol family %d", res->ai_family);
				continue;
			}

			if ((sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) < 0) {
				rfbLog("Error creating reverse socket: %s", strerror(errno));
			} else if (connect(sock, res->ai_addr, res->ai_addrlen) != 0) {
				rfbLog("Error connecting to reverse host %s:%d: %s", host, port, strerror(errno));
				sock = -1;
			}
		}
		freeaddrinfo(res0);
	}

	if (sock < 0)
		return NULL;

	cl = rfbNewClient(sock);
	if (cl) {
		cl->reverseConnection = TRUE;
	}

    return cl;
}

/*
 * rfbNewClient is called when a new connection has been made by whatever
 * means.
 */

rfbClientPtr rfbNewClient(int sock) {
    rfbProtocolVersionMsg pv;
    rfbClientPtr cl;
    BoxRec box;
    int i;
	unsigned int addrlen;
	int bitsPerSample;

    /*
     {
         rfbClientIteratorPtr iterator;

         rfbLog("syncing other clients:");
         iterator = rfbGetClientIterator();
         while ((cl = rfbClientIteratorNext(iterator)) != NULL) {
             rfbLog("     %s",cl->host);
         }
         rfbReleaseClientIterator(iterator);
     }
     */

    cl = (rfbClientPtr)xalloc(sizeof(rfbClientRec));
    memset(cl, 0, sizeof(rfbClientRec));

    cl->sock = sock;
	if (floor(NSAppKitVersionNumber) > floor(NSAppKitVersionNumber10_1)) {
		struct sockaddr_in6 addr;
		char host[NI_MAXHOST];

		addrlen = sizeof(struct sockaddr_in6);

		host[0] = 0;
		getnameinfo((struct sockaddr *)&addr, addrlen, host, sizeof(host), NULL, 0, NI_NUMERICHOST); // Not available on 10.1
		cl->host = strdup(host);
	}
	if (!strlen(cl->host)) {
		struct sockaddr_in addr;
		addrlen = sizeof(struct sockaddr_in);
		getpeername(sock, (struct sockaddr *)&addr, &addrlen);
		cl->host = strdup(inet_ntoa(addr.sin_addr));
	}

    pthread_mutex_init(&cl->outputMutex, NULL);

    cl->state = RFB_PROTOCOL_VERSION;

    /* REDSTONE - Adding some features
        In theory these need not be global, but could be set per client
        */
    cl->disableRemoteEvents = rfbDisableRemote;      // Ignore PB, Keyboard and Mouse events
    cl->swapMouseButtons23 = rfbSwapButtons;         // How to interpret mouse buttons 2 & 3

    cl->needNewScreenSize = NO;
	initPasteboardForClient(cl);

    cl->reverseConnection = FALSE;
    cl->preferredEncoding = rfbEncodingRaw;
    cl->correMaxWidth = 48;
    cl->correMaxHeight = 48;
    cl->zrleData = 0;
    cl->mosData = 0;

    for (i = 0; i < 256; i++)
        cl->modiferKeys[i] = 0;

    box.x1 = box.y1 = 0;
    box.x2 = rfbScreen.width;
    box.y2 = rfbScreen.height;
    REGION_INIT(pScreen,&cl->modifiedRegion,&box,0);

    pthread_mutex_init(&cl->updateMutex, NULL);
    pthread_cond_init(&cl->updateCond, NULL);

    REGION_INIT(pScreen,&cl->requestedRegion,NullBox,0);

	switch (rfbMaxBitDepth) {
		case 32:
		case 16:
		case 8:
			cl->format.bitsPerPixel = max(rfbMaxBitDepth, rfbScreen.bitsPerPixel);
			bitsPerSample = cl->format.bitsPerPixel << 2;
			cl->format.depth = bitsPerSample*3;
			cl->format.bigEndian = !littleEndian;
			cl->format.trueColour = TRUE;

			cl->format.redMax = (1 << bitsPerSample) - 1;
			cl->format.greenMax = (1 << bitsPerSample) - 1;
			cl->format.blueMax = (1 << bitsPerSample) - 1;

			cl->format.redShift = bitsPerSample * 2;
			cl->format.greenShift = bitsPerSample * 1;
			cl->format.blueShift = bitsPerSample * 0;
			break;
		case 0:
		default:
			cl->format = rfbServerFormat;
			break;
	}
	// This will
	rfbSetTranslateFunctionUsingFormat(cl, rfbServerFormat);

    /* SERVER SCALING EXTENSIONS -- Server Scaling is off by default */
    cl->scalingFactor = 1;
	cl->screenBuffer = rfbGetFramebuffer();
    cl->scalingFrameBuffer = cl->screenBuffer;
	cl->scalingPaddedWidthInBytes = rfbScreen.paddedWidthInBytes;

    cl->tightCompressLevel = TIGHT_DEFAULT_COMPRESSION;
    cl->tightQualityLevel = -1;
    for (i = 0; i < 4; i++)
        cl->zsActive[i] = FALSE;

    cl->enableLastRectEncoding = FALSE;
    cl->enableXCursorShapeUpdates = FALSE;
    cl->useRichCursorEncoding = FALSE;
    cl->enableCursorPosUpdates = FALSE;
    cl->desktopSizeUpdate = FALSE;
    cl->immediateUpdate = FALSE;

    pthread_mutex_lock(&rfbClientListMutex);
    cl->next = rfbClientHead;
    cl->prev = NULL;
    if (rfbClientHead)
        rfbClientHead->prev = cl;

    rfbClientHead = cl;
    pthread_mutex_unlock(&rfbClientListMutex);

    rfbResetStats(cl);

    cl->compStreamInited = FALSE;
    cl->compStream.total_in = 0;
    cl->compStream.total_out = 0;
    cl->compStream.zalloc = Z_NULL;
    cl->compStream.zfree = Z_NULL;
    cl->compStream.opaque = Z_NULL;

    cl->zlibCompressLevel = 5;

    cl->compStreamRaw.total_in = ZLIBHEX_COMP_UNINITED;
    cl->compStreamHex.total_in = ZLIBHEX_COMP_UNINITED;

    cl->client_zlibBeforeBufSize = 0;
    cl->client_zlibBeforeBuf = NULL;

    cl->client_zlibAfterBufSize = 0;
    cl->client_zlibAfterBuf = NULL;
    cl->client_zlibAfterBufLen = 0;

    sprintf(pv, rfbProtocolVersionFormat, rfbProtocolMajorVersion, rfbProtocolMinorVersion);

    if (WriteExact(cl, pv, sz_rfbProtocolVersionMsg) < 0) {
        rfbLogPerror("rfbNewClient: write");
        rfbCloseClient(cl);
        return NULL;
    }
    
    // wake up screen in case monitor off (avoid blank VNC screen):
    IOPMAssertionDeclareUserActivity(CFSTR("VNC user is logged in"),
        kIOPMUserActiveLocal, &userActivityLastAssertionId);

    return cl;
}


/*
 * rfbClientConnectionGone is called from sockets.c just after a connection
 * has gone away.
 */

void rfbClientConnectionGone(rfbClientPtr cl) {
    int i;

    // RedstoneOSX - Track and release depressed modifier keys whenever the client disconnects
    keyboardReleaseKeysForClient(cl);

	freePasteboardForClient(cl);

    pthread_mutex_lock(&rfbClientListMutex);

    /* Release the compression state structures if any. */
    if ( cl->compStreamInited == TRUE ) {
        deflateEnd( &(cl->compStream) );
    }

    for (i = 0; i < 4; i++) {
        if (cl->zsActive[i])
            deflateEnd(&cl->zsStruct[i]);
    }

    if (pointerClient == cl)
        pointerClient = NULL;

    if (cl->prev)
        cl->prev->next = cl->next;
    else
        rfbClientHead = cl->next;
    if (cl->next)
        cl->next->prev = cl->prev;

    pthread_mutex_unlock(&rfbClientListMutex);

    REGION_UNINIT(pScreen,&cl->modifiedRegion);

	if (cl->major && cl->minor) {
		// If it didn't get so far as to send a protocol then let's just ignore
		// For Clients with no activity just return with no log
		rfbLog("Client %s disconnected", cl->host);
		rfbSendClientList();
		rfbPrintStats(cl);
	}

    FreeZrleData(cl);

    free(cl->host);

    if (cl->translateLookupTable)
        free(cl->translateLookupTable);

    /* SERVER SCALING EXTENSIONS */
    if( cl->scalingFactor != 1 ){
        free(cl->scalingFrameBuffer);
    }

    pthread_cond_destroy(&cl->updateCond);
    pthread_mutex_destroy(&cl->updateMutex);
    pthread_mutex_destroy(&cl->outputMutex);

    xfree(cl);
    // Not sure why but this log message seems to prevent a crash
    // rfbLog("Client gone");
}


/*
 * rfbProcessClientMessage is called when there is data to read from a client.
 */

void rfbProcessClientMessage(rfbClientPtr cl) {
    switch (cl->state) {
        case RFB_PROTOCOL_VERSION:
            rfbProcessClientProtocolVersion(cl);
            return;
        case RFB_AUTH_VERSION:
            rfbProcessAuthVersion(cl);
            return;
        case RFB_AUTHENTICATION:
            rfbAuthProcessClientMessage(cl);
            return;
        case RFB_INITIALISATION:
            rfbProcessClientInitMessage(cl);
            return;
        default:
            rfbProcessClientNormalMessage(cl);
            return;
    }
}


/*
 * rfbProcessClientProtocolVersion is called when the client sends its
 * protocol version.
 */

void rfbProcessClientProtocolVersion(rfbClientPtr cl) {
    rfbProtocolVersionMsg pv;
    int n;
    char failureReason[256];

    if ((n = ReadExact(cl, pv, sz_rfbProtocolVersionMsg)) <= 0) {
        if (n == 0)
            rfbLog("rfbProcessClientProtocolVersion: client gone");
        else
            rfbLogPerror("rfbProcessClientProtocolVersion: read");
        rfbCloseClient(cl);
        return;
    }

    pv[sz_rfbProtocolVersionMsg] = 0;
    if (sscanf(pv, rfbProtocolVersionFormat, &cl->major, &cl->minor) != 2) {
		if (strncmp(pv,"GET",3)) // Don't log if it was a browser
			rfbLog("rfbProcessClientProtocolVersion: not a valid RFB client");
        rfbCloseClient(cl);
        return;
    }
    rfbLog("Protocol version %d.%d", cl->major, cl->minor);

    if (cl->major != rfbProtocolMajorVersion) {
        /* Major version mismatch - send a ConnFailed message */
        rfbLog("Major version mismatch");
        sprintf(failureReason,
                "RFB protocol version mismatch - server %d.%d, client %d.%d",
                rfbProtocolMajorVersion,rfbProtocolMinorVersion,cl->major,cl->minor);
        rfbClientConnFailed(cl, failureReason);
        return;
    }

    if (cl->minor != rfbProtocolMinorVersion) {
        /* Minor version mismatch - warn but try to continue */
        rfbLog("Ignoring minor version mismatch");
    }

	rfbSendClientList();

    rfbAuthNewClient(cl);
}


/*
 * rfbClientConnFailed is called when a client connection has failed either
 * because it talks the wrong protocol or it has failed authentication.
 */

void rfbClientConnFailed(rfbClientPtr cl, char *reason) {
    char *buf;
    size_t len = strlen(reason);

    buf = (char *)xalloc(8 + len);
    ((CARD32 *)buf)[0] = Swap32IfLE(rfbConnFailed);
    ((CARD32 *)buf)[1] = Swap32IfLE(len);
    memcpy(buf + 8, reason, len);

    if (WriteExact(cl, buf, 8 + len) < 0)
        rfbLogPerror("rfbClientConnFailed: write");
    xfree(buf);
    rfbCloseClient(cl);
}


/*
 * rfbProcessClientInitMessage is called when the client sends its
 * initialisation message.
 */

void rfbProcessClientInitMessage(rfbClientPtr cl) {
    rfbClientInitMsg ci;
    char buf[256];
    rfbServerInitMsg *si = (rfbServerInitMsg *)buf;
    int n;
    size_t len;
    rfbClientIteratorPtr iterator;
    rfbClientPtr otherCl;

    if ((n = ReadExact(cl, (char *)&ci,sz_rfbClientInitMsg)) <= 0) {
        if (n == 0)
            rfbLog("rfbProcessClientInitMessage: client gone");
        else
            rfbLogPerror("rfbProcessClientInitMessage: read");
        rfbCloseClient(cl);
        return;
    }

    si->framebufferWidth = Swap16IfLE(rfbScreen.width);
    si->framebufferHeight = Swap16IfLE(rfbScreen.height);
    si->format = rfbServerFormat;
    si->format.redMax = Swap16IfLE(si->format.redMax);
    si->format.greenMax = Swap16IfLE(si->format.greenMax);
    si->format.blueMax = Swap16IfLE(si->format.blueMax);

    if (strlen(desktopName) > 128)      /* sanity check on desktop name len */
        desktopName[128] = 0;

    strcpy(buf + sz_rfbServerInitMsg, desktopName);
    len = strlen(buf + sz_rfbServerInitMsg);
    si->nameLength = Swap32IfLE(len);

    if (WriteExact(cl, buf, sz_rfbServerInitMsg + len) < 0) {
        rfbLogPerror("rfbProcessClientInitMessage: write");
        rfbCloseClient(cl);
        return;
    }

    cl->state = RFB_NORMAL;

    if (!cl->reverseConnection &&
        (rfbNeverShared || (!rfbAlwaysShared && !ci.shared))) {

        if (rfbDontDisconnect) {
            iterator = rfbGetClientIterator();
            while ((otherCl = rfbClientIteratorNext(iterator)) != NULL) {
                if ((otherCl != cl) && (otherCl->state == RFB_NORMAL)) {
                    rfbLog("-dontdisconnect: Not shared & existing client");
                    rfbLog("  refusing new client %s", cl->host);
                    rfbCloseClient(cl);
                    rfbReleaseClientIterator(iterator);
                    return;
                }
            }
            rfbReleaseClientIterator(iterator);
        } else {
            iterator = rfbGetClientIterator();
            while ((otherCl = rfbClientIteratorNext(iterator)) != NULL) {
                if ((otherCl != cl) && (otherCl->state == RFB_NORMAL)) {
                    rfbLog("Not shared - closing connection to client %s",
                           otherCl->host);
                    rfbCloseClient(otherCl);
                }
            }
            rfbReleaseClientIterator(iterator);
        }
    }
}


/*
 * rfbProcessClientNormalMessage is called when the client has sent a normal
 * protocol message.
 */

void rfbProcessClientNormalMessage(rfbClientPtr cl) {
    int n;
    rfbClientToServerMsg msg;
    char *str;

    if ((n = ReadExact(cl, (char *)&msg, 1)) <= 0) {
        if (n != 0)
            rfbLogPerror("rfbProcessClientNormalMessage: read");
        rfbCloseClient(cl);
        return;
    }

    switch (msg.type) {

        case rfbSetPixelFormat:
		{
            if ((n = ReadExact(cl, ((char *)&msg) + 1,
                               sz_rfbSetPixelFormatMsg - 1)) <= 0) {
                if (n != 0)
                    rfbLogPerror("rfbProcessClientNormalMessage: read");
                rfbCloseClient(cl);
                return;
            }

			if (!rfbMaxBitDepth || msg.spf.format.bitsPerPixel <= rfbMaxBitDepth) {
				cl->format.bitsPerPixel = msg.spf.format.bitsPerPixel;
				cl->format.depth = msg.spf.format.depth;
				cl->format.bigEndian = (msg.spf.format.bigEndian ? 1 : 0);
				cl->format.trueColour = (msg.spf.format.trueColour ? 1 : 0);
				cl->format.redMax = Swap16IfLE(msg.spf.format.redMax);
				cl->format.greenMax = Swap16IfLE(msg.spf.format.greenMax);
				cl->format.blueMax = Swap16IfLE(msg.spf.format.blueMax);
				cl->format.redShift = msg.spf.format.redShift;
				cl->format.greenShift = msg.spf.format.greenShift;
				cl->format.blueShift = msg.spf.format.blueShift;
				rfbSetTranslateFunction(cl);
			}
			else
				rfbLog("rfbProcessClientNormalMessage: Unable to set requested bit depth %d to greater than MaxBitDepth (%d)", msg.spf.format.bitsPerPixel, rfbMaxBitDepth);

            return;
		}

        case rfbFixColourMapEntries:
		{
            if ((n = ReadExact(cl, ((char *)&msg) + 1,
                               sz_rfbFixColourMapEntriesMsg - 1)) <= 0) {
                if (n != 0)
                    rfbLogPerror("rfbProcessClientNormalMessage: read");
                rfbCloseClient(cl);
                return;
            }
            rfbLog("rfbProcessClientNormalMessage: %s",
                   "FixColourMapEntries unsupported\n");
            rfbCloseClient(cl);
            return;
		}

        case rfbSetEncodings: {
            int i;
            CARD32 enc;

            if ((n = ReadExact(cl, ((char *)&msg) + 1,
                               sz_rfbSetEncodingsMsg - 1)) <= 0) {
                if (n != 0)
                    rfbLogPerror("rfbProcessClientNormalMessage: read");
                rfbCloseClient(cl);
                return;
            }

            msg.se.nEncodings = Swap16IfLE(msg.se.nEncodings);

            pthread_mutex_lock(&cl->updateMutex);

            // Since there is not protocol to "clear" these extensions we always clear them and expect them to be re-sent if
            // the client continues to support those options
            cl->preferredEncoding = -1;
            cl->enableLastRectEncoding = FALSE;
            cl->enableXCursorShapeUpdates = FALSE;
            cl->useRichCursorEncoding = FALSE;
            cl->enableCursorPosUpdates = FALSE;
            cl->desktopSizeUpdate = FALSE;
            cl->immediateUpdate = FALSE;

            for (i = 0; i < msg.se.nEncodings; i++) {
                if ((n = ReadExact(cl, (char *)&enc, 4)) <= 0) {
                    if (n != 0)
                        rfbLogPerror("rfbProcessClientNormalMessage: read");
                    rfbCloseClient(cl);
                    return;
                }
                enc = Swap32IfLE(enc);

                switch (enc) {
                    case rfbEncodingCopyRect:
                        break;
                    case rfbEncodingRaw:
                    case rfbEncodingRRE:
                    case rfbEncodingCoRRE:
                    case rfbEncodingHextile:
                    case rfbEncodingZlib:
                    case rfbEncodingTight:
                    case rfbEncodingZlibHex:
                    case rfbEncodingZRLE:
                        if (cl->preferredEncoding == -1) {
                            cl->preferredEncoding = enc;
                            rfbLog("ENCODING: %s for client %s", encNames[cl->preferredEncoding], cl->host);
                        }
                        break;
					case rfbEncodingUltra:
						rfbLog("\tULTRA Encoding not supported(ignored): %u (%X)", (int)enc, (int)enc);
						break;
                        /* PSEUDO_ENCODINGS */
                    case rfbEncodingLastRect:
                        rfbLog("\tEnabling LastRect protocol extension for client %s", cl->host);
                        cl->enableLastRectEncoding = TRUE;
                        break;
                    case rfbEncodingXCursor:
                        //rfbLog("Enabling XCursor protocol extension for client %s", cl->host);
                        cl->enableXCursorShapeUpdates = TRUE;
                        break;
                    case rfbEncodingRichCursor:
                        rfbLog("\tEnabling Cursor Shape protocol extension for client %s", cl->host);
                        cl->useRichCursorEncoding = TRUE;
                        cl->currentCursorSeed = 0;
                        break;
                    case rfbEncodingPointerPos:
                        rfbLog("\tEnabling Cursor Position protocol extension for client %s", cl->host);
                        cl->enableCursorPosUpdates = TRUE;
                        cl->clientCursorLocation = CGPointMake(-1.0, -1.0);
                        break;
                    case rfbEncodingDesktopResize:
                        rfbLog("\tEnabling Dynamic Desktop Sizing for client %s", cl->host);
                        cl->desktopSizeUpdate = TRUE;
                        break;
                    case rfbImmediateUpdate:
                        rfbLog("\tEnabling Immediate updates for client " "%s", cl->host);
                        cl->immediateUpdate = TRUE;
                        break;
					case rfbPasteboardRequest:
						rfbLog("\tEnabling pasteboard request " "%s", cl->host);
						cl->generalPBLastChange = -2; // This will cause it to send a single update that shows the current PB
						break;
					case rfbRichPasteboard:
						if (!rfbDisableRichClipboards) {
							rfbLog("\tEnabling rich pasteboard " "%s", cl->host);
							cl->richClipboardSupport = TRUE;
							// The -2 will already trigger force sending the PB, so we don't need to send the ack.
							if (cl->generalPBLastChange != -2)
								cl->generalPBLastChange = -3;
						}
						break;

                        // Tight encoding options
                    default:
                        if ( enc >= (CARD32)rfbEncodingCompressLevel0 &&
                             enc <= (CARD32)rfbEncodingCompressLevel9 ) {
                            cl->zlibCompressLevel = enc & 0x0F;
                            cl->tightCompressLevel = enc & 0x0F;
                            rfbLog("\tUsing compression level %d for client %s",
                                   cl->tightCompressLevel, cl->host);
                        }
                        else if ( enc >= (CARD32)rfbEncodingQualityLevel0 &&
                                    enc <= (CARD32)rfbEncodingQualityLevel9 ) {
                            cl->tightQualityLevel = enc & 0x0F;
                            rfbLog("\tUsing jpeg image quality level %d for client %s",
                                   cl->tightQualityLevel, cl->host);
                        }
                        else {
                            rfbLog("\tUnknown Encoding Type(ignored): %u (%X)", (int)enc, (int)enc);
                        }
                }
            }

            if (cl->preferredEncoding == -1) {
                cl->preferredEncoding = rfbEncodingRaw;
            }

            pthread_mutex_unlock(&cl->updateMutex);

            // Force a new update to the client
            if (rfbShouldSendNewCursor(cl) || (rfbShouldSendNewPosition(cl)))
                pthread_cond_signal(&cl->updateCond);

            return;
        }


        case rfbFramebufferUpdateRequest:
        {
            RegionRec tmpRegion;
            BoxRec box;

            if ((n = ReadExact(cl, ((char *)&msg) + 1,
                               sz_rfbFramebufferUpdateRequestMsg-1)) <= 0) {
                if (n != 0)
                    rfbLogPerror("rfbProcessClientNormalMessage: read");
                rfbCloseClient(cl);
                return;
            }

            //rfbLog("FUR: %d (%d,%d x %d,%d)", msg.fur.incremental, msg.fur.x, msg.fur.y,  msg.fur.w, msg.fur.h);

            box.x1 = Swap16IfLE(msg.fur.x)*cl->scalingFactor;
            box.y1 = Swap16IfLE(msg.fur.y)*cl->scalingFactor;
            box.x2 = (box.x1 + Swap16IfLE(msg.fur.w))*cl->scalingFactor;
            box.y2 = (box.y1 + Swap16IfLE(msg.fur.h))*cl->scalingFactor;
            SAFE_REGION_INIT(pScreen,&tmpRegion,&box,0);

            pthread_mutex_lock(&cl->updateMutex);
            REGION_UNION(pScreen, &cl->requestedRegion, &cl->requestedRegion,
                         &tmpRegion);
            if (!msg.fur.incremental) {
                REGION_UNION(pScreen,&cl->modifiedRegion,&cl->modifiedRegion,
                             &tmpRegion);
            }
            pthread_mutex_unlock(&cl->updateMutex);
            pthread_cond_signal(&cl->updateCond);
            REGION_UNINIT(pScreen,&tmpRegion);

            return;
        }

        case rfbKeyEvent:
		{
            if (!cl->disableRemoteEvents)
                cl->rfbKeyEventsRcvd++;

            if ((n = ReadExact(cl, ((char *)&msg) + 1,
                               sz_rfbKeyEventMsg - 1)) <= 0) {
                if (n != 0)
                    rfbLogPerror("rfbProcessClientNormalMessage: read");
                rfbCloseClient(cl);
                return;
            }

                if (!cl->disableRemoteEvents)
                    KbdAddEvent(msg.ke.down, (KeySym)Swap32IfLE(msg.ke.key), cl);

                return;
		}
        case rfbPointerEvent: {
            if (!cl->disableRemoteEvents)
                cl->rfbPointerEventsRcvd++;

            if ((n = ReadExact(cl, ((char *)&msg) + 1, sz_rfbPointerEventMsg - 1)) <= 0) {
                if (n != 0)
                    rfbLogPerror("rfbProcessClientNormalMessage: read");
                rfbCloseClient(cl);
                return;
            }

            if (cl->disableRemoteEvents || (pointerClient && (pointerClient != cl)))
                return;

            if (msg.pe.buttonMask == 0)
                pointerClient = NULL;
            else
                pointerClient = cl;

            PtrAddEvent(msg.pe.buttonMask,
                (Swap16IfLE(msg.pe.x)+cl->scalingFactor-1)*cl->scalingFactor,
                (Swap16IfLE(msg.pe.y)+cl->scalingFactor-1)*cl->scalingFactor,
                cl);

            return;
        }
        case rfbClientCutText: {

            if ((n = ReadExact(cl, ((char *)&msg) + 1,
                               sz_rfbClientCutTextMsg - 1)) <= 0) {
                if (n != 0)
                    rfbLogPerror("rfbProcessClientNormalMessage: read");
                rfbCloseClient(cl);
                return;
            }

			msg.cct.length = Swap32IfLE(msg.cct.length);

			str = (char *)xalloc(msg.cct.length);

			if ((n = ReadExact(cl, str, msg.cct.length)) <= 0) {
				if (n != 0)
					rfbLogPerror("rfbProcessClientNormalMessage: read");
				xfree(str);
				rfbCloseClient(cl);
				return;
			}

			if (!cl->disableRemoteEvents) {
                rfbSetCutText(cl, str, msg.cct.length);
			}

			xfree(str);
            return;
        }

        /* SERVER SCALING EXTENSIONS */
		case rfbSetScaleFactorULTRA:
		case rfbSetScaleFactor:
		{
            rfbReSizeFrameBufferMsg rsfb;
            if ((n = ReadExact(cl, ((char *)&msg) + 1,
                               sz_rfbSetScaleFactorMsg-1)) <= 0) {
                if (n != 0)
                    rfbLogPerror("rfbProcessClientNormalMessage: read");
                rfbCloseClient(cl);
                return;
            }

            if( cl->scalingFactor != msg.ssf.scale ){
				const unsigned long csh = (rfbScreen.height+msg.ssf.scale-1) / msg.ssf.scale;
				const unsigned long csw = (rfbScreen.width +msg.ssf.scale-1) / msg.ssf.scale;

				if (cl->scalingFactor != 1)
					free(cl->scalingFrameBuffer);

                cl->scalingFactor = msg.ssf.scale;

				rfbLog("Server Side Scaling: %d for client %s", msg.ssf.scale, cl->host);

				if (cl->scalingFactor == 1) {
					cl->scalingFrameBuffer = cl->screenBuffer;
					cl->scalingPaddedWidthInBytes = rfbScreen.paddedWidthInBytes;
				}
				else {
					cl->scalingFrameBuffer = malloc( csw*csh*rfbScreen.bitsPerPixel/8 );
					cl->scalingPaddedWidthInBytes = csw * rfbScreen.bitsPerPixel/8;
				}

				/* Now notify the client of the new desktop area */
				if (msg.type == rfbSetScaleFactor) {
					rsfb.type = rfbReSizeFrameBuffer;
					rsfb.desktop_w = Swap16IfLE(rfbScreen.width);
					rsfb.desktop_h = Swap16IfLE(rfbScreen.height);
					rsfb.buffer_w = Swap16IfLE(csw);
					rsfb.buffer_h = Swap16IfLE(csh);

					if (WriteExact(cl, (char *)&rsfb, sizeof(rsfb)) < 0) {
						rfbLogPerror("rfbProcessClientNormalMessage: write");
						rfbCloseClient(cl);
						return;
					}
				}
				else {
					// What does UltraVNC expect here probably just a resize event
					rfbFramebufferUpdateMsg *fu = (rfbFramebufferUpdateMsg *)cl->updateBuf;
                    fu->type = rfbFramebufferUpdate;
                    fu->nRects = Swap16IfLE(1);
                    cl->ublen = sz_rfbFramebufferUpdateMsg;

                    rfbSendScreenUpdateEncoding(cl);
				}
			}

            return;
        }

		case rfbRichClipboardAvailable:
			rfbReceiveRichClipboardAvailable(cl);
			return;

		case rfbRichClipboardRequest:
			rfbReceiveRichClipboardRequest(cl);
			return;

		case rfbRichClipboardData:
			rfbReceiveRichClipboardData(cl);
			return;

        default:
		{
            rfbLog("ERROR: Client Sent Message: unknown message type %d", msg.type);
            rfbLog("...... Closing connection to client %s", cl->host);
            rfbCloseClient(cl);
            return;
		}
    }
}

/*
 * rfbSendFramebufferUpdate - send the currently pending framebuffer update to
 * the RFB client.
 */

Bool rfbSendFramebufferUpdate(rfbClientPtr cl, RegionRec updateRegion) {
    int i;
    uint32_t nUpdateRegionRects = 0;
    Bool sendRichCursorEncoding = FALSE;
    Bool sendCursorPositionEncoding = FALSE;

    rfbFramebufferUpdateMsg *fu = (rfbFramebufferUpdateMsg *)cl->updateBuf;

    /* Now send the update */

    cl->rfbFramebufferUpdateMessagesSent++;

    if (cl->preferredEncoding == rfbEncodingCoRRE) {
        for (i = 0; i < REGION_NUM_RECTS(&updateRegion); i++) {
            int x = REGION_RECTS(&updateRegion)[i].x1;
            int y = REGION_RECTS(&updateRegion)[i].y1;
            int w = REGION_RECTS(&updateRegion)[i].x2 - x;
            int h = REGION_RECTS(&updateRegion)[i].y2 - y;
            nUpdateRegionRects += (((w-1) / cl->correMaxWidth + 1)
                                   * ((h-1) / cl->correMaxHeight + 1));
        }
    } else if (cl->preferredEncoding == rfbEncodingZlib) {
        for (i = 0; i < REGION_NUM_RECTS(&updateRegion); i++) {
            int x = REGION_RECTS(&updateRegion)[i].x1;
            int y = REGION_RECTS(&updateRegion)[i].y1;
            int w = REGION_RECTS(&updateRegion)[i].x2 - x;
            int h = REGION_RECTS(&updateRegion)[i].y2 - y;
            nUpdateRegionRects += (((h-1) / (ZLIB_MAX_SIZE( w ) / w)) + 1);
        }
    } else if (cl->preferredEncoding == rfbEncodingTight) {
        for (i = 0; i < REGION_NUM_RECTS(&updateRegion); i++) {
            int x = REGION_RECTS(&updateRegion)[i].x1;
            int y = REGION_RECTS(&updateRegion)[i].y1;
            int w = REGION_RECTS(&updateRegion)[i].x2 - x;
            int h = REGION_RECTS(&updateRegion)[i].y2 - y;
            int n = rfbNumCodedRectsTight(cl, x, y, w, h);
            if (n == 0) {
                nUpdateRegionRects = 0xFFFF;
                break;
            }
            nUpdateRegionRects += n;
        }
    } else {
        nUpdateRegionRects = REGION_NUM_RECTS(&updateRegion);
    }

    // Sometimes send the mouse cursor update also

    if (nUpdateRegionRects != 0xFFFF) {
        if (rfbShouldSendNewCursor(cl)) {
            sendRichCursorEncoding = TRUE;
            nUpdateRegionRects++;
        }
        if (rfbShouldSendNewPosition(cl)) {
            sendCursorPositionEncoding = TRUE;
            nUpdateRegionRects++;
        }
		if (cl->needNewScreenSize) {
			nUpdateRegionRects++;
		}
    }

    fu->type = rfbFramebufferUpdate;
    fu->nRects = Swap16IfLE(nUpdateRegionRects);
    cl->ublen = sz_rfbFramebufferUpdateMsg;

    // Sometimes send the mouse cursor update (this can fail with big cursors so we'll try it first
    if (sendRichCursorEncoding) {
        if (!rfbSendRichCursorUpdate(cl)) {
            // rfbLog("Error Sending Cursor"); // We'll log at the lower level if it fails and only fail a few times
            // return FALSE;  Since this is the first update we can "skip the cursor update" instead of failing the whole thing
			--nUpdateRegionRects;
			fu->nRects = Swap16IfLE(nUpdateRegionRects);
        }
    }
    if (sendCursorPositionEncoding) {
        if (!rfbSendCursorPos(cl)) {
            rfbLog("Error Sending Cursor Position");
            return FALSE;
        }

    }
	if (cl->needNewScreenSize) {
        if (rfbSendScreenUpdateEncoding(cl)) {
            cl->needNewScreenSize = FALSE;
        }
        else {
            rfbLog("Error Sending New Screen Size");
            return FALSE;
        }
    }

	cl->screenBuffer = rfbGetFramebuffer();

    for (i = 0; i < REGION_NUM_RECTS(&updateRegion); i++) {
        uint32_t x = REGION_RECTS(&updateRegion)[i].x1;
        uint32_t y = REGION_RECTS(&updateRegion)[i].y1;
        uint32_t w = REGION_RECTS(&updateRegion)[i].x2 - x;
        uint32_t h = REGION_RECTS(&updateRegion)[i].y2 - y;

		rfbGetFramebufferUpdateInRect(x,y,w,h);


		// Refresh with latest pointer (should be "read-locked" throughout here with CG but I don't see that option)
		if (cl->scalingFactor != 1)
			CopyScalingRect( cl, &x, &y, &w, &h, TRUE);
		else
			cl->scalingFrameBuffer = cl->screenBuffer;

        cl->rfbRawBytesEquivalent += (sz_rfbFramebufferUpdateRectHeader
                                      + w * (cl->format.bitsPerPixel / 8) * h);

        switch (cl->preferredEncoding) {
            case rfbEncodingRaw:
                if (!rfbSendRectEncodingRaw(cl, x, y, w, h)) {
                    return FALSE;
                }
                break;
            case rfbEncodingRRE:
                if (!rfbSendRectEncodingRRE(cl, x, y, w, h)) {
                    return FALSE;
                }
                break;
            case rfbEncodingCoRRE:
                if (!rfbSendRectEncodingCoRRE(cl, x, y, w, h)) {
                    return FALSE;
                }
                break;
            case rfbEncodingHextile:
                if (!rfbSendRectEncodingHextile(cl, x, y, w, h)) {
                    return FALSE;
                }
                break;
            case rfbEncodingZlib:
                if (!rfbSendRectEncodingZlib(cl, x, y, w, h)) {
                    return FALSE;
                }
                break;
            case rfbEncodingTight:
                if (!rfbSendRectEncodingTight(cl, x, y, w, h)) {
                    return FALSE;
                }
                break;
            case rfbEncodingZlibHex:
                if (!rfbSendRectEncodingZlibHex(cl, x, y, w, h)) {
                    return FALSE;
                }
                break;
            case rfbEncodingZRLE:
                if (!rfbSendRectEncodingZRLE(cl, x, y, w, h)) {
                    return FALSE;
                }
                break;
        }
    }

    if (nUpdateRegionRects == 0xFFFF && !rfbSendLastRectMarker(cl))
        return FALSE;

    if (!rfbSendUpdateBuf(cl))
        return FALSE;

    return TRUE;
}

Bool rfbSendScreenUpdateEncoding(rfbClientPtr cl) {
    rfbFramebufferUpdateRectHeader rect;

    if (cl->ublen + sz_rfbFramebufferUpdateRectHeader > UPDATE_BUF_SIZE) {
        if (!rfbSendUpdateBuf(cl))
            return FALSE;
    }

    rect.r.x = 0;
    rect.r.y = 0;
    rect.r.w = Swap16IfLE((rfbScreen.width +cl->scalingFactor-1) / cl->scalingFactor);
    rect.r.h = Swap16IfLE((rfbScreen.height+cl->scalingFactor-1) / cl->scalingFactor);
    rect.encoding = Swap32IfLE(rfbEncodingDesktopResize);

    memcpy(&cl->updateBuf[cl->ublen], &rect, sz_rfbFramebufferUpdateRectHeader);
    cl->ublen += sz_rfbFramebufferUpdateRectHeader;

    cl->rfbRectanglesSent[rfbStatsDesktopResize]++;
    cl->rfbBytesSent[rfbStatsDesktopResize] += sz_rfbFramebufferUpdateRectHeader;

    // Let's push this out right away
    return rfbSendUpdateBuf(cl);
}

/*
 * Send a given rectangle in raw encoding (rfbEncodingRaw).
 */

Bool rfbSendRectEncodingRaw(rfbClientPtr cl, int x, int y, int w, int h) {
    rfbFramebufferUpdateRectHeader rect;
    int nlines;
    int bytesPerLine = w * (cl->format.bitsPerPixel / 8);
    char *fbptr = (cl->scalingFrameBuffer + (cl->scalingPaddedWidthInBytes * y)
                   + (x * (rfbScreen.bitsPerPixel / 8)));

    if (cl->ublen + sz_rfbFramebufferUpdateRectHeader > UPDATE_BUF_SIZE) {
        if (!rfbSendUpdateBuf(cl))
            return FALSE;
    }

    rect.r.x = Swap16IfLE(x);
    rect.r.y = Swap16IfLE(y);
    rect.r.w = Swap16IfLE(w);
    rect.r.h = Swap16IfLE(h);
    rect.encoding = Swap32IfLE(rfbEncodingRaw);

    memcpy(&cl->updateBuf[cl->ublen], &rect, sz_rfbFramebufferUpdateRectHeader);
    cl->ublen += sz_rfbFramebufferUpdateRectHeader;

    cl->rfbRectanglesSent[rfbEncodingRaw]++;
    cl->rfbBytesSent[rfbEncodingRaw] += sz_rfbFramebufferUpdateRectHeader + bytesPerLine * h;

    nlines = (UPDATE_BUF_SIZE - cl->ublen) / bytesPerLine;

    while (TRUE) {
        if (nlines > h)
            nlines = h;

        (*cl->translateFn)(cl->translateLookupTable, &rfbServerFormat,
                           &cl->format, fbptr, &cl->updateBuf[cl->ublen],
                           cl->scalingPaddedWidthInBytes, w, nlines);

        cl->ublen += nlines * bytesPerLine;
        h -= nlines;

        if (h == 0)     /* rect fitted in buffer, do next one */
            return TRUE;

        /* buffer full - flush partial rect and do another nlines */

        if (!rfbSendUpdateBuf(cl))
            return FALSE;

        fbptr += (cl->scalingPaddedWidthInBytes * nlines);

        nlines = (UPDATE_BUF_SIZE - cl->ublen) / bytesPerLine;
        if (nlines == 0) {
            rfbLog("rfbSendRectEncodingRaw: send buffer too small for %d "
                   "bytes per line\n", bytesPerLine);
            rfbCloseClient(cl);
            return FALSE;
        }
    }
}



/*
 * Send an empty rectangle with encoding field set to value of
 * rfbEncodingLastRect to notify client that this is the last
 * rectangle in framebuffer update ("LastRect" extension of RFB
                                    * protocol).
 */

Bool rfbSendLastRectMarker(rfbClientPtr cl) {
    rfbFramebufferUpdateRectHeader rect;

    if (cl->ublen + sz_rfbFramebufferUpdateRectHeader > UPDATE_BUF_SIZE) {
        if (!rfbSendUpdateBuf(cl))
            return FALSE;
    }

    rect.encoding = Swap32IfLE(rfbEncodingLastRect);
    rect.r.x = 0;
    rect.r.y = 0;
    rect.r.w = 0;
    rect.r.h = 0;

    memcpy(&cl->updateBuf[cl->ublen], &rect, sz_rfbFramebufferUpdateRectHeader);
    cl->ublen += sz_rfbFramebufferUpdateRectHeader;

    cl->rfbLastRectMarkersSent++;
    cl->rfbLastRectBytesSent += sz_rfbFramebufferUpdateRectHeader;

    return TRUE;
}


/*
 * Send the contents of updateBuf.  Returns 1 if successful, -1 if
 * not (errno should be set).
 */

Bool rfbSendUpdateBuf(rfbClientPtr cl) {
    /*
     int i;
     for (i = 0; i < cl->ublen; i++) {
         fprintf(stderr,"%02x ",((unsigned char *)cl->updateBuf)[i]);
     }
     fprintf(stderr,"\n");
     */

    if (WriteExact(cl, cl->updateBuf, cl->ublen) < 0) {
        rfbLogPerror("rfbSendUpdateBuf: write");
        rfbCloseClient(cl);
        return FALSE;
    }

    cl->ublen = 0;
    return TRUE;
}


/*
 * rfbSendServerCutText sends a ServerCutText message to all the clients.
 */

void rfbSendServerCutText(rfbClientPtr cl, char *str, size_t len) {
    rfbServerCutTextMsg sct;

    sct.type = rfbServerCutText;
    sct.length = Swap32IfLE(len);

    if (WriteExact(cl, (char *)&sct, sz_rfbServerCutTextMsg) < 0) {
        rfbLogPerror("rfbSendServerCutText: write");
        rfbCloseClient(cl);
    }

    if (WriteExact(cl, str, len) < 0) {
        rfbLogPerror("rfbSendServerCutText: write");
        rfbCloseClient(cl);
    }
}
/*
 void
 rfbSendServerCutText(char *str, int len)
 {
     rfbClientPtr cl;
     rfbServerCutTextMsg sct;
     rfbClientIteratorPtr iterator;

     // XXX bad-- writing with client list lock held
     iterator = rfbGetClientIterator();
     while ((cl = rfbClientIteratorNext(iterator)) != NULL) {
         sct.type = rfbServerCutText;
         sct.length = Swap32IfLE(len);
         if (WriteExact(cl, (char *)&sct,
                        sz_rfbServerCutTextMsg) < 0) {
             rfbLogPerror("rfbSendServerCutText: write");
             rfbCloseClient(cl);
             continue;
         }
         if (WriteExact(cl, str, len) < 0) {
             rfbLogPerror("rfbSendServerCutText: write");
             rfbCloseClient(cl);
         }
     }
     rfbReleaseClientIterator(iterator);
 }
 */

/* SERVER SCALING EXTENSIONS */
static void CopyScalingRect(rfbClientPtr cl,
                            uint32_t* x, uint32_t* y, uint32_t* w, uint32_t* h,
                            bool bDoScaling)
{
    uint32_t cx, cy, cw, ch;
    unsigned char* srcptr;
    unsigned char* dstptr;
    unsigned char* tmpptr;
    unsigned long pixel_value=0, red, green, blue;
    unsigned long xx, yy, u, v;
    const unsigned long bytesPerPixel = rfbScreen.bitsPerPixel/8;
    const uint32_t csh = (rfbScreen.height+cl->scalingFactor-1)/ cl->scalingFactor;
    const uint32_t csw = (rfbScreen.width +cl->scalingFactor-1)/ cl->scalingFactor;

    cy = (*y) / cl->scalingFactor;
    ch = (*h+cl->scalingFactor-1) / cl->scalingFactor+1;
    cx = (*x) / cl->scalingFactor;
    cw = (*w+cl->scalingFactor-1) / cl->scalingFactor+1;

    if( cy > csh ){
        cy = csh;
    }
    if( cy + ch > csh ){
        ch = csh - cy;
    }
    if( cx > csw ){
        cx = csw;
    }
    if( cx + cw > csw ){
        cw = csw - cx;
    }

    if( bDoScaling ){
        uint32_t rx = cx * cl->scalingFactor;
        uint32_t ry = cy * cl->scalingFactor;
#if 0
        uint32_t rh = ch * cl->scalingFactor;
        uint32_t rw = cw * cl->scalingFactor;
#endif

        /* Copy and scale data from screen buffer to scaling buffer */
        srcptr = (unsigned char*)cl->screenBuffer + (ry * rfbScreen.paddedWidthInBytes ) + (rx * bytesPerPixel);
        dstptr = (unsigned char*)cl->scalingFrameBuffer+ (cy * cl->scalingPaddedWidthInBytes) + (cx * bytesPerPixel);

        if( cl->format.trueColour ) { /* Blend neighbouring pixels together */
            for( yy=0; yy < ch; yy++ ){
                for( xx=0; xx < cw; xx++ ){
                    red = green = blue = 0;
                    for( v = 0; v < (unsigned long)cl->scalingFactor; v++ ){
                        tmpptr = srcptr;
                        for( u = 0; u < (unsigned long)cl->scalingFactor; u++ ){
                            switch( bytesPerPixel ){
                            case 1:
                                pixel_value = (unsigned long)*(unsigned char* )tmpptr;
                                break;
                            case 2:
                                pixel_value = (unsigned long)*(unsigned short*)tmpptr;
                                break;
                            case 3:    /* 24bpp may cause bus error? */
                            case 4:
                                pixel_value = (unsigned long)*(unsigned long* )tmpptr;
                                break;
                            }
                            red   += (pixel_value >> rfbServerFormat.redShift  )& rfbServerFormat.redMax;
                            green += (pixel_value >> rfbServerFormat.greenShift)& rfbServerFormat.greenMax;
                            blue  += (pixel_value >> rfbServerFormat.blueShift )& rfbServerFormat.blueMax;
                            tmpptr  += rfbScreen.paddedWidthInBytes;
                        }
                        srcptr  += bytesPerPixel;
                    }
                    red   /= cl->scalingFactor * cl->scalingFactor;
                    green /= cl->scalingFactor * cl->scalingFactor;
                    blue  /= cl->scalingFactor * cl->scalingFactor;

                    pixel_value = (red   << rfbServerFormat.redShift)
                                + (green << rfbServerFormat.greenShift)
                                + (blue  << rfbServerFormat.blueShift);

                    switch( bytesPerPixel ){
                    case 1:
                        *(unsigned char* )dstptr = (unsigned char )pixel_value;
                        break;
                    case 2:
                        *(unsigned short*)dstptr = (unsigned short)pixel_value;
                        break;
                    case 3:    /* 24bpp may cause bus error? */
                    case 4:
                        *(unsigned long* )dstptr = (unsigned long )pixel_value;
                        break;
                    }
                    dstptr += bytesPerPixel;
                }
                srcptr += (rfbScreen.paddedWidthInBytes - cw * bytesPerPixel)* cl->scalingFactor;
                dstptr += cl->scalingPaddedWidthInBytes - cw * bytesPerPixel;
            }
        }else{ /* Not truecolour, so we can't blend. Just use the top-left pixel instead */
            for( yy=0; yy < ch; yy++ ){
                for( xx=0; xx < cw; xx++ ){
                    memcpy( dstptr, srcptr, bytesPerPixel);
                    srcptr += bytesPerPixel * cl->scalingFactor;
                    dstptr += bytesPerPixel;
                }
                srcptr += (rfbScreen.paddedWidthInBytes - cw * bytesPerPixel)* cl->scalingFactor;
                dstptr += cl->scalingPaddedWidthInBytes - cw * bytesPerPixel;
            }
        }
    }

    *y = cy;
    *h = ch;
    *x = cx;
    *w = cw;
}
