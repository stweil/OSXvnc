/*
 * zlib.c
 *
 * Routines to implement zlib based encoding (deflate).
 */

/*
 *  Copyright (C) 2000 Tridia Corporation.  All Rights Reserved.
 *  Copyright (C) 1999 AT&T Laboratories Cambridge.  All Rights Reserved.
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
 *
 * For the latest source code, please check:
 *
 * http://www.developVNC.org/
 *
 * or send email to feedback@developvnc.org.
 */

#include <stdio.h>
#include "rfb.h"

/*
 * rfbSendOneRectEncodingZlib - send a given rectangle using one Zlib
 *                              rectangle encoding.
 */

Bool rfbSendOneRectEncodingZlib(rfbClientPtr cl, int x, int y, int w, int h)
{
    rfbFramebufferUpdateRectHeader rect;
    rfbZlibHeader hdr;
    int deflateResult;
    int i;
    char *fbptr = (cl->scalingFrameBuffer + (cl->scalingPaddedWidthInBytes * y)
    	   + (x * (rfbScreen.bitsPerPixel / 8)));

    int maxRawSize;
    int maxCompSize;

    maxRawSize = (rfbScreen.width * rfbScreen.height
                  * (cl->format.bitsPerPixel / 8));

    if (zlibBeforeBufSize < maxRawSize) {
	zlibBeforeBufSize = maxRawSize;
	if (zlibBeforeBuf == NULL)
	    zlibBeforeBuf = (char *)xalloc(zlibBeforeBufSize);
	else
	    zlibBeforeBuf = (char *)xrealloc(zlibBeforeBuf, zlibBeforeBufSize);
    }

    /* zlib compression is not useful for very small data sets.
     * So, we just send these raw without any compression.
     */
    if (( w * h * (rfbScreen.bitsPerPixel / 8)) <
          VNC_ENCODE_ZLIB_MIN_COMP_SIZE ) {

        int result;

        /* The translation function (used also by the in raw encoding)
         * requires 4/2/1 byte alignment in the output buffer (which is
         * cl->updateBuf for the raw encoding) based on the bitsPerPixel of
         * the viewer/client.  This prevents SIGBUS errors on some
         * architectures like SPARC, PARISC...
         */
        if (( cl->format.bitsPerPixel > 8 ) &&
            ( cl->ublen % ( cl->format.bitsPerPixel / 8 )) != 0 ) {
            if (!rfbSendUpdateBuf(cl))
                return FALSE;
        }

        result = rfbSendRectEncodingRaw(cl, x, y, w, h);

        return result;

    }

    /*
     * zlib requires output buffer to be slightly larger than the input
     * buffer, in the worst case.
     */
    maxCompSize = maxRawSize + (( maxRawSize + 99 ) / 100 ) + 12;

    if (zlibAfterBufSize < maxCompSize) {
	zlibAfterBufSize = maxCompSize;
	if (zlibAfterBuf == NULL)
	    zlibAfterBuf = (char *)xalloc(zlibAfterBufSize);
	else
	    zlibAfterBuf = (char *)xrealloc(zlibAfterBuf, zlibAfterBufSize);
    }

    /*
     * Convert pixel data to client format.
     */
    (*cl->translateFn)(cl->translateLookupTable, &rfbServerFormat,
		       &cl->format, fbptr, zlibBeforeBuf,
		       cl->scalingPaddedWidthInBytes, w, h);

    cl->compStream.next_in = ( Bytef * )zlibBeforeBuf;
    cl->compStream.avail_in = w * h * (cl->format.bitsPerPixel / 8);
    cl->compStream.next_out = ( Bytef * )zlibAfterBuf;
    cl->compStream.avail_out = maxCompSize;
    cl->compStream.data_type = Z_BINARY;

    /* Initialize the deflation state. */
    if ( cl->compStreamInited == FALSE ) {

        cl->compStream.total_in = 0;
        cl->compStream.total_out = 0;
        cl->compStream.zalloc = Z_NULL;
        cl->compStream.zfree = Z_NULL;
        cl->compStream.opaque = Z_NULL;

        deflateInit2( &(cl->compStream),
                        cl->zlibCompressLevel,
                        Z_DEFLATED,
                        MAX_WBITS,
                        MAX_MEM_LEVEL,
                        Z_DEFAULT_STRATEGY );
        /* deflateInit( &(cl->compStream), Z_BEST_COMPRESSION ); */
        /* deflateInit( &(cl->compStream), Z_BEST_SPEED ); */
        cl->compStreamInited = TRUE;

    }

    size_t previousOut = cl->compStream.total_out;

    /* Perform the compression here. */
    deflateResult = deflate( &(cl->compStream), Z_SYNC_FLUSH );

    /* Find the total size of the resulting compressed data. */
    zlibAfterBufLen = cl->compStream.total_out - previousOut;

    if ( deflateResult != Z_OK ) {
        rfbLog("zlib deflation error: %s", cl->compStream.msg);
        return FALSE;
    }

    /* Note that it is not possible to switch zlib parameters based on
     * the results of the compression pass.  The reason is
     * that we rely on the compressor and decompressor states being
     * in sync.  Compressing and then discarding the results would
     * cause lose of synchronization.
     */

    /* Update statics */
    cl->rfbRectanglesSent[rfbEncodingZlib]++;
    cl->rfbBytesSent[rfbEncodingZlib] += (sz_rfbFramebufferUpdateRectHeader
					 + sz_rfbZlibHeader + zlibAfterBufLen);

    if (cl->ublen + sz_rfbFramebufferUpdateRectHeader + sz_rfbZlibHeader
	> UPDATE_BUF_SIZE)
    {
	if (!rfbSendUpdateBuf(cl))
	    return FALSE;
    }

    rect.r.x = Swap16IfLE(x);
    rect.r.y = Swap16IfLE(y);
    rect.r.w = Swap16IfLE(w);
    rect.r.h = Swap16IfLE(h);
    rect.encoding = Swap32IfLE(rfbEncodingZlib);

    memcpy(&cl->updateBuf[cl->ublen], &rect, sz_rfbFramebufferUpdateRectHeader);
    cl->ublen += sz_rfbFramebufferUpdateRectHeader;

    hdr.nBytes = Swap32IfLE(zlibAfterBufLen);

    memcpy(&cl->updateBuf[cl->ublen], &hdr, sz_rfbZlibHeader);
    cl->ublen += sz_rfbZlibHeader;

    for (i = 0; i < zlibAfterBufLen;) {

	int bytesToCopy = UPDATE_BUF_SIZE - cl->ublen;

	if (i + bytesToCopy > zlibAfterBufLen) {
	    bytesToCopy = zlibAfterBufLen - i;
	}

	memcpy(&cl->updateBuf[cl->ublen], &zlibAfterBuf[i], bytesToCopy);

	cl->ublen += bytesToCopy;
	i += bytesToCopy;

	if (cl->ublen == UPDATE_BUF_SIZE) {
	    if (!rfbSendUpdateBuf(cl))
		return FALSE;
	}
    }

    return TRUE;

}


/*
 * rfbSendRectEncodingZlib - send a given rectangle using one or more
 *                           Zlib encoding rectangles.
 */

Bool
rfbSendRectEncodingZlib(cl, x, y, w, h)
    rfbClientPtr cl;
    int x, y, w, h;
{
    int  maxLines;
    int  linesRemaining;
    rfbRectangle partialRect;

    partialRect.x = x;
    partialRect.y = y;
    partialRect.w = w;
    partialRect.h = h;

    /* Determine maximum pixel/scan lines allowed per rectangle. */
    maxLines = ( ZLIB_MAX_SIZE(w) / w );

    /* Initialize number of scan lines left to do. */
    linesRemaining = h;

    /* Loop until all work is done. */
    while ( linesRemaining > 0 ) {

        int linesToComp;

        if ( maxLines < linesRemaining )
            linesToComp = maxLines;
        else
            linesToComp = linesRemaining;

        partialRect.h = linesToComp;

        /* Encode (compress) and send the next rectangle. */
        if ( ! rfbSendOneRectEncodingZlib( cl,
                                           partialRect.x,
                                           partialRect.y,
                                           partialRect.w,
                                           partialRect.h )) {

            return FALSE;
        }

        /* Technically, flushing the buffer here is not extremely
         * efficient.  However, this improves the overall throughput
         * of the system over very slow networks.  By flushing
         * the buffer with every maximum size zlib rectangle, we
         * improve the pipelining usage of the server CPU, network,
         * and viewer CPU components.  Insuring that these components
         * are working in parallel actually improves the performance
         * seen by the user.
         * Since, zlib is most useful for slow networks, this flush
         * is appropriate for the desired behavior of the zlib encoding.
         */
        if (( cl->ublen > 0 ) &&
            ( linesToComp == maxLines )) {
            if (!rfbSendUpdateBuf(cl)) {

                return FALSE;
            }
        }

        /* Update remaining and incremental rectangle location. */
        linesRemaining -= linesToComp;
        partialRect.y += linesToComp;

    }

    return TRUE;

}


