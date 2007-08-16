/*
 *  ServerListSynchronize.h
 *  Echoware
 *
 *  Created by admin on 2/21/07.
 *  Copyright 2007 __MyCompanyName__. All rights reserved.
 *
 */

#include <Carbon/Carbon.h>
#include "pthread.h"
#include "CritSection.h"
#import <Foundation/Foundation.h>
#include "EchoController.h"

class CServerListSynchronize
{
	public:
		CServerListSynchronize();
		~CServerListSynchronize();

		void Init();
		void Start(EchoController *echo);
		void Terminate();
		
		bool getStatusChanged();
		void setStatusChanged(bool value);
	private:
		bool m_fTerminated_Connect;
		bool m_fTerminated_Remove;
		bool m_fTerminated_Update;
		
		bool m_fTerminated;
		
		bool m_fHasQuit_Connect;
		bool m_fHasQuit_Remove;
		bool m_fHasQuit_Update;
		
		bool m_fInitialized;
		bool m_fStatusChanged;

		EchoController *echo;
		
		void* m_hManageThread_Connect;
		unsigned long m_dwManageThread_Connect;
		
		void* m_hManageThread_Remove;
		unsigned long m_dwManageThread_Remove;
		
		void* m_hManageThread_Update;
		unsigned long m_dwManageThread_Update;
		
		CCritSection m_critSection;
		CCritSection m_critSectionRemove;
		
		static unsigned long ManageThreadProc_Connect(void* lpParameter);
		static unsigned long ManageThreadProc_Remove(void* lpParameter);
		static unsigned long ManageThreadProc_Update(void* lpParameter);
};
