
// This structure represents the entire state of the RFB server
// We use it for passing off to the bundles

typedef struct rfbserver {
	id vncServer;
	
	char *desktopName;
	int rfbPort;
	BOOL rfbLocalhostOnly;

	pthread_mutex_t listenerAccepting;
	pthread_cond_t listenerGotNewClient;

    /*
    ScreenRec hackScreen;
    rfbScreenInfo rfbScreen;
	 
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

