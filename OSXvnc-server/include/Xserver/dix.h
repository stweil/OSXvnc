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
/* $XConsortium: dix.h /main/44 1996/12/15 21:24:57 rws $ */
/* $XFree86: xc/programs/Xserver/include/dix.h,v 3.7 1996/12/31 04:17:46 dawes Exp $ */

#ifndef DIX_H
#define DIX_H

#include "gc.h"
#include "window.h"
#include "input.h"

#define EARLIER -1
#define SAMETIME 0
#define LATER 1

#define NullClient ((ClientPtr) 0)
#define REQUEST(type) \
        register type *stuff = (type *)client->requestBuffer


#define REQUEST_SIZE_MATCH(req)\
    if ((sizeof(req) >> 2) != client->req_len)\
         return(BadLength)

#define REQUEST_AT_LEAST_SIZE(req) \
    if ((sizeof(req) >> 2) > client->req_len )\
         return(BadLength)

#define REQUEST_FIXED_SIZE(req, n)\
    if (((sizeof(req) >> 2) > client->req_len) || \
        (((sizeof(req) + (n) + 3) >> 2) != client->req_len)) \
         return(BadLength)

#define LEGAL_NEW_RESOURCE(id,client)\
    if (!LegalNewID(id,client)) \
    {\
        client->errorValue = id;\
        return(BadIDChoice);\
    }

/* XXX if you are using this macro, you are probably not generating Match
 * errors where appropriate */
#define LOOKUP_DRAWABLE(did, client)\
    ((client->lastDrawableID == did) ? \
     client->lastDrawable : (DrawablePtr)LookupDrawable(did, client))

#ifdef XCSECURITY

#define SECURITY_VERIFY_DRAWABLE(pDraw, did, client, mode)\
    if (client->lastDrawableID == did && !client->trustLevel)\
        pDraw = client->lastDrawable;\
    else \
    {\
        pDraw = (DrawablePtr) SecurityLookupIDByClass(client, did, \
                                                      RC_DRAWABLE, mode);\
        if (!pDraw) \
        {\
            client->errorValue = did; \
            return BadDrawable;\
        }\
        if (pDraw->type == UNDRAWABLE_WINDOW)\
            return BadMatch;\
    }

#define SECURITY_VERIFY_GEOMETRABLE(pDraw, did, client, mode)\
    if (client->lastDrawableID == did && !client->trustLevel)\
        pDraw = client->lastDrawable;\
    else \
    {\
        pDraw = (DrawablePtr) SecurityLookupIDByClass(client, did, \
                                                      RC_DRAWABLE, mode);\
        if (!pDraw) \
        {\
            client->errorValue = did; \
            return BadDrawable;\
        }\
    }

#define SECURITY_VERIFY_GC(pGC, rid, client, mode)\
    if (client->lastGCID == rid && !client->trustLevel)\
        pGC = client->lastGC;\
    else\
        pGC = (GC *) SecurityLookupIDByType(client, rid, RT_GC, mode);\
    if (!pGC)\
    {\
        client->errorValue = rid;\
        return (BadGC);\
    }

#define VERIFY_DRAWABLE(pDraw, did, client)\
        SECURITY_VERIFY_DRAWABLE(pDraw, did, client, SecurityUnknownAccess)

#define VERIFY_GEOMETRABLE(pDraw, did, client)\
        SECURITY_VERIFY_GEOMETRABLE(pDraw, did, client, SecurityUnknownAccess)

#define VERIFY_GC(pGC, rid, client)\
        SECURITY_VERIFY_GC(pGC, rid, client, SecurityUnknownAccess)

#else /* not XCSECURITY */

#define VERIFY_DRAWABLE(pDraw, did, client)\
    if (client->lastDrawableID == did)\
        pDraw = client->lastDrawable;\
    else \
    {\
        pDraw = (DrawablePtr) LookupIDByClass(did, RC_DRAWABLE);\
        if (!pDraw) \
        {\
            client->errorValue = did; \
            return BadDrawable;\
        }\
        if (pDraw->type == UNDRAWABLE_WINDOW)\
            return BadMatch;\
    }

#define VERIFY_GEOMETRABLE(pDraw, did, client)\
    if (client->lastDrawableID == did)\
        pDraw = client->lastDrawable;\
    else \
    {\
        pDraw = (DrawablePtr) LookupIDByClass(did, RC_DRAWABLE);\
        if (!pDraw) \
        {\
            client->errorValue = did; \
            return BadDrawable;\
        }\
    }

#define VERIFY_GC(pGC, rid, client)\
    if (client->lastGCID == rid)\
        pGC = client->lastGC;\
    else\
        pGC = (GC *)LookupIDByType(rid, RT_GC);\
    if (!pGC)\
    {\
        client->errorValue = rid;\
        return (BadGC);\
    }

#define SECURITY_VERIFY_DRAWABLE(pDraw, did, client, mode)\
        VERIFY_DRAWABLE(pDraw, did, client)

#define SECURITY_VERIFY_GEOMETRABLE(pDraw, did, client, mode)\
        VERIFY_GEOMETRABLE(pDraw, did, client)

#define SECURITY_VERIFY_GC(pGC, rid, client, mode)\
        VERIFY_GC(pGC, rid, client)

#endif /* XCSECURITY */

/*
 * We think that most hardware implementations of DBE will want
 * LookupID*(dbe_back_buffer_id) to return the window structure that the
 * id is a back buffer for.  Since both front and back buffers will
 * return the same structure, you need to be able to distinguish
 * somewhere what kind of buffer (front/back) was being asked for, so
 * that ddx can render to the right place.  That's the problem that the
 * following code solves.  Note: we couldn't embed this in the LookupID*
 * functions because the VALIDATE_DRAWABLE_AND_GC macro often circumvents
 * those functions by checking a one-element cache.  That's why we're
 * mucking with VALIDATE_DRAWABLE_AND_GC.
 *
 * If you put -DNEED_DBE_BUF_BITS into PervasiveDBEDefines, the window
 * structure will have two additional bits defined, srcBuffer and
 * dstBuffer, and their values will be maintained via the macros
 * SET_DBE_DSTBUF and SET_DBE_SRCBUF (below).  If you also
 * put -DNEED_DBE_BUF_VALIDATE into PervasiveDBEDefines, the function
 * DbeValidateBuffer will be called any time the bits change to give you
 * a chance to do some setup.  See the DBE code for more details on this
 * function.  We put in these levels of conditionality so that you can do
 * just what you need to do, and no more.  If neither of these defines
 * are used, the bits won't be there, and VALIDATE_DRAWABLE_AND_GC will
 * be unchanged.        dpw
 */

#if defined(NEED_DBE_BUF_BITS)
#define SET_DBE_DSTBUF(_pDraw, _drawID) \
        SET_DBE_BUF(_pDraw, _drawID, dstBuffer, TRUE)
#define SET_DBE_SRCBUF(_pDraw, _drawID) \
        SET_DBE_BUF(_pDraw, _drawID, srcBuffer, FALSE)
#if defined (NEED_DBE_BUF_VALIDATE)
#define SET_DBE_BUF(_pDraw, _drawID, _whichBuffer, _dstbuf) \
    if (_pDraw->type == DRAWABLE_WINDOW)\
    {\
        int thisbuf = (_pDraw->id == _drawID);\
        if (thisbuf != ((X11WindowPtr)_pDraw)->_whichBuffer)\
        {\
             ((X11WindowPtr)_pDraw)->_whichBuffer = thisbuf;\
             DbeValidateBuffer((X11WindowPtr)_pDraw, _drawID, _dstbuf);\
        }\
     }
#else /* want buffer bits, but don't need to call DbeValidateBuffer */
#define SET_DBE_BUF(_pDraw, _drawID, _whichBuffer, _dstbuf) \
    if (_pDraw->type == DRAWABLE_WINDOW)\
    {\
        ((X11WindowPtr)_pDraw)->_whichBuffer = (_pDraw->id == _drawID);\
    }
#endif /* NEED_DBE_BUF_VALIDATE */
#else /* don't want buffer bits in window */
#define SET_DBE_DSTBUF(_pDraw, _drawID) /**/
#define SET_DBE_SRCBUF(_pDraw, _drawID) /**/
#endif /* NEED_DBE_BUF_BITS */

#define VALIDATE_DRAWABLE_AND_GC(drawID, pDraw, pGC, client)\
    if ((stuff->gc == INVALID) || (client->lastGCID != stuff->gc) ||\
        (client->lastDrawableID != drawID))\
    {\
        SECURITY_VERIFY_GEOMETRABLE(pDraw, drawID, client, SecurityWriteAccess);\
        SECURITY_VERIFY_GC(pGC, stuff->gc, client, SecurityReadAccess);\
        if ((pGC->depth != pDraw->depth) ||\
            (pGC->pScreen != pDraw->pScreen))\
            return (BadMatch);\
        client->lastDrawable = pDraw;\
        client->lastDrawableID = drawID;\
        client->lastGC = pGC;\
        client->lastGCID = stuff->gc;\
    }\
    else\
    {\
        pGC = client->lastGC;\
        pDraw = client->lastDrawable;\
    }\
    SET_DBE_DSTBUF(pDraw, drawID);\
    if (pGC->serialNumber != pDraw->serialNumber)\
        ValidateGC(pDraw, pGC);


#define WriteReplyToClient(pClient, size, pReply) \
   if ((pClient)->swapped) \
      (*ReplySwapVector[((xReq *)(pClient)->requestBuffer)->reqType]) \
           (pClient, (int)(size), pReply); \
      else (void) WriteToClient(pClient, (int)(size), (char *)(pReply));

#define WriteSwappedDataToClient(pClient, size, pbuf) \
   if ((pClient)->swapped) \
      (*(pClient)->pSwapReplyFunc)(pClient, (int)(size), pbuf); \
   else (void) WriteToClient (pClient, (int)(size), (char *)(pbuf));

typedef struct _TimeStamp *TimeStampPtr;

#ifndef _XTYPEDEF_CLIENTPTR
typedef struct _Client *ClientPtr; /* also in misc.h */
#define _XTYPEDEF_CLIENTPTR
#endif

typedef struct _WorkQueue       *WorkQueuePtr;

extern ClientPtr requestingClient;
extern ClientPtr *clients;
extern ClientPtr serverClient;
extern int currentMaxClients;

#if !(defined(__alpha) || defined(__alpha__))
typedef long HWEventQueueType;
#else
typedef int HWEventQueueType;
#endif
typedef HWEventQueueType* HWEventQueuePtr;

extern HWEventQueuePtr checkForInput[2];

typedef struct _TimeStamp {
    CARD32 months;      /* really ~49.7 days */
    CARD32 milliseconds;
}           TimeStamp;

/* dispatch.c */

extern void SetInputCheck(
#if NeedFunctionPrototypes
    HWEventQueuePtr /*c0*/,
    HWEventQueuePtr /*c1*/
#endif
);

extern void CloseDownClient(
#if NeedFunctionPrototypes
    ClientPtr /*client*/
#endif
);

extern void UpdateCurrentTime(
#if NeedFunctionPrototypes
    void
#endif
);

extern void UpdateCurrentTimeIf(
#if NeedFunctionPrototypes
    void
#endif
);

extern void InitSelections(
#if NeedFunctionPrototypes
    void
#endif
);

extern void FlushClientCaches(
#if NeedFunctionPrototypes
    XID /*id*/
#endif
);

extern int dixDestroyPixmap(
#if NeedFunctionPrototypes
    pointer /*value*/,
    XID /*pid*/
#endif
);

extern void CloseDownRetainedResources(
#if NeedFunctionPrototypes
    void
#endif
);

extern void InitClient(
#if NeedFunctionPrototypes
    ClientPtr /*client*/,
    int /*i*/,
    pointer /*ospriv*/
#endif
);

extern ClientPtr NextAvailableClient(
#if NeedFunctionPrototypes
    pointer /*ospriv*/
#endif
);

extern void SendErrorToClient(
#if NeedFunctionPrototypes
    ClientPtr /*client*/,
    unsigned int /*majorCode*/,
    unsigned int /*minorCode*/,
    XID /*resId*/,
    int /*errorCode*/
#endif
);

extern void DeleteWindowFromAnySelections(
#if NeedFunctionPrototypes
    X11WindowPtr /*pWin*/
#endif
);

extern void MarkClientException(
#if NeedFunctionPrototypes
    ClientPtr /*client*/
#endif
);

extern int GetGeometry(
#if NeedFunctionPrototypes
    ClientPtr /*client*/,
    xGetGeometryReply* /* wa */
#endif
);

/* dixutils.c */

extern void CopyISOLatin1Lowered(
#if NeedFunctionPrototypes
    unsigned char * /*dest*/,
    unsigned char * /*source*/,
    int /*length*/
#endif
);

#ifdef XCSECURITY

extern X11WindowPtr SecurityLookupWindow(
#if NeedFunctionPrototypes
    XID /*rid*/,
    ClientPtr /*client*/,
    Mask /*access_mode*/
#endif
);

extern pointer SecurityLookupDrawable(
#if NeedFunctionPrototypes
    XID /*rid*/,
    ClientPtr /*client*/,
    Mask /*access_mode*/
#endif
);

extern X11WindowPtr LookupWindow(
#if NeedFunctionPrototypes
    XID /*rid*/,
    ClientPtr /*client*/
#endif
);

extern pointer LookupDrawable(
#if NeedFunctionPrototypes
    XID /*rid*/,
    ClientPtr /*client*/
#endif
);

#else

extern X11WindowPtr LookupWindow(
#if NeedFunctionPrototypes
    XID /*rid*/,
    ClientPtr /*client*/
#endif
);

extern pointer LookupDrawable(
#if NeedFunctionPrototypes
    XID /*rid*/,
    ClientPtr /*client*/
#endif
);

#define SecurityLookupWindow(rid, client, access_mode) \
        LookupWindow(rid, client)

#define SecurityLookupDrawable(rid, client, access_mode) \
        LookupDrawable(rid, client)

#endif /* XCSECURITY */

extern ClientPtr LookupClient(
#if NeedFunctionPrototypes
    XID /*rid*/,
    ClientPtr /*client*/
#endif
);

extern void NoopDDA(
#if NeedVarargsPrototypes
    void *,
    ...
#endif
);

extern int AlterSaveSetForClient(
#if NeedFunctionPrototypes
    ClientPtr /*client*/,
    X11WindowPtr /*pWin*/,
    unsigned /*mode*/
#endif
);

extern void DeleteWindowFromAnySaveSet(
#if NeedFunctionPrototypes
    X11WindowPtr /*pWin*/
#endif
);

extern void BlockHandler(
#if NeedFunctionPrototypes
    pointer /*pTimeout*/,
    pointer /*pReadmask*/
#endif
);

extern void WakeupHandler(
#if NeedFunctionPrototypes
    int /*result*/,
    pointer /*pReadmask*/
#endif
);

typedef struct timeval ** OSTimePtr;

typedef void (* BlockHandlerProcPtr)(
#if NeedNestedPrototypes
    pointer /* blockData */,
    OSTimePtr /* pTimeout */,
    pointer /* pReadmask */
#endif
);

typedef void (* WakeupHandlerProcPtr)(
#if NeedNestedPrototypes
    pointer /* blockData */,
    int /* result */,
    pointer /* pReadmask */
#endif
);

extern Bool RegisterBlockAndWakeupHandlers(
#if NeedFunctionPrototypes
    BlockHandlerProcPtr /*blockHandler*/,
    WakeupHandlerProcPtr /*wakeupHandler*/,
    pointer /*blockData*/
#endif
);

extern void RemoveBlockAndWakeupHandlers(
#if NeedFunctionPrototypes
    BlockHandlerProcPtr /*blockHandler*/,
    WakeupHandlerProcPtr /*wakeupHandler*/,
    pointer /*blockData*/
#endif
);

extern void InitBlockAndWakeupHandlers(
#if NeedFunctionPrototypes
    void
#endif
);

extern void ProcessWorkQueue(
#if NeedFunctionPrototypes
    void
#endif
);

extern Bool QueueWorkProc(
#if NeedFunctionPrototypes
    Bool (* /*function*/)(
#if NeedNestedPrototypes
        ClientPtr /*clientUnused*/,
        pointer /*closure*/
#endif
        ),
    ClientPtr /*client*/,
    pointer /*closure*/
#endif
);

typedef Bool (* ClientSleepProcPtr)(
#if NeedFunctionPrototypes
    ClientPtr /*client*/,
    pointer /*closure*/
#endif
);

extern Bool ClientSleep(
    ClientPtr /*client*/,
    ClientSleepProcPtr /* function */,
    pointer /*closure*/
);

extern Bool ClientSignal(
    ClientPtr /*client*/
);

extern void ClientWakeup(
    ClientPtr /*client*/
);

extern Bool ClientIsAsleep(
    ClientPtr /*client*/
);

/* atom.c */

extern Atom MakeAtom(
    char * /*string*/,
    unsigned /*len*/,
    Bool /*makeit*/
);

extern Bool ValidAtom(
    Atom /*atom*/
);

extern char *NameForAtom(
    Atom /*atom*/
);

extern void AtomError(
    void
);

extern void FreeAllAtoms(
    void
);

extern void InitAtoms(
    void
);

/* events.c */

extern void SetMaskForEvent(
    Mask /* mask */,
    int /* event */
);

extern Bool PointerConfinedToScreen(
    void
);

extern Bool IsParent(
    X11WindowPtr /* maybeparent */,
    X11WindowPtr /* child */
);

extern X11WindowPtr GetCurrentRootWindow(
    void
);

extern X11WindowPtr GetSpriteWindow(
    void
);

extern void GetSpritePosition(
    int * /* px */,
    int * /* py */
);

extern void NoticeEventTime(
    xEventPtr /* xE */
);

extern void EnqueueEvent(
    xEventPtr /* xE */,
    DeviceIntPtr /* device */,
    int /* count */
);

extern void ComputeFreezes(
    void
);

extern void CheckGrabForSyncs(
    DeviceIntPtr /* dev */,
    Bool /* thisMode */,
    Bool /* otherMode */
);

extern void ActivatePointerGrab(
    DeviceIntPtr /* mouse */,
    GrabPtr /* grab */,
    TimeStamp /* time */,
    Bool /* autoGrab */
);

extern void DeactivatePointerGrab(
    DeviceIntPtr /* mouse */
);

extern void ActivateKeyboardGrab(
    DeviceIntPtr /* keybd */,
    GrabPtr /* grab */,
    TimeStamp /* time */,
    Bool /* passive */
);

extern void DeactivateKeyboardGrab(
    DeviceIntPtr /* keybd */
);

extern void AllowSome(
    ClientPtr   /* client */,
    TimeStamp /* time */,
    DeviceIntPtr /* thisDev */,
    int /* newState */
);

extern void ReleaseActiveGrabs(
ClientPtr client
);

extern int DeliverEventsToWindow(
    X11WindowPtr /* pWin */,
    xEventPtr /* pEvents */,
    int /* count */,
    Mask /* filter */,
    GrabPtr /* grab */,
    int /* mskidx */
);

extern int DeliverDeviceEvents(
    X11WindowPtr /* pWin */,
    xEventPtr /* xE */,
    GrabPtr /* grab */,
    X11WindowPtr /* stopAt */,
    DeviceIntPtr /* dev */,
    int /* count */
);

extern void DefineInitialRootWindow(
    X11WindowPtr /* win */
);

extern void WindowHasNewCursor(
    X11WindowPtr /* pWin */
);

extern Bool CheckDeviceGrabs(
    DeviceIntPtr /* device */,
    xEventPtr /* xE */,
    int /* checkFirst */,
    int /* count */
);

extern void DeliverFocusedEvent(
    DeviceIntPtr /* keybd */,
    xEventPtr /* xE */,
    X11WindowPtr /* window */,
    int /* count */
);

extern void DeliverGrabbedEvent(
    xEventPtr /* xE */,
    DeviceIntPtr /* thisDev */,
    Bool /* deactivateGrab */,
    int /* count */
);

extern void RecalculateDeliverableEvents(
    X11WindowPtr /* pWin */
);

extern int OtherClientGone(
    pointer /* value */,
    XID /* id */
);

extern void DoFocusEvents(
    DeviceIntPtr /* dev */,
    X11WindowPtr /* fromWin */,
    X11WindowPtr /* toWin */,
    int /* mode */
);

extern int SetInputFocus(
    ClientPtr /* client */,
    DeviceIntPtr /* dev */,
    Window /* focusID */,
    CARD8 /* revertTo */,
    Time /* ctime */,
    Bool /* followOK */
);

extern int GrabDevice(
    ClientPtr /* client */,
    DeviceIntPtr /* dev */,
    unsigned /* this_mode */,
    unsigned /* other_mode */,
    Window /* grabWindow */,
    unsigned /* ownerEvents */,
    Time /* ctime */,
    Mask /* mask */,
    CARD8 * /* status */
);

extern void InitEvents(
    void
);

extern void DeleteWindowFromAnyEvents(
    X11WindowPtr        /* pWin */,
    Bool /* freeResources */
);

extern void CheckCursorConfinement(
    X11WindowPtr /* pWin */
);

extern Mask EventMaskForClient(
    X11WindowPtr /* pWin */,
    ClientPtr /* client */
);



extern int DeliverEvents(
    X11WindowPtr /*pWin*/,
    xEventPtr /*xE*/,
    int /*count*/,
    X11WindowPtr /*otherParent*/
);

extern void WriteEventsToClient(
    ClientPtr /*pClient*/,
    int      /*count*/,
    xEventPtr /*events*/
);

extern int TryClientEvents(
    ClientPtr /*client*/,
    xEventPtr /*pEvents*/,
    int /*count*/,
    Mask /*mask*/,
    Mask /*filter*/,
    GrabPtr /*grab*/
);

extern int EventSelectForWindow(
    X11WindowPtr /*pWin*/,
    ClientPtr /*client*/,
    Mask /*mask*/
);

extern int EventSuppressForWindow(
    X11WindowPtr /*pWin*/,
    ClientPtr /*client*/,
    Mask /*mask*/,
    Bool * /*checkOptional*/
);

extern int MaybeDeliverEventsToClient(
    X11WindowPtr /*pWin*/,
    xEventPtr /*pEvents*/,
    int /*count*/,
    Mask /*filter*/,
    ClientPtr /*dontClient*/
);

extern void WindowsRestructured(
    void
);

extern void ResetClientPrivates(
    void
);

extern int AllocateClientPrivateIndex(
    void
);

extern Bool AllocateClientPrivate(
    int /*index*/,
    unsigned /*amount*/
);

/*
 *  callback manager stuff
 */

#ifndef _XTYPEDEF_CALLBACKLISTPTR
typedef struct _CallbackList *CallbackListPtr; /* also in misc.h */
#define _XTYPEDEF_CALLBACKLISTPTR
#endif

typedef void (*CallbackProcPtr) (
    CallbackListPtr *, pointer, pointer
);

typedef Bool (*AddCallbackProcPtr) (
    CallbackListPtr *, CallbackProcPtr, pointer
);

typedef Bool (*DeleteCallbackProcPtr) (
    CallbackListPtr *, CallbackProcPtr, pointer
);

typedef void (*CallCallbacksProcPtr) (
    CallbackListPtr *, pointer
);

typedef void (*DeleteCallbackListProcPtr) (
    CallbackListPtr *
);

typedef struct _CallbackProcs {
    AddCallbackProcPtr          AddCallback;
    DeleteCallbackProcPtr       DeleteCallback;
    CallCallbacksProcPtr        CallCallbacks;
    DeleteCallbackListProcPtr   DeleteCallbackList;
} CallbackFuncsRec, *CallbackFuncsPtr;

extern Bool CreateCallbackList(
    CallbackListPtr * /*pcbl*/,
    CallbackFuncsPtr /*cbfuncs*/
);

extern Bool AddCallback(
    CallbackListPtr * /*pcbl*/,
    CallbackProcPtr /*callback*/,
    pointer /*data*/
);

extern Bool DeleteCallback(
    CallbackListPtr * /*pcbl*/,
    CallbackProcPtr /*callback*/,
    pointer /*data*/
);

extern void CallCallbacks(
    CallbackListPtr * /*pcbl*/,
    pointer /*call_data*/
);

extern void DeleteCallbackList(
    CallbackListPtr * /*pcbl*/
);

extern void InitCallbackManager(
    void
);

/*
 *  ServerGrabCallback stuff
 */

extern CallbackListPtr ServerGrabCallback;

typedef enum {SERVER_GRABBED, SERVER_UNGRABBED,
              CLIENT_PERVIOUS, CLIENT_IMPERVIOUS } ServerGrabState;

typedef struct {
    ClientPtr client;
    ServerGrabState grabstate;
} ServerGrabInfoRec;

/*
 *  EventCallback stuff
 */

extern CallbackListPtr EventCallback;

typedef struct {
    ClientPtr client;
    xEventPtr events;
    int count;
} EventInfoRec;

/*
 *  DeviceEventCallback stuff
 */

extern CallbackListPtr DeviceEventCallback;

typedef struct {
    xEventPtr events;
    int count;
} DeviceEventInfoRec;

#endif /* DIX_H */
