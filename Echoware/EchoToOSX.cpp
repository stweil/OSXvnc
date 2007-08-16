/*
 *  EchoToOSX.cpp
 *  Echoware
 *
 *  Created by Jonathan on 3/29/06.
 *  Copyright 2006 __MyCompanyName__. All rights reserved.
 *
 */

#include <time.h>
#include <pthread.h>
#include <unistd.h>

#include "EchoToOSX.h"


// Get Relative Time in milliseconds (not to be used for absolute time)
DWORD GetTickCount()
{
	struct timeval startTime;
	gettimeofday(&startTime, NULL);
	return (DWORD) ((startTime.tv_sec * 1000) + (startTime.tv_usec / 1000));
}

void *CreateThread(int securityFlags, 
				   int stackSize, 
				   long unsigned int (*start_routine)(void *), 
				   void *arg, 
				   int creationFlags, 
				   long unsigned int *lpThreadId)
{
	pthread_t newThread;
	int threadID = pthread_create(&newThread, 
								  NULL,
								  (void *(*)(void *))start_routine, 
								  arg);
	*lpThreadId = threadID;
	
	pthread_detach(newThread);
	
	return newThread;
}

bool PostThreadMessage(unsigned long , int, void *, int)
{
	// mach_msg, socket comm, shared state?
}

bool PeekMessage(void *, int,int,int, int)
{
	
}

bool ShutdownThread(void *shutdownThread, unsigned long waitTimeout, bool *shouldQuit, bool *hasQuit)
{
	if (shutdownThread)
	{	
		if (!(*hasQuit))
		{
			*shouldQuit = 1;
			
			{
				DWORD startTime = GetTickCount();
				
				while (!(*hasQuit) && (GetTickCount() - startTime < waitTimeout))
				{
					Sleep(250000);
				}
			}
		
			if (!(*hasQuit))
			{
				if (shutdownThread)
					pthread_cancel((pthread_t) shutdownThread);	
				return false;
			}
		}
	}
	
	return true;
}


