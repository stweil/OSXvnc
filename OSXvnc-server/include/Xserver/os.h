/***********************************************************

Copyright (c) 1987  X Consortium

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
X CONSORTIUM BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of the X Consortium shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from the X Consortium.


Copyright 1987 by Digital Equipment Corporation, Maynard, Massachusetts.

                        All Rights Reserved

Permission to use, copy, modify, and distribute this software and its
documentation for any purpose and without fee is hereby granted,
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in
supporting documentation, and that the name of Digital not be
used in advertising or publicity pertaining to distribution of the
software without specific, written prior permission.

DIGITAL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
DIGITAL BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

******************************************************************/

/* $XConsortium: os.h /main/60 1996/12/15 21:25:13 rws $ */
/* $XFree86: xc/programs/Xserver/include/os.h,v 3.16.2.1 1998/01/22 10:47:13 dawes Exp $ */

#ifndef OS_H
#define OS_H
#include "misc.h"
#define ALLOCATE_LOCAL_FALLBACK(_size) Xalloc((unsigned long)(_size))
#define DEALLOCATE_LOCAL_FALLBACK(_ptr) Xfree((pointer)(_ptr))
#include "Xalloca.h"

#define NullFID ((FID) 0)

#define SCREEN_SAVER_ON   0
#define SCREEN_SAVER_OFF  1
#define SCREEN_SAVER_FORCER 2
#define SCREEN_SAVER_CYCLE  3

#ifndef MAX_REQUEST_SIZE
#define MAX_REQUEST_SIZE 65535
#endif
#ifndef MAX_BIG_REQUEST_SIZE
#define MAX_BIG_REQUEST_SIZE 1048575
#endif

typedef pointer	FID;
typedef struct _FontPathRec *FontPathPtr;
typedef struct _NewClientRec *NewClientPtr;

#define xnfalloc(size) XNFalloc((unsigned long)(size))
#define xnfrealloc(ptr, size) XNFrealloc((pointer)(ptr), (unsigned long)(size))

#define xalloc(size) Xalloc((unsigned long)(size))
#define xnfalloc(size) XNFalloc((unsigned long)(size))
#define xcalloc(_num, _size) Xcalloc((unsigned long)(_num)*(unsigned long)(_size))
#define xrealloc(ptr, size) Xrealloc((pointer)(ptr), (unsigned long)(size))
#define xnfrealloc(ptr, size) XNFrealloc((pointer)(ptr), (unsigned long)(size))
#define xfree(ptr) Xfree((pointer)(ptr))

#ifdef SCO
#include <stdio.h>
#endif
#ifndef X_NOT_STDC_ENV
#include <string.h>
#else
#ifdef SYSV
#include <string.h>
#else
#include <strings.h>
#endif
#endif

/* have to put $(SIGNAL_DEFINES) in DEFINES in Imakefile to get this right */
#ifdef SIGNALRETURNSINT
#define SIGVAL int
#else
#define SIGVAL void
#endif

extern Bool OsDelayInitColors;

extern int WaitForSomething(
    int* /*pClientsReady*/
);

#ifdef LBX
#define ReadRequestFromClient(client)   ((client)->readRequest(client))
extern int StandardReadRequestFromClient(
#if NeedFunctionPrototypes
    ClientPtr /*client*/
#endif
);
#else
extern int ReadRequestFromClient(
    ClientPtr /*client*/
);
#endif /* LBX */

extern Bool InsertFakeRequest(
    ClientPtr /*client*/,
    char* /*data*/,
    int /*count*/
);

extern int ResetCurrentRequest(
    ClientPtr /*client*/
);

extern void FlushAllOutput(
    void
);

extern void FlushIfCriticalOutputPending(
    void
);

extern void SetCriticalOutputPending(
    void
);

extern int WriteToClient(
    ClientPtr /*who*/,
    int /*count*/,
    char* /*buf*/
);

extern void ResetOsBuffers(
    void
);

extern void CreateWellKnownSockets(
    void
);

extern void ResetWellKnownSockets(
    void
);

extern XID
AuthorizationIDOfClient(
    ClientPtr /*client*/
);

extern char *ClientAuthorized(
    ClientPtr /*client*/,
    unsigned int /*proto_n*/,
    char* /*auth_proto*/,
    unsigned int /*string_n*/,
    char* /*auth_string*/
);

extern Bool EstablishNewConnections(
    ClientPtr /*clientUnused*/,
    pointer /*closure*/
);

extern void CheckConnections(
    void
);

extern void CloseDownConnection(
    ClientPtr /*client*/
);

extern int AddEnabledDevice(
    int /*fd*/
);

extern int RemoveEnabledDevice(
    int /*fd*/
);

extern int OnlyListenToOneClient(
    ClientPtr /*client*/
);

extern int ListenToAllClients(
    void
);

extern int IgnoreClient(
    ClientPtr /*client*/
);

extern int AttendClient(
    ClientPtr /*client*/
);

extern int MakeClientGrabImpervious(
    ClientPtr /*client*/
);

extern int MakeClientGrabPervious(
    ClientPtr /*client*/
);

extern void Error(
    char* /*str*/
);

extern CARD32 GetTimeInMillis(
    void
);

extern int AdjustWaitForDelay(
    pointer /*waitTime*/,
    unsigned long /*newdelay*/
);

typedef	struct _OsTimerRec *OsTimerPtr;

typedef CARD32 (*OsTimerCallback)(
    OsTimerPtr /* timer */,
    CARD32 /* time */,
    pointer /* arg */
);

extern void TimerInit(
    void
);

extern Bool TimerForce(
    OsTimerPtr /* timer */
);

#define TimerAbsolute (1<<0)
#define TimerForceOld (1<<1)

extern OsTimerPtr TimerSet(
    OsTimerPtr /* timer */,
    int /* flags */,
    CARD32 /* millis */,
    OsTimerCallback /* func */,
    pointer /* arg */
);

extern void TimerCheck(
    void
);

extern void TimerCancel(
    OsTimerPtr /* pTimer */
);

extern void TimerFree(
    OsTimerPtr /* pTimer */
);

extern SIGVAL AutoResetServer(
    int /*sig*/
);

extern SIGVAL GiveUp(
    int /*sig*/
);

extern void UseMsg(
    void
);

extern void ProcessCommandLine(
    int /*argc*/,
    char* /*argv*/[]
);

extern unsigned long *Xalloc(
    unsigned long /*amount*/
);

extern unsigned long *XNFalloc(
    unsigned long /*amount*/
);

extern unsigned long *Xcalloc(
    unsigned long /*amount*/
);

extern unsigned long *Xrealloc(
    pointer /*ptr*/,
    unsigned long /*amount*/
);

extern unsigned long *XNFrealloc(
    pointer /*ptr*/,
    unsigned long /*amount*/
);

extern void Xfree(
    pointer /*ptr*/
);

extern void OsInitAllocator(
    void
);

typedef SIGVAL (*OsSigHandlerPtr)(
    int /* sig */
);

extern OsSigHandlerPtr OsSignal(
    int /* sig */,
    OsSigHandlerPtr /* handler */
);

extern int auditTrailLevel;

extern void AuditF(
    char* /*f*/,
    ...
);

extern void FatalError(
#if NeedVarargsPrototypes
    char* /*f*/,
    ...
#endif
)
#if __GNUC__ == 2 && __GNUC_MINOR__ > 4
__attribute((noreturn))
#endif
;

extern void ErrorF(
#if NeedVarargsPrototypes
    char* /*f*/,
    ...
#endif
);

#ifdef SERVER_LOCK
extern void LockServer(
#if NeedFunctionPrototypes
    void
#endif
);

extern void UnlockServer(
#if NeedFunctionPrototypes
    void
#endif
);
#endif

extern int OsLookupColor(
#if NeedFunctionPrototypes
    int	/*screen*/,
    char * /*name*/,
    unsigned /*len*/,
    unsigned short * /*pred*/,
    unsigned short * /*pgreen*/,
    unsigned short * /*pblue*/
#endif
);

extern void OsInit(
#if NeedFunctionPrototypes
    void
#endif
);

extern void OsCleanup(
#if NeedFunctionPrototypes
    void
#endif
);

extern void OsVendorFatalError(
#if NeedFunctionPrototypes
    void
#endif
);

extern void OsVendorInit(
#if NeedFunctionPrototypes
    void
#endif
);

extern int OsInitColors(
#if NeedFunctionPrototypes
    void
#endif
);

#if !defined(WIN32) && !defined(__EMX__)
extern int System(
#if NeedFunctionPrototypes
    char *
#endif
);

extern pointer Popen(
#if NeedFunctionPrototypes
    char *,
    char *
#endif
);

extern int Pclose(
#if NeedFunctionPrototypes
    pointer
#endif
);
#else
#define System(a) system(a)
#define Popen(a,b) popen(a,b)
#define Pclose(a) pclose(a)
#endif

extern int AddHost(
#if NeedFunctionPrototypes
    ClientPtr	/*client*/,
    int         /*family*/,
    unsigned    /*length*/,
    pointer     /*pAddr*/
#endif
);

extern Bool ForEachHostInFamily (
#if NeedFunctionPrototypes
    int	    /*family*/,
    Bool    (* /*func*/ )(
#if NeedNestedPrototypes
            unsigned char * /* addr */,
            short           /* len */,
            pointer         /* closure */
#endif
            ),
    pointer /*closure*/
#endif
);

extern int RemoveHost(
#if NeedFunctionPrototypes
    ClientPtr	/*client*/,
    int         /*family*/,
    unsigned    /*length*/,
    pointer     /*pAddr*/
#endif
);

extern int GetHosts(
#if NeedFunctionPrototypes
    pointer * /*data*/,
    int	    * /*pnHosts*/,
    int	    * /*pLen*/,
    BOOL    * /*pEnabled*/
#endif
);

typedef struct sockaddr * sockaddrPtr;

extern int InvalidHost(
#if NeedFunctionPrototypes
    sockaddrPtr /*saddr*/,
    int		/*len*/
#endif
);

extern int LocalClient(
#if NeedFunctionPrototypes
    ClientPtr /* client */
#endif
);

extern int ChangeAccessControl(
#if NeedFunctionPrototypes
    ClientPtr /*client*/,
    int /*fEnabled*/
#endif
);

extern int GetAccessControl(
#if NeedFunctionPrototypes
    void
#endif
);


extern void AddLocalHosts(
#if NeedFunctionPrototypes
    void
#endif
);

extern void ResetHosts(
#if NeedFunctionPrototypes
    char *display
#endif
);

extern void EnableLocalHost(
#if NeedFunctionPrototypes
    void
#endif
);

extern void DisableLocalHost(
#if NeedFunctionPrototypes
    void
#endif
);

extern void AccessUsingXdmcp(
#if NeedFunctionPrototypes
    void
#endif
);

extern void DefineSelf(
#if NeedFunctionPrototypes
    int /*fd*/
#endif
);

extern void AugmentSelf(
#if NeedFunctionPrototypes
    pointer /*from*/,
    int /*len*/
#endif
);

extern void InitAuthorization(
#if NeedFunctionPrototypes
    char * /*filename*/
#endif
);

extern int LoadAuthorization(
#if NeedFunctionPrototypes
    void
#endif
);

extern void RegisterAuthorizations(
#if NeedFunctionPrototypes
    void
#endif
);

extern XID CheckAuthorization(
#if NeedFunctionPrototypes
    unsigned int /*namelength*/,
    char * /*name*/,
    unsigned int /*datalength*/,
    char * /*data*/,
    ClientPtr /*client*/,
    char ** /*reason*/
#endif
);

extern void ResetAuthorization(
#if NeedFunctionPrototypes
    void
#endif
);

extern int AddAuthorization(
#if NeedFunctionPrototypes
    unsigned int /*name_length*/,
    char * /*name*/,
    unsigned int /*data_length*/,
    char * /*data*/
#endif
);

extern XID GenerateAuthorization(
#if NeedFunctionPrototypes
    unsigned int   /* name_length */,
    char	*  /* name */,
    unsigned int   /* data_length */,
    char	*  /* data */,
    unsigned int * /* data_length_return */,
    char	** /* data_return */
#endif
);

extern void ExpandCommandLine(
#if NeedFunctionPrototypes
    int * /*pargc*/,
    char *** /*pargv*/
#endif
);

extern int ddxProcessArgument(
#if NeedFunctionPrototypes
    int /*argc*/,
    char * /*argv*/ [],
    int /*i*/
#endif
);

/*
 *  idiom processing stuff
 */

xReqPtr PeekNextRequest(
#if NeedFunctionPrototypes
    xReqPtr req, ClientPtr client, Bool readmore
#endif
);

void SkipRequests(
#if NeedFunctionPrototypes
    xReqPtr req, ClientPtr client, int numskipped
#endif
);

/* int ReqLen(xReq *req, ClientPtr client)
 * Given a pointer to a *complete* request, return its length in bytes.
 * Note that if the request is a big request (as defined in the Big
 * Requests extension), the macro lies by returning 4 less than the
 * length that it actually occupies in the request buffer.  This is so you
 * can blindly compare the length with the various sz_<request> constants
 * in Xproto.h without having to know/care about big requests.
 */
#define ReqLen(_pxReq, _client) \
 ((_pxReq->length ? \
     (_client->swapped ? lswaps(_pxReq->length) : _pxReq->length) \
  : ((_client->swapped ? \
	lswapl(((CARD32*)_pxReq)[1]) : ((CARD32*)_pxReq)[1])-1) \
  ) << 2)

/* otherReqTypePtr CastxReq(xReq *req, otherReqTypePtr)
 * Cast the given request to one of type otherReqTypePtr to access
 * fields beyond the length field.
 */
#define CastxReq(_pxReq, otherReqTypePtr) \
    (_pxReq->length ? (otherReqTypePtr)_pxReq \
		    : (otherReqTypePtr)(((CARD32*)_pxReq)+1))

/* stuff for SkippedRequestsCallback */
extern CallbackListPtr SkippedRequestsCallback;
typedef struct {
    xReqPtr req;
    ClientPtr client;
    int numskipped;
} SkippedRequestInfoRec;

/* stuff for ReplyCallback */
extern CallbackListPtr ReplyCallback;
typedef struct {
    ClientPtr client;
    pointer replyData;
    unsigned long dataLenBytes;
    unsigned long bytesRemaining;
    Bool startOfReply;
} ReplyInfoRec;

/* stuff for FlushCallback */
extern CallbackListPtr FlushCallback;

#endif /* OS_H */
