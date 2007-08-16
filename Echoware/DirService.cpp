/*
 *  DirService.cpp
 *  Echoware
 *
 *  Created by admin on 4/11/07.
 *  Copyright 2007 __MyCompanyName__. All rights reserved.
 *
 */

#include "DirService.h"

CDirService::CDirService()
{
	tDirStatus dsStatus;

	dsRef = 0;
	dsSearchNodeRef = 0;
		
	dsStatus = dsOpenDirService(&dsRef);
	if (dsStatus != eDSNoErr)
	{
			cleanup();
			return;
	}

	dsStatus = OpenSearchNode(dsRef, &dsSearchNodeRef);
	if (dsStatus != eDSNoErr)
	{
		cleanup();
		return;
	}		
}

CDirService::~CDirService()
{
	cleanup();
}

void CDirService::cleanup()
{
	if (dsSearchNodeRef)
		dsCloseDirNode(dsSearchNodeRef);
	dsSearchNodeRef = 0;
	if (dsRef)
		dsCloseDirService(dsRef);
	dsRef = 0;
}


NSDictionary* CDirService::attributeDataForNodeOfType(const char* type, 
												 const char* value, 
												 const char* attr1, 
												 const char* attr2, 
												 const char* attr3, 
												 const char* attr4, 
												 const char* attr5, 
												 const char* attr6)
{
	NSMutableDictionary* dict = [NSMutableDictionary dictionary];
	
	tDirStatus		dsStatus		= eDSRecordNotFound;
	tDataListPtr	pAttribsToGet	= NULL;
	tDataListPtr	pRecTypeList	= NULL;
	tDataListPtr	pRecNameList	= NULL;
	tDataBufferPtr	pSearchBuffer	= NULL;
	unsigned long	ulRecCount		= 0;	// do not limit the number of records we are expecting
	unsigned long	ulBufferSize	= 2048;	// start with a 2k buffer for any data
		
	// we will want the actual record name and the name of the node where the user resides
	pAttribsToGet = dsBuildListFromStrings( dsRef, attr1, attr2, attr3, attr4, attr5, attr6 );
	if( pAttribsToGet == NULL ) {
		dsStatus = eMemoryAllocError;
		goto cleanup;
	}
	
	// build a list to search for user record
	pRecNameList = dsBuildListFromStrings( dsRef, value, NULL );
	if( pRecNameList == NULL ) {
		dsStatus = eMemoryAllocError;
		goto cleanup;
	}
	
	// build a list of record types to search, in this case users
	pRecTypeList = dsBuildListFromStrings( dsRef, type, NULL);
	if( pRecTypeList == NULL ) {
		dsStatus = eMemoryAllocError;
		goto cleanup;
	}
	
	// allocate a working buffer, this may be grown if we receive a eDSBufferTooSmall error
	pSearchBuffer = dsDataBufferAllocate( dsRef, ulBufferSize );
	if( pSearchBuffer == NULL ) {
		dsStatus = eMemoryAllocError;
		goto cleanup;
	}
	
	// now search for the record using dsGetRecordList
	dsStatus = dsGetRecordList( dsSearchNodeRef, pSearchBuffer, pRecNameList, eDSExact, pRecTypeList, 
								pAttribsToGet, 0, &ulRecCount, NULL );
	
	// if there was not an error and we found only 1 record for this user
	if( dsStatus == eDSNoErr && ulRecCount == 1 ) {
		tAttributeListRef	dsAttributeListRef	= 0;
		tRecordEntryPtr		dsRecordEntryPtr	= 0;
		int					ii;
		
		// Get the 1st record entry from the buffer since we only expect 1 result
		dsStatus = dsGetRecordEntry( dsSearchNodeRef, pSearchBuffer, 1, &dsAttributeListRef, &dsRecordEntryPtr );
		if (dsStatus == eDSNoErr)
		{
			// loop through the attributes in the record to get the data we requested
			// all indexes with Open Directory APIs start with 1 not 0
			for (ii = 1 ; ii <= dsRecordEntryPtr->fRecordAttributeCount; ii++)
			{
				tAttributeEntryPtr		dsAttributeEntryPtr			= NULL;
				tAttributeValueEntryPtr	dsAttributeValueEntryPtr	= NULL;
				tAttributeValueListRef	dsAttributeValueListRef		= 0;
				
				// get the attribute entry from the record
				dsStatus = dsGetAttributeEntry( dsSearchNodeRef, pSearchBuffer, dsAttributeListRef, ii, 
												&dsAttributeValueListRef, &dsAttributeEntryPtr );
				
				// get the value from the attribute if we were successful at getting an entry
				if( dsStatus == eDSNoErr ) {
					int valueIndex=1;
					NSMutableArray* values = [NSMutableArray array];
					
					while( (dsStatus = dsGetAttributeValue( dsSearchNodeRef, pSearchBuffer, valueIndex, dsAttributeValueListRef, &dsAttributeValueEntryPtr )) == eDSNoErr ) {
						NSString* key   = [NSString stringWithCString: dsAttributeEntryPtr->fAttributeSignature.fBufferData encoding: NSUTF8StringEncoding];
						NSString* value = [NSString stringWithCString: dsAttributeValueEntryPtr->fAttributeValueData.fBufferData encoding: NSUTF8StringEncoding];						
						[values addObject:value];
						if ([dict objectForKey:key]==nil) {
							[dict setObject:values forKey:key];
						}
						valueIndex++;
					}
				}
				
				// close any value list references that may have been opened
				if( dsAttributeValueListRef != 0 ) {
					dsCloseAttributeValueList( dsAttributeValueListRef );
					dsAttributeValueListRef = 0;
				}
				
				// free the attribute value entry if we got an entry
				if( dsAttributeValueEntryPtr != NULL ) {
					dsDeallocAttributeValueEntry( dsRef, dsAttributeValueEntryPtr );
					dsAttributeValueEntryPtr = NULL;
				}
				
				// free the attribute entry itself as well
				if( dsAttributeEntryPtr != NULL ) {
					dsDeallocAttributeEntry( dsRef, dsAttributeEntryPtr );
					dsAttributeEntryPtr = NULL;
				}
			}
			
			// close any reference to attribute list
			if( dsAttributeListRef != 0 ) {
				dsCloseAttributeList( dsAttributeListRef );
				dsAttributeListRef = 0;
			}
			
			// deallocate the record entry
			if( dsRecordEntryPtr != NULL ) {
				dsDeallocRecordEntry( dsRef, dsRecordEntryPtr );
				dsRecordEntryPtr = NULL;
			}
		}
	} else if( dsStatus == eDSNoErr && ulRecCount > 1 ) {
		// if we have more than 1 user, then we shouldn't attempt to authenticate
		// we chose to return eDSAuthInvalidUserName as an error since we can't distinguish
		// the specific user to return
		dsStatus = eDSAuthInvalidUserName;
	}
	
cleanup:
		// if we allocated pAttribsToGet, we need to clean up
		if( pAttribsToGet != NULL ) {
			dsDataListDeallocate( dsRef, pAttribsToGet );
			
			// need to free pointer as dsDataListDeallocate does not free it, just the list items
			free( pAttribsToGet );
			pAttribsToGet = NULL;
		}
	
	// if we allocated pRecTypeList, we need to clean up
	if( pRecTypeList != NULL ) {
		dsDataListDeallocate( dsRef, pRecTypeList );
		
		// need to free pointer as dsDataListDeallocate does not free it, just the list items
		free( pRecTypeList );
		pRecTypeList = NULL;
	}
	
	// if we allocated pRecNameList, we need to clean up
	if( pRecNameList != NULL ) {
		dsDataListDeallocate( dsRef, pRecNameList );
		
		// need to free pointer as dsDataListDeallocate does not free it, just the list items
		free( pRecNameList );
		pRecNameList = NULL;
	}
	
	// if we allocated pSearchBuffer, we need to clean up
	if( pSearchBuffer != NULL ) {
		dsDataBufferDeAllocate( dsRef, pSearchBuffer );
		pSearchBuffer = NULL;
	}
	
	return dict;	
}

bool CDirService::authenticateUser(const char* username, const char* password)
{
	NSDictionary* userInfo = attributeDataForNodeOfType(kDSStdRecordTypeUsers, username, kDSNAttrRecordName, kDSNAttrMetaNodeLocation,
														NULL, NULL,	NULL, NULL);
	NSString* recordName   = [[userInfo objectForKey:[NSString stringWithCString:kDSNAttrRecordName encoding:NSUTF8StringEncoding]] objectAtIndex:0];
	NSString* nodeLocation = [[userInfo objectForKey:[NSString stringWithCString:kDSNAttrMetaNodeLocation encoding:NSUTF8StringEncoding]] objectAtIndex:0];		
	bool rc = false;
	if ([recordName length] > 0 && [nodeLocation length] > 0)
	{
		tDirNodeReference	dsUserNodeRef	= 0;
		tDirStatus			dsStatus;		
		
		tDataListPtr dsUserNodePath = dsBuildFromPath( dsRef, [nodeLocation cStringUsingEncoding:NSUTF8StringEncoding], "/" );		
		
		dsStatus = dsOpenDirNode( dsRef, dsUserNodePath, &dsUserNodeRef );
		
		if ( dsStatus == eDSNoErr )
		{
			
			// Use our Utility routine to do the authentication
			dsStatus = DoPasswordAuth( dsRef, dsUserNodeRef, kDSStdAuthNodeNativeClearTextOK, 
									   [recordName cStringUsingEncoding:NSUTF8StringEncoding], password );
			
			// Determine if successful.  There are cases where you may receive other errors
			// such as eDSAuthPasswordExpired.
			if ( dsStatus == eDSNoErr )
			{
				rc = true;
			}
		}
		
		// free the data list as it is no longer needed
		dsDataListDeallocate( dsRef, dsUserNodePath );
		free( dsUserNodePath );
		dsUserNodePath = NULL;
	}

	return rc;
}

tDirStatus CDirService::OpenSearchNode(tDirReference inDSRef, tDirNodeReference *outNodeRef)
{
	tDataBufferPtr		pWorkingBuffer	= NULL;
	tDataListPtr		pSearchNode		= NULL;
	tDirStatus			dsStatus;
	tContextData		dsContext		= NULL;
	unsigned long		ulReturnCount	= 0;
	
	// verify none of the parameters are NULL, if so return an eDSNullParameter
	if( outNodeRef == NULL || inDSRef == 0 ) {
		return eDSNullParameter;
	}
	
	// allocate a buffer to hold return information, defaulting to 4k
	pWorkingBuffer = dsDataBufferAllocate( inDSRef, 4096 );
	if( pWorkingBuffer == NULL ) {
		return eMemoryAllocError;
	}
	
	// locate the name of the search node
	dsStatus = dsFindDirNodes( inDSRef, pWorkingBuffer, NULL, eDSSearchNodeName, &ulReturnCount, &dsContext );
	if( dsStatus == eDSNoErr ) {
		// pass 1 for node index since there should only be one value
		dsStatus = dsGetDirNodeName( inDSRef, pWorkingBuffer, 1, &pSearchNode );
	}
	
	// if we ended up with a context, we should release it
	if( dsContext != NULL ) {
		dsReleaseContinueData( inDSRef, dsContext );
		dsContext = NULL;
	}
	
	// release the current working buffer
	if( pWorkingBuffer != NULL ) {
		dsDataBufferDeAllocate( inDSRef, pWorkingBuffer );
		pWorkingBuffer = NULL;
	}
	
	// open search node
	if( dsStatus == eDSNoErr && pSearchNode != NULL ) {
		dsStatus = dsOpenDirNode( inDSRef, pSearchNode, outNodeRef );
	} 
	
	// deallocate the tDataListPtr item used to locate the Search node
	if( pSearchNode != NULL ) {
		dsDataListDeallocate( inDSRef, pSearchNode );
		
		// need to free pointer as dsDataListDeallocate does not free it, just the list items
		free( pSearchNode );
		pSearchNode = NULL;
	}
	
	return dsStatus;
} // OpenSearchNode

tDirStatus CDirService::DoPasswordAuth(tDirReference inDSRef, tDirNodeReference inNodeRef, const char *inAuthMethod,
						   const char *inRecordName, const char *inPassword)
{
	tDirStatus		dsStatus		= eDSAuthFailed;
	tDataNodePtr	pAuthMethod		= NULL;
	tDataBufferPtr	pAuthStepData	= NULL;
	tDataBufferPtr	pAuthRespData	= NULL;
	tContextData	pContextData	= NULL;
	
	// if any of our parameters are NULL, return a NULL parameter
	// if a password is not set for a user, an empty string should be sent for the password
	if( inDSRef == 0 || inNodeRef == 0 || inRecordName == NULL || inPassword == NULL ) {
		return eDSNullParameter;
	}
	
	// since this is password based, we can only support password-based methods
	if( strcmp(inAuthMethod, kDSStdAuthNodeNativeNoClearText) == 0 || 
		strcmp(inAuthMethod, kDSStdAuthNodeNativeClearTextOK) == 0 ||
		strcmp(inAuthMethod, kDSStdAuthClearText) == 0 ||
		strcmp(inAuthMethod, kDSStdAuthCrypt) == 0 ) {
		
		// turn the specified method into a tDataNode
		pAuthMethod = dsDataNodeAllocateString( inDSRef, inAuthMethod );
		if( pAuthMethod == NULL ) {
			dsStatus = eMemoryAllocError;
			goto cleanup;
		}
		
		// allocate a buffer large enough to hold all the username and password plus length bytes
		pAuthStepData = dsDataBufferAllocate( inDSRef, 4 + strlen(inRecordName) + 4 + strlen(inPassword) );
		if( pAuthStepData == NULL ) {
			dsStatus = eMemoryAllocError;
			goto cleanup;
		}
		
		// allocate a buffer for the out step data even though we don't expect anything, 
		// it is a required parameter
		pAuthRespData = dsDataBufferAllocate( inDSRef, 128 );
		if( pAuthRespData == NULL ) {
			dsStatus = eMemoryAllocError;
			goto cleanup;
		}
		
		// now place the username and password into the buffer
		AppendStringToBuffer( pAuthStepData, inRecordName, strlen(inRecordName) );
		AppendStringToBuffer( pAuthStepData, inPassword, strlen(inPassword) );
		
		// attemp the authentication
		dsStatus = dsDoDirNodeAuth( inNodeRef, pAuthMethod, 1, pAuthStepData, pAuthRespData, &pContextData );
		
	} else {
		// otherwise, return a parameter error
		dsStatus = eDSAuthParameterError;
	}
	
cleanup:
		
		// release pContextData if we had continue data
		if( pContextData != NULL ) {
			dsReleaseContinueData( inDSRef, pContextData );
			pContextData = NULL;
		}
	
	// deallocate memory for pAuthRespData if it was allocated
	if( pAuthRespData != NULL ) {
		dsDataNodeDeAllocate( inDSRef, pAuthRespData );
		pAuthRespData = NULL;
	}
	
	// deallocate memory for pAuthStepData if it was allocated
	if( pAuthStepData != NULL ) {
		dsDataBufferDeAllocate( inDSRef, pAuthStepData );
		pAuthStepData = NULL;
	}
	
	// deallocate memory for pAuthMethod if it was allocated
	if( pAuthMethod != NULL ) {
		dsDataNodeDeAllocate( inDSRef, pAuthMethod );
		pAuthMethod = NULL;
	}
	
	return dsStatus;
} // DoPasswordAuth

#pragma mark Support Functions

tDirStatus CDirService::AppendStringToBuffer(tDataBufferPtr inBuffer, const char *inString, long inLength)
{
	tDirStatus	dsStatus	= eDSBufferTooSmall;
	
	// ensure neither of our parameters are NULL
	if( inString == NULL || inBuffer == NULL ) {
		return eDSNullParameter;
	}
	
	// check to see if we have enough room in the buffer for the string and the 4 byte length
	if( inBuffer->fBufferSize >= (inBuffer->fBufferLength + 4 + inLength) ) {
		
		char	*pBufferEnd = inBuffer->fBufferData + inBuffer->fBufferLength;
		
		// prepend the data with the length of the string
		bcopy( &inLength, pBufferEnd, sizeof(long) );
		pBufferEnd += sizeof( long );
		
		// now add the string to the buffer
		bcopy( inString, pBufferEnd, inLength );
		
		// increase the buffer accordingly
		inBuffer->fBufferLength += 4 + inLength;
		
		// set successful error status
		dsStatus = eDSNoErr;
	}
	
	return dsStatus;
} // AppendStringToBuffer
