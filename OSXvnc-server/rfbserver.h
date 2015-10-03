// This structure represents the entire state of the RFB server
// We use it for passing off to the bundles

typedef struct rfbserver {
	id vncServer;

	char *desktopName;
	int rfbPort;
	BOOL rfbLocalhostOnly;

	pthread_mutex_t listenerAccepting;
	pthread_cond_t listenerGotNewClient;
} rfbserver;
