/*
 * zlibhex.c
 *
 * Routines to implement ZlibHex Encoding
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

static Bool sendZlibHex8(rfbClientPtr cl, int x, int y, int w, int h);
static Bool sendZlibHex16(rfbClientPtr cl, int x, int y, int w, int h);
static Bool sendZlibHex32(rfbClientPtr cl, int x, int y, int w, int h);


/*
 * rfbSendRectEncodingZlibHex - send a rectangle using zlibhex encoding.
 */

Bool
rfbSendRectEncodingZlibHex(cl, x, y, w, h)
    rfbClientPtr cl;
    int x, y, w, h;
{
    rfbFramebufferUpdateRectHeader rect;

    if (cl->ublen + sz_rfbFramebufferUpdateRectHeader > UPDATE_BUF_SIZE) {
	if (!rfbSendUpdateBuf(cl))
	    return FALSE;
    }

    rect.r.x = Swap16IfLE(x);
    rect.r.y = Swap16IfLE(y);
    rect.r.w = Swap16IfLE(w);
    rect.r.h = Swap16IfLE(h);
    rect.encoding = Swap32IfLE(rfbEncodingZlibHex);

    memcpy(&cl->updateBuf[cl->ublen], (char *)&rect,
	   sz_rfbFramebufferUpdateRectHeader);
    cl->ublen += sz_rfbFramebufferUpdateRectHeader;

    cl->rfbRectanglesSent[rfbEncodingZlibHex]++;
    cl->rfbBytesSent[rfbEncodingZlibHex] += sz_rfbFramebufferUpdateRectHeader;

    switch (cl->format.bitsPerPixel) {
    case 8:
	return sendZlibHex8(cl, x, y, w, h);
    case 16:
	return sendZlibHex16(cl, x, y, w, h);
    case 32:
	return sendZlibHex32(cl, x, y, w, h);
    }

    rfbLog("rfbSendRectEncodingZlibHex: bpp %d?\n", cl->format.bitsPerPixel);
    return FALSE;
}

int
zlibCompress( BYTE *from_buf,
              BYTE *to_buf,
              unsigned int length,
              unsigned int size,
              rfbClientPtr cl,
              struct z_stream_s *compressor )
{
    int previousTotalOut;
    int deflateResult;

    /* Initialize input/output buffer assignment for compressor state. */
    compressor->avail_in = length;
    compressor->next_in = from_buf;
    compressor->avail_out = size;
    compressor->next_out = to_buf;
    compressor->data_type = Z_BINARY;

    /* If necessary, the first time, initialize the compressor state. */
    if ( compressor->total_in == ZLIBHEX_COMP_UNINITED )
    {

        compressor->total_in = 0;
        compressor->total_out = 0;
        compressor->zalloc = Z_NULL;
        compressor->zfree = Z_NULL;
        compressor->opaque = Z_NULL;

        deflateResult = deflateInit2( compressor,
			              cl->zlibCompressLevel,
				      Z_DEFLATED,
				      MAX_WBITS,
				      MAX_MEM_LEVEL,
				      Z_DEFAULT_STRATEGY );
        if ( deflateResult != Z_OK )
        {
            rfbLog( "deflateInit2 returned error:%d:%s\n",
                    deflateResult,
                    compressor->msg );
            return -1;
        }

    }

    /* Record previous total output size. */
    previousTotalOut = compressor->total_out;

    /* Compress the raw data into the result buffer. */
    deflateResult = deflate( compressor, Z_SYNC_FLUSH );

    if ( deflateResult != Z_OK )
    {
        rfbLog( "deflate returned error:%d:%s\n",
                deflateResult,
                compressor->msg);
        return -1;
    }

    return compressor->total_out - previousTotalOut;
}

#define PUT_PIXEL8(pix) (cl->updateBuf[cl->ublen++] = (pix))

#define PUT_PIXEL16(pix) (cl->updateBuf[cl->ublen++] = ((char*)&(pix))[0], \
			  cl->updateBuf[cl->ublen++] = ((char*)&(pix))[1])

#define PUT_PIXEL32(pix) (cl->updateBuf[cl->ublen++] = ((char*)&(pix))[0], \
			  cl->updateBuf[cl->ublen++] = ((char*)&(pix))[1], \
			  cl->updateBuf[cl->ublen++] = ((char*)&(pix))[2], \
			  cl->updateBuf[cl->ublen++] = ((char*)&(pix))[3])


#define DEFINE_SEND_ZLIBHEX(bpp)					      \
									      \
									      \
static Bool subrectEncode##bpp(CARD##bpp *data, int w, int h, CARD##bpp bg,   \
			       CARD##bpp fg, Bool mono, rfbClientPtr cl);     \
static void testColours##bpp(CARD##bpp *data, int size, Bool *mono,	      \
			     Bool *solid, CARD##bpp *bg, CARD##bpp *fg,       \
                             rfbClientPtr cl);                                \
									      \
									      \
/*									      \
 * rfbSendZlibHex							      \
 */									      \
									      \
static Bool								      \
sendZlibHex##bpp(cl, rx, ry, rw, rh)					      \
    rfbClientPtr cl;							      \
    int rx, ry, rw, rh;							      \
{									      \
    int x, y, w, h;							      \
    int startUblen;							      \
    char *fbptr;							      \
    CARD##bpp bg = 0, fg = 0, newBg = 0, newFg = 0;			      \
    Bool mono, solid;							      \
    Bool validBg = FALSE;						      \
    Bool validFg = FALSE;						      \
    CARD##bpp clientPixelData[(16*16+2)*(bpp/8)];			      \
    int encodedBytes;							      \
    int compressedSize;							      \
    CARD16 *card16ptr;							      \
									      \
    for (y = ry; y < ry+rh; y += 16) {					      \
	for (x = rx; x < rx+rw; x += 16) {				      \
	    w = h = 16;							      \
	    if (rx+rw - x < 16)						      \
		w = rx+rw - x;						      \
	    if (ry+rh - y < 16)						      \
		h = ry+rh - y;						      \
									      \
	    if ((cl->ublen + 1 + (2 + 16*16)*(bpp/8) + 22) > UPDATE_BUF_SIZE) {   \
		if (!rfbSendUpdateBuf(cl))				      \
		    return FALSE;					      \
	    }								      \
									      \
	    fbptr = (cl->scalingFrameBuffer + (cl->scalingPaddedWidthInBytes * y) \
		     + (x * (rfbScreen.bitsPerPixel / 8)));		      \
									      \
	    (*cl->translateFn)(cl->translateLookupTable, &rfbServerFormat,    \
			       &cl->format, fbptr, (char *)clientPixelData,   \
			       cl->scalingPaddedWidthInBytes, w, h);	      \
									      \
	    startUblen = cl->ublen;						      \
	    cl->updateBuf[startUblen] = 0;					      \
	    cl->ublen++;							      \
									      \
	    testColours##bpp(clientPixelData, w * h,			      \
			     &mono, &solid, &newBg, &newFg, cl);		      \
									      \
	    if (!validBg || (newBg != bg)) {				      \
		validBg = TRUE;						      \
		bg = newBg;						      \
		cl->updateBuf[startUblen] |= rfbHextileBackgroundSpecified;	      \
		PUT_PIXEL##bpp(bg);					      \
	    }								      \
									      \
	    if (solid) {						      \
		cl->rfbBytesSent[rfbEncodingZlibHex] += cl->ublen - startUblen;   \
		continue;						      \
	    }								      \
									      \
	    cl->updateBuf[startUblen] |= rfbHextileAnySubrects;		      \
									      \
	    if (mono) {							      \
		if (!validFg || (newFg != fg)) {			      \
		    validFg = TRUE;					      \
		    fg = newFg;						      \
		    cl->updateBuf[startUblen] |= rfbHextileForegroundSpecified;   \
		    PUT_PIXEL##bpp(fg);					      \
		}							      \
	    } else {							      \
		validFg = FALSE;					      \
		cl->updateBuf[startUblen] |= rfbHextileSubrectsColoured;	      \
	    }								      \
									      \
	    if (!subrectEncode##bpp(clientPixelData, w, h, bg, fg, mono, cl)) {   \
		encodedBytes = -1;					      \
	    }								      \
	    else {							      \
		encodedBytes = cl->ublen - startUblen - 1;			      \
	    }								      \
									      \
									      \
	    if ( encodedBytes == -1 ) {					      \
		/* straight hextile failed, deal with raw data */	      \
		if ( w * h * (bpp/8) > VNC_ENCODE_ZLIBHEX_MIN_COMP_SIZE ) {   \
		   /* rectangle large enough, zlib raw data */		      \
		   validBg = FALSE;					      \
		   validFg = FALSE;					      \
		   cl->ublen = startUblen;					      \
		   cl->updateBuf[cl->ublen++] = rfbHextileZlibRaw;		      \
		   (*cl->translateFn)(cl->translateLookupTable,		      \
					&rfbServerFormat, &cl->format, fbptr, \
					(char *)clientPixelData,	      \
					cl->scalingPaddedWidthInBytes, w, h);  \
									      \
		   compressedSize = zlibCompress( (BYTE*) clientPixelData,    \
						  (BYTE*) &cl->updateBuf[cl->ublen+2], \
						  w * h * (bpp/8),	      \
						  (16*16+2)*(bpp/8)+20,	      \
						  cl,			      \
						  &(cl->compStreamRaw));      \
									      \
		    card16ptr = (CARD16*) (&cl->updateBuf[cl->ublen]);		      \
		    *card16ptr = Swap16IfLE(compressedSize);		      \
		    cl->ublen += compressedSize + 2;			      \
	    	}							      \
		else {							      \
		   /* rectangle was too small, use raw */		      \
		   validBg = FALSE;					      \
		   validFg = FALSE;					      \
		   cl->ublen = startUblen;					      \
		   cl->updateBuf[cl->ublen++] = rfbHextileRaw;			      \
		   (*cl->translateFn)(cl->translateLookupTable,		      \
					&rfbServerFormat, &cl->format, fbptr, \
					(char *)clientPixelData,	      \
					cl->scalingPaddedWidthInBytes, w, h);  \
									      \
		    /* Extra copy protects against bus errors on RISC. */     \
		    memcpy(&cl->updateBuf[cl->ublen], (char *)clientPixelData,	      \
			    w * h * (bpp/8));				      \
									      \
		    cl->ublen += w * h * (bpp/8);				      \
	    	}							      \
	    }								      \
	    else {							      \
		/* straight hextile worked, deal with hextiled data */	      \
		if ( encodedBytes > (VNC_ENCODE_ZLIBHEX_MIN_COMP_SIZE * 2)) { \
		   /* hex data large enough, zlib hex data */		      \
		   cl->ublen = startUblen;					      \
		   cl->updateBuf[cl->ublen++] |= rfbHextileZlibHex;		      \
		   memcpy( clientPixelData, &cl->updateBuf[cl->ublen], encodedBytes); \
									      \
		   compressedSize = zlibCompress( (BYTE*) clientPixelData,    \
						  (BYTE*) &cl->updateBuf[cl->ublen+2], \
						  encodedBytes,		      \
						  (16*16+2)*(bpp/8)+20,	      \
						  cl,			      \
						  &(cl->compStreamHex));      \
									      \
		    card16ptr = (CARD16*) (&cl->updateBuf[cl->ublen]);		      \
		    *card16ptr = Swap16IfLE(compressedSize);		      \
		    cl->ublen += compressedSize + 2;			      \
	    	}							      \
		   /* hex data too small, use as is */			      \
	    }								      \
									      \
	    cl->rfbBytesSent[rfbEncodingZlibHex] += cl->ublen - startUblen;	      \
	}								      \
    }									      \
									      \
    return TRUE;							      \
}

#define DEFINE_SUBRECT_ENCODE(bpp)					      \
									      \
static Bool								      \
subrectEncode##bpp(CARD##bpp *data, int w, int h, CARD##bpp bg,		      \
		   CARD##bpp fg, Bool mono, rfbClientPtr cl)		      \
{									      \
    CARD##bpp singleCL;							      \
    int x,y;								      \
    int i,j;								      \
    int hx=0,hy,vx=0,vy;						      \
    int hyflag;								      \
    CARD##bpp *seg;							      \
    CARD##bpp *line;							      \
    int hw,hh,vw,vh;							      \
    int thex,they,thew,theh;						      \
    int numsubs = 0;							      \
    int newLen;								      \
    int nSubrectsUblen;							      \
									      \
    nSubrectsUblen = cl->ublen;						      \
    cl->ublen++;								      \
									      \
    for (y=0; y<h; y++) {						      \
	line = data+(y*w);						      \
	for (x=0; x<w; x++) {						      \
	    if (line[x] != bg) {					      \
		singleCL = line[x];						      \
		hy = y-1;						      \
		hyflag = 1;						      \
		for (j=y; j<h; j++) {					      \
		    seg = data+(j*w);					      \
		    if (seg[x] != singleCL) {break;}				      \
		    i = x;						      \
		    while ((seg[i] == singleCL) && (i < w)) i += 1;		      \
		    i -= 1;						      \
		    if (j == y) vx = hx = i;				      \
		    if (i < vx) vx = i;					      \
		    if ((hyflag > 0) && (i >= hx)) {			      \
			hy += 1;					      \
		    } else {						      \
			hyflag = 0;					      \
		    }							      \
		}							      \
		vy = j-1;						      \
									      \
		/* We now have two possible subrects: (x,y,hx,hy) and	      \
		 * (x,y,vx,vy).  We'll choose the bigger of the two.	      \
		 */							      \
		hw = hx-x+1;						      \
		hh = hy-y+1;						      \
		vw = vx-x+1;						      \
		vh = vy-y+1;						      \
									      \
		thex = x;						      \
		they = y;						      \
									      \
		if ((hw*hh) > (vw*vh)) {				      \
		    thew = hw;						      \
		    theh = hh;						      \
		} else {						      \
		    thew = vw;						      \
		    theh = vh;						      \
		}							      \
									      \
		if (mono) {						      \
		    newLen = cl->ublen - nSubrectsUblen + 2;		      \
		} else {						      \
		    newLen = cl->ublen - nSubrectsUblen + bpp/8 + 2;	      \
		}							      \
									      \
		if (newLen > (w * h * (bpp/8)))				      \
		    return FALSE;					      \
									      \
		numsubs += 1;						      \
									      \
		if (!mono) PUT_PIXEL##bpp(singleCL);				      \
									      \
		cl->updateBuf[cl->ublen++] = rfbHextilePackXY(thex,they);	      \
		cl->updateBuf[cl->ublen++] = rfbHextilePackWH(thew,theh);	      \
									      \
		/*							      \
		 * Now mark the subrect as done.			      \
		 */							      \
		for (j=they; j < (they+theh); j++) {			      \
		    for (i=thex; i < (thex+thew); i++) {		      \
			data[j*w+i] = bg;				      \
		    }							      \
		}							      \
	    }								      \
	}								      \
    }									      \
									      \
    cl->updateBuf[nSubrectsUblen] = numsubs;				      \
									      \
    return TRUE;							      \
}

#define DEFINE_TEST_COLOURS(bpp)					      \
									      \
/*									      \
 * testColours() tests if there are one (solid), two (mono) or more	      \
 * colours in a tile and gets a reasonable guess at the best background	      \
 * pixel, and the foreground pixel for mono.				      \
 */									      \
									      \
static void								      \
testColours##bpp(data,size,mono,solid,bg,fg,cl)				      \
    CARD##bpp *data;							      \
    int size;								      \
    Bool *mono;								      \
    Bool *solid;							      \
    CARD##bpp *bg;							      \
    CARD##bpp *fg;							      \
    rfbClientPtr cl;   							      \
{									      \
    CARD##bpp colour1 = 0, colour2 = 0;				              \
    int n1 = 0, n2 = 0;							      \
    *mono = TRUE;							      \
    *solid = TRUE;							      \
									      \
    for (; size > 0; size--, data++) {					      \
									      \
	if (n1 == 0)							      \
	    colour1 = *data;						      \
									      \
	if (*data == colour1) {						      \
	    n1++;							      \
	    continue;							      \
	}								      \
									      \
	if (n2 == 0) {							      \
	    *solid = FALSE;						      \
	    colour2 = *data;						      \
	}								      \
									      \
	if (*data == colour2) {						      \
	    n2++;							      \
	    continue;							      \
	}								      \
									      \
	*mono = FALSE;							      \
	break;								      \
    }									      \
									      \
    if (n1 > n2) {							      \
	*bg = colour1;							      \
	*fg = colour2;							      \
    } else {								      \
	*bg = colour2;							      \
	*fg = colour1;							      \
    }									      \
}

DEFINE_SEND_ZLIBHEX(8)
DEFINE_SUBRECT_ENCODE(8)
DEFINE_TEST_COLOURS(8)
DEFINE_SEND_ZLIBHEX(16)
DEFINE_SUBRECT_ENCODE(16)
DEFINE_TEST_COLOURS(16)
DEFINE_SEND_ZLIBHEX(32)
DEFINE_SUBRECT_ENCODE(32)
DEFINE_TEST_COLOURS(32)
