//
//  NSAuthorization.m
//  OSXvnc
//
//  Created by Jonathan Gillaspie on Fri Dec 12 2003.
//  Copyright (c) 2003 __MyCompanyName__. All rights reserved.
//

#import "NSAuthorization.h"

#include "unistd.h"

@implementation NSAuthorization

- init {
    AuthorizationFlags myFlags = kAuthorizationFlagDefaults |
    kAuthorizationFlagInteractionAllowed |
    kAuthorizationFlagPreAuthorize |
    kAuthorizationFlagExtendRights;
    OSStatus myStatus;
    AuthorizationItem myItems = {kAuthorizationRightExecute, 0, NULL, 0};
    AuthorizationRights myRights = {1, &myItems};
    
    [super init];
    
    myStatus = AuthorizationCreate(NULL, kAuthorizationEmptyEnvironment, myFlags, &myAuthorizationRef);

    // This will pre-authorize the authentication
    if (myStatus == errAuthorizationSuccess)
        myStatus = AuthorizationCopyRights(myAuthorizationRef, &myRights, NULL, myFlags, NULL);

    if (myStatus != errAuthorizationSuccess) {
        return nil;
    }
    
    return self;
}

- (BOOL) executeCommand:(NSString *) command withArgs: (NSArray *) argumentArray {
    char **copyArguments = NULL;
    int i;
    OSStatus myStatus;
        
    copyArguments = malloc(sizeof(char *) * ([argumentArray count]+1));
    for (i=0;i<[argumentArray count];i++) {
        copyArguments[i] = (char *) [[argumentArray objectAtIndex:i] lossyCString];
    }
    copyArguments[i] = NULL;
    
    myStatus = AuthorizationExecuteWithPrivileges(myAuthorizationRef, 
                                                  [command lossyCString], 
                                                  kAuthorizationFlagDefaults,
                                                  copyArguments, 
                                                  NULL); // FILE HANDLE for I/O
    
    // What would be better would be to get the I/O handle and block until it is closed
    // sigh, but for now...
    
    usleep(400000); // We need to give the process time to finish

    free(copyArguments);
    
    if (myStatus != errAuthorizationSuccess)
        NSLog(@"Error: Executing %@ with Authorization: %d", command, myStatus);

    return (myStatus == errAuthorizationSuccess);
}


- (void) dealloc {
    AuthorizationFree (myAuthorizationRef, kAuthorizationFlagDefaults);

    [super dealloc];
}

@end
