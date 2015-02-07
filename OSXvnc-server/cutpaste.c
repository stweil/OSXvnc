/*
 * cutpaste.c - routines to deal with cut & paste buffers / selection.
 */

/*
 *  OSXvnc Copyright (C) 2001 Dan McGuirk <mcguirk@incompleteness.net>.
 *  Original Xvnc code Copyright (C) 1999 AT&T Laboratories Cambridge.  
 *  All Rights Reserved.
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
 */

#import <Cocoa/Cocoa.h>

#include <stdio.h>
#include <pthread.h>
#include "rfb.h"
#include "getMACAddress.h"

// This prevents us from asking if this represents a local file
#define VineRemoteProxy @"VineRemoteProxy"

#define CorePasteboardFlavor_furl @"CorePasteboardFlavorType 0x6675726C"
#define CorePasteboardFlavor_icns @"CorePasteboardFlavorType 0x69636E73"
#define CorePasteboardFlavor_ut16 @"CorePasteboardFlavorType 0x75743136"
#define CorePasteboardFlavor_ustl @"CorePasteboardFlavorType 0x7573746C"
#define CorePasteboardFlavor_TEXT @"CorePasteboardFlavorType 0x54455854"
#define CorePasteboardFlavor_styl @"CorePasteboardFlavorType 0x7374796C"
#define CorePasteboardFlavor_fccc @"CorePasteboardFlavorType 0x66636363"
#define CorePasteboardFlavor_flst @"CorePasteboardFlavorType 0xC46C7374"

#ifndef NSAppKitVersionNumber10_3
#define NSAppKitVersionNumber10_3 743
#endif

@interface NSFileManager (RSFileManagerAdditions)

- (BOOL)ensureDirectoryAtPath:(NSString *)path attributes:(NSDictionary *)attributes;

@end

@implementation NSFileManager (RSFileManagerAdditions)

- (BOOL)ensureDirectoryAtPath:(NSString *)path attributes:(NSDictionary *)attributes {
	NSString *parentDirectory;
	BOOL isDir = NO;
	path = [path stringByStandardizingPath];
	if ([self fileExistsAtPath:path isDirectory:&isDir]) {
		if (!isDir && [path isEqualToString:@"/tmp"]) return YES; // work around craziness
		return isDir; // if it's already there, we succeed iff it's a directory
	}
	parentDirectory = [path stringByDeletingLastPathComponent];
	if (parentDirectory && ![parentDirectory isEqualToString:@""]) {
		BOOL parentExists = [self fileExistsAtPath:parentDirectory isDirectory:&isDir];
		if (parentExists || [self ensureDirectoryAtPath:parentDirectory attributes:attributes]) {
			return [self createDirectoryAtPath:path withIntermediateDirectories: NO attributes:attributes error:NULL];
		} else {
			return NO; // failed to find or create parent directory
		}
	} else {
		return NO; // failed to create parent directory
	}
}

@end

@interface NSData (RSDataAdditions)

- (unsigned int) locationOfData:(NSData *)target startingAtLocation:(unsigned int)startLoc;
- (NSMutableArray *) dataComponentsSeparatedByData:(NSData *)separator;

@end
	
@implementation NSData (RSDataAdditions)

- (unsigned int) locationOfData:(NSData *)target startingAtLocation:(unsigned int)startLoc {
	char *myBytes = (char *)[self bytes];
	char *targetBytes = (char *)[target bytes];
	unsigned int targetLen = [target length];
	unsigned int loc, inner;
	for (loc = startLoc; loc+targetLen < [self length]; loc++) {
		BOOL found = YES; // assume we will match at this loc until disproven
		for (inner = 0; found && inner < targetLen; inner++) {
			if (*(myBytes+loc+inner) != *(targetBytes+inner))
				found = NO; // nope
		}
		if (found)
			return loc;
	}
	return NSNotFound;
}

- (NSMutableArray *) dataComponentsSeparatedByData:(NSData *)separator {
	NSMutableArray *array = [NSMutableArray array];
	unsigned int prevLoc = 0;
	unsigned int nextLoc = [self locationOfData:separator startingAtLocation:0];
	while (nextLoc != NSNotFound) {
		[array addObject:[self subdataWithRange:NSMakeRange(prevLoc, nextLoc - prevLoc)]];
		prevLoc = nextLoc + [separator length];
		nextLoc = [self locationOfData:separator startingAtLocation:prevLoc];
	}
	if (prevLoc <= [self length]) // add the final section (use '<=' to get an empty data if separator was found at the end)
		[array addObject:[self subdataWithRange:NSMakeRange(prevLoc, [self length] - prevLoc)]];
	return array;
}

@end


@interface NSClipboardProxy : NSObject {
	rfbClientPtr clientPointer;
	NSString *pasteboardName;
}

- initWithClientPtr: (rfbClientPtr) cl;
- (void) setPasteboardName: pbName;
- (void) removeClient;
- (void) setupTypes: (NSArray *) availableTypes;
- (void) pasteboard:(NSPasteboard *)thePasteboard provideDataForType:(NSString *)type;

@end


static BOOL debugPB = NO;

@implementation NSClipboardProxy

- initWithClientPtr: (rfbClientPtr) cl {
	if (self = [super init]) {
		clientPointer = cl;
	}
	return self;
}

- (void) setPasteboardName: pbName {
	[pasteboardName release];
	pasteboardName = [pbName retain];
}

- (void) setupTypes:(NSArray *) availableTypes {
	NSPasteboard *thePasteboard = [NSPasteboard pasteboardWithName:pasteboardName];
	if (debugPB)
		NSLog(@"Registering Types With Pasteboard");
	int newChangeCount = [thePasteboard declareTypes:[availableTypes arrayByAddingObject:VineRemoteProxy] owner:self];
	// Don't send it back to the same client via rich clipboards
	pthread_mutex_lock(&clientPointer->updateMutex);
	[(NSMutableDictionary *)clientPointer->richClipboardChangeCounts setObject:[NSNumber numberWithInt:newChangeCount] forKey:pasteboardName]; 
	pthread_mutex_unlock(&clientPointer->updateMutex);
}

// Sent by the PB system when someone requests the data (now time to ask for it)
// Happens in the main thread, should setup the data to be written, alert client send thread, wait for response and return it
- (void)pasteboard:(NSPasteboard *)thePasteboard provideDataForType:(NSString *)type {
	time_t startTime=time(NULL);

	if (!clientPointer)
		return;

	pthread_mutex_lock(&clientPointer->updateMutex);
	
	// Notify sending thread of request
	clientPointer->richClipboardReceivedName = [[thePasteboard name] retain];
	clientPointer->richClipboardReceivedType = [type retain];

	pthread_mutex_unlock(&clientPointer->updateMutex);
	pthread_cond_signal(&clientPointer->updateCond);
	// which will call rfbSendRichClipboardRequest 

	// Wait for flag indicating returned data
	while (clientPointer && !clientPointer->richClipboardReceivedNSData && (time(NULL) - startTime < 60)) { // 60 second timeout
		usleep(.10 * 1000000);
	}
	if (!clientPointer)
		return;
	
	if (clientPointer->richClipboardReceivedChangeCount >= 0) {
		NSString *availableType = clientPointer->richClipboardReceivedType;
		NSData *pasteboardData = clientPointer->richClipboardReceivedNSData;
		
		if ([availableType hasPrefix:@"RSFileWrapper:"]) { // special file data
			availableType = [availableType substringFromIndex:14];
			if ([availableType isEqualToString:NSFileContentsPboardType]) {
						[thePasteboard setData:pasteboardData forType:availableType];
				//clipBoardReceivedChangeCount = lastChangeCount;
			} 
			else { // not FileContents
				NSFileWrapper *theWrapper = [[NSFileWrapper alloc] initWithSerializedRepresentation:pasteboardData];
				// create path to file(s) in /tmp
				NSFileManager *fileManager = [NSFileManager defaultManager];
				if (!clientPointer->receivedFileTempFolder) {
					clientPointer->receivedFileTempFolder = [[@"/tmp/Vine-" stringByAppendingString:[[NSProcessInfo processInfo] globallyUniqueString]] retain];
					[fileManager ensureDirectoryAtPath:clientPointer->receivedFileTempFolder attributes:nil];
				}
				// write the files
				NSString *filename = [theWrapper preferredFilename];
				BOOL setOfFiles = [filename isEqualToString:@"<Redstone set of files>"];
				if (setOfFiles) {
					filename = [@"Files-" stringByAppendingString:[[NSProcessInfo processInfo] globallyUniqueString]];
				}
				filename = [(NSString *)clientPointer->receivedFileTempFolder stringByAppendingPathComponent:filename]; // get full path
				// set pasteboard data to point to the files
				if (setOfFiles) {
					// got a set of files -- put their local names on PB as NSFilenamesPboardType
					NSMutableArray *filenames = [NSMutableArray array];
					if (floor(NSAppKitVersionNumber) > floor(NSAppKitVersionNumber10_3)) { // Tiger+
						NSDictionary *filesDict = [theWrapper fileWrappers];
						NSEnumerator *fileEnum = [filesDict keyEnumerator];
						NSString *aFilename;
						while (aFilename = [fileEnum nextObject]) {
							[filenames addObject:[filename stringByAppendingPathComponent:aFilename]];
						}
						[theWrapper writeToFile:filename atomically:NO updateFilenames:NO];
						filename = [filenames objectAtIndex:0]; // this is needed for setting the URL later (must be set to one of the files, not the folder)
					} else {
						// for 10.2 and 10.3, can't receive multiple files -- put them in a folder
						static int batchCounter = 0;
						filename = [filename stringByAppendingPathComponent:[NSString stringWithFormat:@"CopiedFiles_%02d",++batchCounter]];
						[fileManager ensureDirectoryAtPath:filename attributes:nil];
						[filenames addObject:filename];
						//[theWrapper writeToFile:filename atomically:NO updateFilenames:NO];
						// let's avoid saving the extra .tiff files that seem to get written for each file
						NSDictionary *filesDict = [theWrapper fileWrappers];
						NSEnumerator *fileEnum = [filesDict keyEnumerator];
						NSString *aFilename;
						while (aFilename = [fileEnum nextObject]) {
							NSFileWrapper *singleItemWrapper = [filesDict objectForKey:aFilename];
							NSString *singleItemPath = [filename stringByAppendingPathComponent:aFilename];
							[singleItemWrapper writeToFile:singleItemPath atomically:NO updateFilenames:NO];
						}
					}
					[thePasteboard setPropertyList:filenames forType:NSFilenamesPboardType];
					//clipBoardReceivedChangeCount = lastChangeCount;
				} else if ([availableType isEqualToString:NSFilenamesPboardType]) { // Filenames type sent, but only got 1 file
					[theWrapper writeToFile:filename atomically:NO updateFilenames:YES];
					[thePasteboard setPropertyList:[NSArray arrayWithObject:filename] forType:NSFilenamesPboardType];
					//clipBoardReceivedChangeCount = lastChangeCount;
				} else {
					[theWrapper writeToFile:filename atomically:NO updateFilenames:YES];
				}
				if ([availableType isEqualToString:NSURLPboardType] || [availableType isEqualToString:CorePasteboardFlavor_furl]) {
					// got a URL type -- so put a URL on the PB, even if we already supplied file names
					NSURL *theUrl = [NSURL fileURLWithPath:filename];
						//[theUrl writeToPasteboard:thePasteboard];
						NSData *theData = [[theUrl absoluteString] dataUsingEncoding:NSUTF8StringEncoding];
								[thePasteboard setData:theData forType:availableType];
						//clipBoardReceivedChangeCount = lastChangeCount;
					} else if (![availableType isEqualToString:NSFilenamesPboardType]) {
						// ERROR
						NSLog(@"Error: received data type: %@", availableType);
					}
			}
		} 
		else if (pasteboardData) { // not RSFileWrapper
			[thePasteboard setData:pasteboardData forType:availableType];
			//clipBoardReceivedChangeCount = lastChangeCount;
		}
	}
	else { // Error occured
		NSString *errorString = [[NSString alloc] initWithData:clientPointer->richClipboardReceivedNSData encoding:NSUTF8StringEncoding];
		NSLog(@"%@", errorString);
		[errorString release];
	};
	
	if (clientPointer) {
		pthread_mutex_lock(&clientPointer->updateMutex);
		[(id)clientPointer->richClipboardReceivedNSData release];
		clientPointer->richClipboardReceivedNSData = nil;
		[(id)clientPointer->richClipboardReceivedType release];
		clientPointer->richClipboardReceivedType = nil;
		pthread_mutex_unlock(&clientPointer->updateMutex);
	}
}

- (void) removeClient {
	clientPointer = NULL;
	// Would be nice to tell our PB that we aren't valid
}

- (void)pasteboardChangedOwner:(NSPasteboard *)sender {
	if (clientPointer) {
		pthread_mutex_lock(&clientPointer->updateMutex);
		clientPointer->clipboardProxy = nil;
		pthread_mutex_unlock(&clientPointer->updateMutex);
	}
	[self autorelease];
}

@end


// Currently there is a problem when OSXvnc is run PRIOR to the pbs (which starts when a user logs in)
// the OSXvnc process is NOT connected to the pbs port - this is an OS X security measure which we aren't certain
// how to work around

// We might be able to register with the port later
// Restart VNC on login (of course this kills sessions)
// or spawn a little agent at login -- modify the /etc/ttys and add a -LoginHook process
// or possibly run with -inetd...hmmm...

// This is the global VNC change count
NSRecursiveLock *pasteboardLock = nil;

// Used to lock access to pasteboardString, clientCutText and pasteboards array
NSRecursiveLock *pasteboardVariablesLock = nil;

NSString *pasteboardString = nil;
NSString *clientCutText = nil;
NSMutableDictionary *pasteboards = nil; 

unsigned long long maxTransferSize = 0x10000000;

// Each Pasteboard has an array with item 0 = the ChangeCount and item 1 = the AvailableTypes Array
int generalPBLastChangeCount=-1;
NSStringEncoding pasteboardStringEncoding = NSWindowsCP1252StringEncoding; // RFBProto 003.008

void initPasteboard() {
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	pasteboardLock = [[NSRecursiveLock alloc] init];
	pasteboardVariablesLock = [[NSRecursiveLock alloc] init];
	
	if (![NSPasteboard generalPasteboard]) {
		rfbLog("Pasteboard Inaccessible - Pasteboard sharing disabled");

		[pasteboardVariablesLock lock];
		// Record first in case another event comes in after notifying clients
		generalPBLastChangeCount = 0;
		[pasteboardString release];
		pasteboardString = [[NSString alloc] initWithString:@"\e<PASTEBOARD INACCESSIBLE>\e"];
		[pasteboardVariablesLock unlock];
		
		rfbDisableRichClipboards = TRUE;
	}
	else {
		NSArray *pbNames = [NSArray arrayWithObjects:NSGeneralPboard, NSRulerPboard, NSFontPboard, NSFindPboard, NSDragPboard, nil];
		NSEnumerator *objEnum = [pbNames objectEnumerator];
		NSString *aPasteboard = nil;
		
		pasteboards = [[NSMutableDictionary alloc] init];
		
		while (aPasteboard = [objEnum nextObject]) {
			// Each Pasteboard has an array with item 0 = the ChangeCount and item 1 = the AvailableTypes Array
			[pasteboards setObject:[NSMutableArray arrayWithObjects:[NSNumber numberWithInt:0], [NSArray array], nil] forKey:aPasteboard];
		}

		[[NSUserDefaults standardUserDefaults] registerDefaults:
			[NSDictionary dictionaryWithObject:[NSNumber numberWithUnsignedLongLong:0x10000000] forKey:@"MaxFileTransferSize"]];		
		
		maxTransferSize = [[[NSUserDefaults standardUserDefaults] objectForKey:@"MaxFileTransferSize"] unsignedLongLongValue];
		debugPB = [[NSUserDefaults standardUserDefaults] boolForKey:@"DebugPB"];
	}
	
	if (floor(NSAppKitVersionNumber) <= floor(NSAppKitVersionNumber10_1))
		rfbDisableRichClipboards = TRUE;
	
    [pool release];
}



void initPasteboardForClient(rfbClientPtr cl) {
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    /* REDSTONE - Have new client keep his PB currently */
	cl->generalPBLastChange = -1;
	cl->richClipboardChangeCounts = [[NSMutableDictionary alloc] init];
	cl->clipboardProxy = nil;
	cl->richClipboardName = nil;
	cl->richClipboardType = nil;
	cl->richClipboardNSData = nil;
	
	cl->richClipboardReceivedName = nil;
	cl->richClipboardReceivedType = nil;
	cl->richClipboardReceivedNSData = nil;
	cl->receivedFileTempFolder = nil;
	
	[pool release];
}

void freePasteboardForClient(rfbClientPtr cl) {
	[(NSClipboardProxy *) cl->clipboardProxy removeClient];
	[(NSString *)cl->receivedFileTempFolder release];
	[(NSMutableDictionary *)cl->richClipboardChangeCounts release];
}

// This notifies us that the VNCclient set some new pasteboard
void rfbSetCutText(rfbClientPtr cl, char *str, int len) {
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    
	if (generalPBLastChangeCount != 0) {	
		cl->generalPBLastChange = -1;
		
		[pasteboardVariablesLock lock];
		[clientCutText release];
		clientCutText = [[NSString alloc] initWithData:[NSData dataWithBytes:str length:len] encoding: pasteboardStringEncoding];
		[pasteboardVariablesLock unlock];

		//  we'll stall until it gets picked up by the main thread
		while (clientCutText)
			usleep(10000);		
	}
	
    [pool release];
}

static BOOL pasteboardRepresentsExistingFile(NSPasteboard *thePasteboard) {
	NSArray *pboardTypes = [thePasteboard types];
	if ([pboardTypes containsObject:NSFilenamesPboardType]) {
		NSArray *fileNames = [thePasteboard propertyListForType:NSFilenamesPboardType];
		NSFileManager *fileManager = [NSFileManager defaultManager];
		int index;
		for (index = 0; index < [fileNames count]; index++) {
			if ([fileManager fileExistsAtPath:[fileNames objectAtIndex:index]]) {
				return YES; // at least one of the files exists
			}
		}
	} 
	else if ([pboardTypes containsObject:CorePasteboardFlavor_flst]) {
		return YES; // don't try to parse it all here -- just assume it represents actual files
	}
	else if ([pboardTypes containsObject:VineRemoteProxy]) {
		// In this case we do NOT want to go to the next test, it can lead to a deadlock...
		// where the main thread tries to pull the data from client thread
		// while the client thread waits for the main thread to determine if we have new data
		return NO;
	}
	else if ([pboardTypes containsObject:NSURLPboardType]) {
		NSURL *theUrl = [NSURL URLFromPasteboard:thePasteboard];
		if ([theUrl isFileURL]) {
			return [[NSFileManager defaultManager] fileExistsAtPath:[theUrl path]];
		}
	}
	return NO;
}

static NSArray *arrayFromFlstData(NSData *flstData) {
	NSArray *fileArray = [flstData dataComponentsSeparatedByData:[@"seldtype" dataUsingEncoding:NSUTF8StringEncoding allowLossyConversion:YES]];
	int fileIndex, componentIndex;
	NSMutableArray *pathArray = [NSMutableArray array];
	BOOL error = NO;
	if ([flstData locationOfData:[@"dle2" dataUsingEncoding:NSUTF8StringEncoding allowLossyConversion:YES] startingAtLocation:0] != 0) {
		if (debugPB) NSLog(@"PROBLEM: flst data doesn't start with 'dle2' !!");
		error = YES;
	}
	for (fileIndex = 1; !error && fileIndex < [fileArray count]; fileIndex++) { // (note: start at 1 -- ignore first chunk)
		NSArray *componentArray = [[fileArray objectAtIndex:fileIndex] dataComponentsSeparatedByData: [@"nameseld" dataUsingEncoding:NSUTF8StringEncoding allowLossyConversion:YES]];
		NSString *filePath = @"/";
		for (componentIndex = 1; componentIndex < [componentArray count]; componentIndex++) { // (note: start at 1 -- ignore first chunk)
			NSData *componentData = [componentArray objectAtIndex:componentIndex];
			if ([componentData locationOfData:[@"utxt" dataUsingEncoding:NSUTF8StringEncoding allowLossyConversion:YES] startingAtLocation:0] != 0) {
				if (debugPB) NSLog(@"PROBLEM: flst component doesn't start with 'utxt' !!");
				error = YES;
				break;
			}
			unsigned char high, low;
			high = *(unsigned char *)([componentData bytes]+6);
			low = *(unsigned char *)([componentData bytes]+7);
			unsigned short len = 256*high + low;
			NSData *subdata = [componentData subdataWithRange:NSMakeRange(8,len)];
			NSString *componentStr = [[NSString alloc] initWithData:subdata encoding:NSUnicodeStringEncoding];
			filePath = [filePath stringByAppendingPathComponent:componentStr];
			[componentStr release];
		}
		[pathArray addObject:filePath];
	}
	return pathArray;
}

static NSArray *getListOfFilenamesFromPasteboard(NSPasteboard *thePasteboard) {
	NSArray *pboardTypes = [thePasteboard types];
	if ([pboardTypes containsObject:CorePasteboardFlavor_flst]) { // 10.2 & 10.3
		NSData *flstData = [thePasteboard dataForType:CorePasteboardFlavor_flst];
		return arrayFromFlstData(flstData); //pathArray;
	} else {
		return nil;
	}
}

// We call this in the main thread to see if we have a new pasteboard change and should notify clients to do an update
void rfbCheckForPasteboardChange() {
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	NSEnumerator *pasteboardsEnum = [pasteboards keyEnumerator];
	NSString *pasteboardName = nil;
	
	[pasteboardVariablesLock lock]; // Protect references to clientCutText
	NS_DURING
		if (clientCutText) {
			if ([[NSPasteboard generalPasteboard] declareTypes:[NSArray arrayWithObject:NSStringPboardType] owner:nil]) {
				[[NSPasteboard generalPasteboard] setString:clientCutText forType:NSStringPboardType];
			}
			[clientCutText release];
			clientCutText = nil;
		};
	NS_HANDLER
		NSLog(@"Problem Writing Cut Text To Pasteboard: %@", localException);
	NS_ENDHANDLER
	[pasteboardVariablesLock unlock];

	// First Let's see if we have new info on the pasteboard - if so we'll send an update to each client
	if (generalPBLastChangeCount != [[NSPasteboard generalPasteboard] changeCount]) {
		rfbClientPtr cl;
		rfbClientIteratorPtr iterator = rfbGetClientIterator();

		// Let's grab a copy of it here in the Main/Event Thread so that the output threads don't have to deal with the PB directly
		if ([[NSPasteboard generalPasteboard] availableTypeFromArray:[NSArray arrayWithObject:NSStringPboardType]]) {
			[pasteboardVariablesLock lock];
			// Record first in case another event comes in after notifying clients
			generalPBLastChangeCount = [[NSPasteboard generalPasteboard] changeCount];
			[pasteboardString release];
			pasteboardString = [[[NSPasteboard generalPasteboard] stringForType:NSStringPboardType] copy];
			[pasteboardVariablesLock unlock];
		}

		// Notify each client
		while ((cl = rfbClientIteratorNext(iterator)) != NULL) {
			if (!cl->richClipboardSupport)
				pthread_cond_signal(&cl->updateCond);
		}
		rfbReleaseClientIterator(iterator);
	}
	
	// Check EACH pasteboard  by name to see if it has new data
	while (pasteboardName = [pasteboardsEnum nextObject]) {
		NSMutableArray *pbInfoArray = [pasteboards objectForKey:pasteboardName];
		NSPasteboard *thePasteboard = [NSPasteboard pasteboardWithName:pasteboardName];

		// Record the change count first in case another event comes in while we are pulling the Types
		int pasteboardsChangeCount = [thePasteboard changeCount];
		
		[pasteboardVariablesLock lock];
		if ([[pbInfoArray objectAtIndex:0] intValue] != pasteboardsChangeCount) {
			// Check for special file types (URL for a local file, or NSFilenamesPboardType for an existing file)
			NSArray *pboardTypes = [thePasteboard types];
			[pbInfoArray replaceObjectAtIndex:0 withObject:[NSNumber numberWithInt:pasteboardsChangeCount]];
			if (![pboardTypes count]) {
				if (debugPB)
					NSLog(@"Warning - Pasteboard with no types, ignored");
				[pasteboardVariablesLock unlock];
				continue;
			}
			
			pboardTypes = [pboardTypes arrayByAddingObject:[NSString stringWithFormat:@"RSPBID:%@:%@", NSUserName(), getMACAddressString()]];
			if (![pboardTypes containsObject:NSFileContentsPboardType]) { // no need to check, if file contents is already there
				if (pasteboardRepresentsExistingFile(thePasteboard)) {
					pboardTypes = [pboardTypes arrayByAddingObject:NSFileContentsPboardType];
					if (![pboardTypes containsObject:CorePasteboardFlavor_fccc]) {
						pboardTypes = [pboardTypes arrayByAddingObject:CorePasteboardFlavor_fccc];
					}
					if (![pboardTypes containsObject:NSFilenamesPboardType]) {
						pboardTypes = [pboardTypes arrayByAddingObject:NSFilenamesPboardType];
					}
					if (![pboardTypes containsObject:CorePasteboardFlavor_furl]) {
						pboardTypes = [pboardTypes arrayByAddingObject:CorePasteboardFlavor_furl];
					}
				}
			}
			[pbInfoArray replaceObjectAtIndex:1 withObject:pboardTypes];

			{
				rfbClientPtr cl;
				rfbClientIteratorPtr iterator = rfbGetClientIterator();

				// Notify each client
				while ((cl = rfbClientIteratorNext(iterator)) != NULL) {
					if (cl->richClipboardSupport)
						pthread_cond_signal(&cl->updateCond);
				}
				
				rfbReleaseClientIterator(iterator);
			}
		}
		[pasteboardVariablesLock unlock];
	}
	
	[pool release];
}

static void rfbSendRichClipboardAck(rfbClientPtr cl) {
	struct {
		CARD8 type;                 /* always rfbServerCutText */
		CARD8 pad1;
		CARD16 pad2;
		CARD32 pbChangeCount;
		CARD32 pbNameLength;
	} rfbServerRichPasteboardInfo;		
	
    rfbServerRichPasteboardInfo.type = rfbRichClipboardAvailable;
    rfbServerRichPasteboardInfo.pbChangeCount = 0;
    rfbServerRichPasteboardInfo.pbNameLength = 0;	
    if (WriteExact(cl, (char *)&rfbServerRichPasteboardInfo, sizeof(rfbServerRichPasteboardInfo)) < 0) {
        rfbLogPerror("rfbSendRichClipboardAck: write");
        rfbCloseClient(cl);
    }
}

static void rfbSendRichClipboardAvailable(rfbClientPtr cl, NSString *pasteboardNameString, NSArray *pbInfoArray) {
	struct {
		CARD8 type;
		CARD8 pad1;
		CARD16 pad2;
		int pbChangeCount;
		CARD32 pbNameLength;
	} rfbServerRichPasteboardInfo;		
	char *pasteboardName = (char *)[pasteboardNameString UTF8String];
	char *pbTypesListString = (char *)[[[pbInfoArray objectAtIndex:1] componentsJoinedByString:@"\t"] UTF8String];
	CARD32 typesArrayLength = Swap32IfLE(strlen(pbTypesListString));
	
	if (debugPB)
		NSLog(@"Sending CB Available [%s] Types: %@", pasteboardName, [pbInfoArray objectAtIndex:1]);
    rfbServerRichPasteboardInfo.type = rfbRichClipboardAvailable;
    rfbServerRichPasteboardInfo.pbChangeCount = [[pbInfoArray objectAtIndex:0] intValue];
    rfbServerRichPasteboardInfo.pbNameLength = Swap32IfLE(strlen(pasteboardName));	
    if (WriteExact(cl, (char *)&rfbServerRichPasteboardInfo, sizeof(rfbServerRichPasteboardInfo)) < 0) {
        rfbLogPerror("rfbSendServerNewPasteboardInfo: write");
        rfbCloseClient(cl);
    }
	
	/* followed by char text[pbNameLength] */
    if (WriteExact(cl, pasteboardName, strlen(pasteboardName)) < 0) {
        rfbLogPerror("rfbSendServerNewPasteboardInfo: write");
        rfbCloseClient(cl);
    }
	
	// Length of pbTypesListString
    if (WriteExact(cl, (char *)&typesArrayLength, sizeof(typesArrayLength)) < 0) {
        rfbLogPerror("rfbSendServerNewPasteboardInfo: write");
        rfbCloseClient(cl);
    }
	/* followed by char text[typesArrayLength] */
	if (WriteExact(cl, pbTypesListString, strlen(pbTypesListString)) < 0) {
        rfbLogPerror("rfbSendServerNewPasteboardInfo: write");
        rfbCloseClient(cl);
    }
}

void rfbSendRichClipboardRequest(rfbClientPtr cl) {
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	struct {
		char messageType;
		char padding1;
		char padding2;
		char padding3;
		int changeCount;
	} requestRichClipboardInfo;
	const char *stringToSend;
	int stringLength;
	
	if (debugPB)
		NSLog(@"Sending CB Request [%@] Types: %@", cl->richClipboardReceivedName, cl->richClipboardReceivedType);
	requestRichClipboardInfo.messageType=rfbRichClipboardRequest;
	requestRichClipboardInfo.padding1=0;
	requestRichClipboardInfo.padding2=0;
	requestRichClipboardInfo.padding3=0;
	requestRichClipboardInfo.changeCount=htonl(-1); // -1 to indicate we don't care about a specific request
	WriteExact(cl, (char *)&requestRichClipboardInfo, sizeof(requestRichClipboardInfo));
	
	stringToSend = [(id)cl->richClipboardReceivedName UTF8String];
	stringLength = htonl(strlen(stringToSend));
	WriteExact(cl, (char *)&stringLength, sizeof(stringLength));
	WriteExact(cl, (char *)stringToSend, ntohl(stringLength));

	stringToSend = [(id)cl->richClipboardReceivedType UTF8String];
	stringLength = htonl(strlen(stringToSend));
	WriteExact(cl, (char *)&stringLength, sizeof(stringLength));
	WriteExact(cl, (char *)stringToSend, ntohl(stringLength));
		
	[(id)cl->richClipboardReceivedName release];
	cl->richClipboardReceivedName = nil;
	[(id)cl->richClipboardReceivedType release];
	cl->richClipboardReceivedType = nil;
	
	[pool release];
}
	
static void rfbSendRichClipboardData(rfbClientPtr cl) {
	struct {
		CARD8 type;                 /* always rfbServerCutText */
		CARD8 pad1;
		CARD16 pad2;
		int pbChangeCount;
		CARD32 pbNameLength;
	} rfbServerRichPasteboardInfo;
	CARD32 sendLength;
	
	if (debugPB)
		NSLog(@"Sending CB Data [%s] Type: %s", cl->richClipboardName, cl->richClipboardType);
    rfbServerRichPasteboardInfo.type = rfbRichClipboardData;
    rfbServerRichPasteboardInfo.pbChangeCount = Swap32IfLE(cl->richClipboardDataChangeCount);
    rfbServerRichPasteboardInfo.pbNameLength = Swap32IfLE(strlen(cl->richClipboardName));
    if (WriteExact(cl, (char *)&rfbServerRichPasteboardInfo, sizeof(rfbServerRichPasteboardInfo)) < 0) {
        rfbLogPerror("rfbSendServerNewPasteboardInfo: write");
        rfbCloseClient(cl);
    }
	
	/* followed by char text[pbNameLength] */
    if (WriteExact(cl, cl->richClipboardName, strlen(cl->richClipboardName)) < 0) {
        rfbLogPerror("rfbSendServerNewPasteboardInfo: write");
        rfbCloseClient(cl);
    }
	
	// Length of cl->richClipboardName
	sendLength = Swap32IfLE(strlen(cl->richClipboardType));
    if (WriteExact(cl, (char *)&sendLength, sizeof(sendLength)) < 0) {
        rfbLogPerror("rfbSendServerNewPasteboardInfo: write");
        rfbCloseClient(cl);
    }
	/* followed by char text[typesArrayLength] */
	if (WriteExact(cl, cl->richClipboardType, strlen(cl->richClipboardType)) < 0) {
        rfbLogPerror("rfbSendServerNewPasteboardInfo: write");
        rfbCloseClient(cl);
    }
	
	// Length of pasteboard data
	sendLength = Swap32IfLE([(NSData *)cl->richClipboardNSData length]);
    if (WriteExact(cl, (char *)&sendLength, sizeof(sendLength)) < 0) {
        rfbLogPerror("rfbSendServerNewPasteboardInfo: write");
        rfbCloseClient(cl);
    }
	/* followed by char text[dataLength] */
	if (WriteExact(cl, (char *)[(NSData *)cl->richClipboardNSData bytes], [(NSData *)cl->richClipboardNSData length]) < 0) {
        rfbLogPerror("rfbSendServerNewPasteboardInfo: write");
        rfbCloseClient(cl);
    }
	
	xfree(cl->richClipboardName);
	cl->richClipboardName = NULL;
	xfree(cl->richClipboardType);
	cl->richClipboardType = NULL;
	[(NSData *)cl->richClipboardNSData release];
	cl->richClipboardNSData = nil;
}

// Each client output thread will come here to get the PB and send it, we HAVE the updateLock at this time
void rfbClientUpdatePasteboard(rfbClientPtr cl) {
	// They must have passed initialization and be in the Normal operational mode
	if (cl->state == RFB_NORMAL) {
		NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
		
		if (cl->richClipboardNSData != NULL)
			rfbSendRichClipboardData(cl);
		
		if (cl->richClipboardReceivedName != NULL)
			rfbSendRichClipboardRequest(cl);
			
		// New clients don't get the latest clipboard, they keep their own
		if (cl->generalPBLastChange == -1) {
			cl->generalPBLastChange = generalPBLastChangeCount;
		}
		// Client Identified that it supports Rich Clipboards
		// Send the acknowledgement but no clipboard data
		else if (cl->generalPBLastChange == -3) {
			NSEnumerator *pasteboardsEnum = [pasteboards keyEnumerator];
			NSString *pasteboardName = nil;

			rfbSendRichClipboardAck(cl);
			
			[pasteboardVariablesLock lock]; // Protect references to pasteboards
			cl->generalPBLastChange = generalPBLastChangeCount;
			while (pasteboardName = [pasteboardsEnum nextObject]) {
				NSMutableArray *pbInfoArray = [pasteboards objectForKey:pasteboardName];
					
				[(NSMutableDictionary *)cl->richClipboardChangeCounts setObject:[pbInfoArray objectAtIndex:0] forKey:pasteboardName];
			}
			[pasteboardVariablesLock unlock]; // Protect references to pasteboards
		}
		else {			
			if (cl->richClipboardSupport) {
				NSEnumerator *pasteboardsEnum = [pasteboards keyEnumerator];
				NSString *pasteboardName = nil;
				
				[pasteboardVariablesLock lock]; // Protect references to pasteboards
				while (pasteboardName = [pasteboardsEnum nextObject]) {
					NSMutableArray *pbInfoArray = [pasteboards objectForKey:pasteboardName];
					int changeCountForPasteboard = [[(NSDictionary *)cl->richClipboardChangeCounts objectForKey:pasteboardName] intValue];
					
					if (changeCountForPasteboard < [[pbInfoArray objectAtIndex:0] intValue]) {
						if (changeCountForPasteboard != -1)
							rfbSendRichClipboardAvailable(cl, pasteboardName, pbInfoArray);
						
						[(NSMutableDictionary *)cl->richClipboardChangeCounts setObject:[pbInfoArray objectAtIndex:0] forKey:pasteboardName];
					}
				}
				[pasteboardVariablesLock unlock]; // Protect references to pasteboards
			}
			else {
				[pasteboardVariablesLock lock];
				NS_DURING
					if (cl->generalPBLastChange != generalPBLastChangeCount && pasteboardString) {
						NSData *encodedString = [pasteboardString dataUsingEncoding:pasteboardStringEncoding allowLossyConversion:YES];
						
						if ([encodedString length])
							rfbSendServerCutText(cl, (char *) [encodedString bytes], [encodedString length]);
						
						cl->generalPBLastChange = generalPBLastChangeCount;
					};
				NS_HANDLER
				NS_ENDHANDLER		
				[pasteboardVariablesLock unlock];
			}				
		}
		
		[pool release];
	}
}

// Receive Thread

void rfbReceiveRichClipboardAvailable(rfbClientPtr cl) {
	NSAutoreleasePool *myPool = [[NSAutoreleasePool alloc] init];
	char *readString;
	unsigned int stringLength;
	int pbChangeCount;
	NSString *pasteboardName = nil;
	NSArray *availableTypes = nil;
	int returnCheck;
	
	ReadExact(cl, ((char *)&pbChangeCount), 3);
	ReadExact(cl, ((char *)&pbChangeCount), 4);
	// Right now we don't really care or store this value, 
	// We if we wanted more explicit tracking of what gets returned 
	pbChangeCount = Swap32IfLE(pbChangeCount);
	
	// Pasteboard Name
	ReadExact(cl, (char *)&stringLength, 4);
	stringLength = Swap32IfLE(stringLength);
	readString = (char *) xalloc(stringLength+1);
	readString[stringLength] = 0;
	ReadExact(cl, readString, stringLength);
	pasteboardName = [NSString stringWithUTF8String:readString];
	xfree(readString);
	
	// Type String
	ReadExact(cl, (char *)&stringLength, 4);
	stringLength = Swap32IfLE(stringLength);
	readString = (char *) xalloc(stringLength+1);
	readString[stringLength] = 0;
	returnCheck = ReadExact(cl, readString, stringLength);
	availableTypes = [[NSString stringWithUTF8String:readString] componentsSeparatedByString:@"\t"];
	xfree(readString);
	
	if (debugPB)
		NSLog(@"Received CB Available [%@] Types: %@", pasteboardName, availableTypes);
	
	if (returnCheck <= 0) {
		if (returnCheck != 0)
			rfbLogPerror("rfbReceiveRichClipboardAvailable: read");
		rfbCloseClient(cl);
	}
	
	if (!rfbDisableRichClipboards) {
		if ([availableTypes indexOfObject:[NSString stringWithFormat:@"RSPBID:%@:%@", NSUserName(), getMACAddressString()]] != NSNotFound) {
			if (debugPB)
				NSLog(@"Rich clipboard info appears to be from our own account (%@:%@) -- ignored.", NSUserName(), getMACAddressString());
		}
		else /*if (addNewDataToPB)*/ {
			NSClipboardProxy *newProxy = [[NSClipboardProxy alloc] initWithClientPtr:cl];
			[newProxy setPasteboardName: pasteboardName];
			
			pthread_mutex_lock(&cl->updateMutex);
			//We need to remove ourself as the old proxy's owner at this point, so it doesn't try to request data
			[(NSClipboardProxy *)cl->clipboardProxy removeClient];
			// We do NOT free here, the proxy is freed when the pasteboard system is done using it.
			// [(NSClipboardProxy *)cl->clipboardProxy release];
			cl->clipboardProxy = newProxy;
			pthread_mutex_unlock(&cl->updateMutex);
			
			//if (NSAppKitVersionNumber >= NSAppKitVersionNumber10_2) //We're just disabling rich clipboards below 10.2
			int flstIndex = [availableTypes indexOfObject:CorePasteboardFlavor_flst];
			if (flstIndex != NSNotFound) {
				availableTypes = [[availableTypes mutableCopy] autorelease];
				[(NSMutableArray *)availableTypes removeObjectAtIndex:flstIndex]; // don't declare flst
			}
			
			[newProxy performSelectorOnMainThread:@selector(setupTypes:) withObject:availableTypes waitUntilDone:NO];
		}
	}
		
	[myPool release];
	
	return;
}

void checkTotalSize(unsigned long long *totalSize, NSString *path, NSFileManager *fileManager) {
	BOOL isDirectory = NO;
	if ([fileManager fileExistsAtPath:path isDirectory:&isDirectory]) {
		if (isDirectory) {
			NSArray *contents = [fileManager contentsOfDirectoryAtPath:path error:NULL];
			int index;
			for (index=0; index<[contents count]; index++) {
				checkTotalSize(totalSize, [path stringByAppendingPathComponent:[contents objectAtIndex:index]], fileManager);
			}
		} else { // regular file
			NSDictionary *attributes = [fileManager attributesOfItemAtPath:[path stringByResolvingSymlinksInPath] error:NULL];
			unsigned long long size = [[attributes objectForKey:NSFileSize] unsignedLongLongValue];
			*totalSize += size;
			if (*totalSize > maxTransferSize) {
				[NSException raise:@"Maximum Size Exceeded" format:@"Total size of files being transferred exceeds the maximum allowed."];
			}
		}
	}
}

void rfbReceiveRichClipboardRequest(rfbClientPtr cl) {
	NSAutoreleasePool *myPool = [[NSAutoreleasePool alloc] init];
	int pbChangeCount;
	int readLength;
	int returnCheck;
	char *newClipboardName=NULL;
	char *newClipboardType=NULL;
	NSData *newClipboardNSData=nil;
	int newClipboardDataChangeCount=0;
	
	ReadExact(cl, ((char *)&pbChangeCount), 3);
	ReadExact(cl, ((char *)&pbChangeCount), 4);
	pbChangeCount = Swap32IfLE(pbChangeCount);
	ReadExact(cl, ((char *)&readLength), 4);
	readLength = Swap32IfLE(readLength);
	
	newClipboardName = (char *) xalloc(readLength+1);
	newClipboardName[readLength] = 0;
	ReadExact(cl, newClipboardName, readLength);
	
	ReadExact(cl, ((char *)&readLength), sizeof(readLength));
	readLength = Swap32IfLE(readLength);
	
	newClipboardType = (char *) xalloc(readLength+1);
	newClipboardType[readLength] = 0;
	returnCheck = ReadExact(cl, newClipboardType, readLength);
		
	if (debugPB)
		NSLog(@"Received CB Request [%s] For type: %s", newClipboardName, newClipboardType);

	if (returnCheck > 0) {
		NSPasteboard *thePasteboard = [NSPasteboard pasteboardWithName:[[[NSString alloc] initWithUTF8String:newClipboardName] autorelease]];
		NSString *theType = [[[NSString alloc] initWithUTF8String:newClipboardType] autorelease];
		
		// check whether we need to send file contents in place of another type
		if (([theType isEqualToString:NSFileContentsPboardType] && ![[thePasteboard types] containsObject:NSFileContentsPboardType])
			|| (([theType isEqualToString:NSFilenamesPboardType] || [theType isEqualToString:NSURLPboardType] || [theType isEqualToString:CorePasteboardFlavor_furl]) && pasteboardRepresentsExistingFile(thePasteboard))) {
			NSFileManager *fileManager = [NSFileManager defaultManager];
			NSArray *pboardTypes = [thePasteboard types];
			NSFileWrapper *theWrapper = nil;
			unsigned long long totalSize = 0;
			
			NS_DURING {
				if ([pboardTypes containsObject:NSFilenamesPboardType] || [pboardTypes containsObject:CorePasteboardFlavor_flst]) { // unless URL requested, try file names first
					NSArray *fileNames = [thePasteboard propertyListForType:NSFilenamesPboardType];
					if (!fileNames) {
						fileNames = getListOfFilenamesFromPasteboard(thePasteboard);
						if (debugPB)
							NSLog(@"flst list to Send CB Data for filenames: %@", fileNames);
					}
					if ([fileNames count] == 1) {
						NSString *path = [fileNames objectAtIndex:0];
						checkTotalSize(&totalSize, path, fileManager);
						theWrapper = [[[NSFileWrapper alloc] initWithPath:path] autorelease];
					} else {
						int index;
						NSMutableDictionary *wrappers = [NSMutableDictionary dictionary];
						for (index = 0; index < [fileNames count]; index++) {
							NSString *path = [fileNames objectAtIndex:index];
							if ([fileManager fileExistsAtPath:path]) {
								NSFileWrapper *wrapper = [[[NSFileWrapper alloc] initWithPath:path] autorelease];
								checkTotalSize(&totalSize, path, fileManager);
								if (wrapper)
									[wrappers setObject:wrapper forKey:path];
								else // Could possibly put a "dummy file" in place indicating the error
									NSLog(@"Error Unable To Read File:%@", path);
							}
						}
						theWrapper = [[[NSFileWrapper alloc] initDirectoryWithFileWrappers:wrappers] autorelease];
						[theWrapper setFilename:@"<Redstone set of files>"];
						[theWrapper setPreferredFilename:@"<Redstone set of files>"];
					}
				}
				if (!theWrapper) { // try for a URL next
					NSURL *theUrl = [NSURL URLFromPasteboard:thePasteboard];
					if ([theUrl isFileURL]) { // only do this for file: url's
						NSString *path = [theUrl path];
						if ([fileManager fileExistsAtPath:path]) {
							checkTotalSize(&totalSize, path, fileManager);
							theWrapper = [[[NSFileWrapper alloc] initWithPath:path] autorelease];
						}
					}
				}
				if (theWrapper) { // finish setting up the wrapper
					theType = [@"RSFileWrapper:" stringByAppendingString:theType];
					newClipboardNSData = [[theWrapper serializedRepresentation] retain];
					newClipboardDataChangeCount = [thePasteboard changeCount];
					const char *newTypeStr = [theType UTF8String];
					xfree(newClipboardType);
					newClipboardType = (char *) xalloc(strlen(newTypeStr)+1);
					strcpy(newClipboardType, newTypeStr);
				}
			};
			NS_HANDLER {
				newClipboardDataChangeCount = -1; // Indicate an Error
				newClipboardNSData = [[[NSString stringWithFormat:@"Unable to copy files - %@", localException] dataUsingEncoding:NSUTF8StringEncoding] retain];
			};
			NS_ENDHANDLER
			
		}
		
		if (!newClipboardNSData) {
			newClipboardNSData = [[thePasteboard dataForType:theType] retain];
			newClipboardDataChangeCount = [thePasteboard changeCount];
			if (!newClipboardNSData && [theType isEqualToString:CorePasteboardFlavor_fccc]) { // 10.3 "Copy" Code
				newClipboardNSData = [[@"copy" dataUsingEncoding:NSUTF8StringEncoding] retain];
			}					
		}
		
		if (!newClipboardNSData || pbChangeCount >= 0) {
			newClipboardDataChangeCount = -1; // Indicate an Error
			newClipboardNSData = [[@"Clipboard Data Unavailable" dataUsingEncoding:NSUTF8StringEncoding] retain];
		}

		// Wait for the send thread to send pending data out (if the client goes away, so does this thread)
		while (cl->richClipboardNSData) {
			usleep(.1*1000000);
		}		
	
		//Here we protect access to cl->rfbRichClipboard vars
		pthread_mutex_lock(&cl->updateMutex);
		
		if (cl->richClipboardName)
			xfree(cl->richClipboardName);
		cl->richClipboardName = newClipboardName;
		
		if (cl->richClipboardType)
			xfree(cl->richClipboardType);
		cl->richClipboardType = newClipboardType;
		
		// [(id)cl->richClipboardNSData release];
		cl->richClipboardNSData = newClipboardNSData;
		cl->richClipboardDataChangeCount = newClipboardDataChangeCount;
		
		// Should we note if the ChangeCount is > than our recorded value here?
		pthread_mutex_unlock(&cl->updateMutex);
		pthread_cond_signal(&cl->updateCond);		
	}
	else {
		if (returnCheck != 0)
			rfbLogPerror("rfbReceiveRichClipboardDataRequest: read");
		rfbCloseClient(cl);
	}

	[myPool release];
	
	return;
}

void rfbReceiveRichClipboardData(rfbClientPtr cl) {
	NSAutoreleasePool *myPool = [[NSAutoreleasePool alloc] init];
	int readLength;
	int returnCheck;
	int changeCount;
	char *readString;
	NSMutableData *data = nil;

	ReadExact(cl, ((char *)&changeCount), 3); // Padding
	ReadExact(cl, ((char *)&changeCount), 4); // Change Count
	
	// Pasteboard Name
	ReadExact(cl, ((char *)&readLength), 4);
	readLength = Swap32IfLE(readLength);
	readString = (char *) xalloc(readLength+1);
	readString[readLength] = 0;
	ReadExact(cl, readString, readLength);
	xfree(readString);
	
	// Pasteboard Type
	ReadExact(cl, ((char *)&readLength), sizeof(readLength));
	readLength = Swap32IfLE(readLength);
	readString = (char *) xalloc(readLength+1);
	readString[readLength] = 0;
	ReadExact(cl, readString, readLength);

	// Read Data
	ReadExact(cl, ((char *)&readLength), sizeof(readLength));
	readLength = Swap32IfLE(readLength);
	data = [[NSMutableData alloc] initWithLength:readLength];
	returnCheck = ReadExact(cl, [data mutableBytes], readLength);

	if (returnCheck > 0) {
		if (debugPB)
			NSLog(@"Received CB Data [%@] Type: %s Length: %d bytes", cl->richClipboardReceivedName, readString, readLength);
		pthread_mutex_lock(&cl->updateMutex);
		cl->richClipboardReceivedChangeCount = Swap32IfLE(changeCount); 
		cl->richClipboardReceivedType = [[NSString stringWithUTF8String:readString] retain];
		cl->richClipboardReceivedNSData = data;		
		pthread_mutex_unlock(&cl->updateMutex);
	}
	else {
		if (returnCheck != 0)
			rfbLogPerror("rfbReceiveRichClipboardDataRequest: read");
		rfbCloseClient(cl);
	}
	
	xfree(readString);
	[myPool release];
}
