/*
 *  DirService.h
 *  Echoware
 *
 *  Created by admin on 4/11/07.
 *  Copyright 2007 __MyCompanyName__. All rights reserved.
 *
 */

#import <Cocoa/Cocoa.h>
#import <DirectoryService/DirectoryService.h>

class CDirService
{
	private:
		tDirReference dsRef;
		tDirNodeReference dsSearchNodeRef;

		tDirStatus OpenSearchNode(tDirReference inDSRef, tDirNodeReference *outNodeRef);
		tDirStatus DoPasswordAuth(tDirReference inDSRef, tDirNodeReference inNodeRef, const char *inAuthMethod,
						   const char *inRecordName, const char *inPassword);
		tDirStatus AppendStringToBuffer(tDataBufferPtr inBuffer, const char *inString, long inLength);
	public:
		CDirService();
		virtual ~CDirService();

		NSDictionary* attributeDataForNodeOfType(const char* type, 
												 const char* value, 
												 const char* attr1, 
												 const char* attr2, 
												 const char* attr3, 
												 const char* attr4, 
												 const char* attr5, 
												 const char* attr6);

		bool authenticateUser(const char* username, const char* password);
		void cleanup();
};
