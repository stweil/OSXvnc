
// This structure represents the entire state of the RFB server
// We use it for passing off to the bundles

typedef struct rfbserver {
    /*
    static const int rfbEndianTest = 0;

    ScreenRec hackScreen;
    rfbScreenInfo rfbScreen;

    char *desktopName;
    char rfbThisHost[];

    Bool rfbAlwaysShared;
    Bool rfbNeverShared;
    Bool rfbDontDisconnect;
    Bool rfbReverseMods;

    Bool rfbSwapButtons;
    Bool rfbDisableRemote;

    Bool rfbLocalBuffer;

    // sockets.c

    int rfbMaxClientWait;

     */
    
    // kbdptr.c

    CGKeyCode *keyTable;
    unsigned char *keyTableMods;
    BOOL *pressModsForKeys;

    /*
    // rfbserver.c 

    rfbClientPtr pointerClient;

    typedef struct rfbClientIterator *rfbClientIteratorPtr;


    // translate.c

    Bool *rfbEconomicTranslate;
    rfbPixelFormat *rfbServerFormat;


    // httpd.c 

    int *httpPort;
    char *httpDir;

    // auth.c

    char *rfbAuthPasswdFile;
    Bool *rfbAuthenticating;

    // tight.c

    Bool rfbTightDisableGradient;

    // stats.c 

    char* encNames[];

    // dimming.c

    Bool *rfbNoDimming;
    Bool *rfbNoSleep;
     */
} rfbserver;


