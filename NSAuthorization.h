//
//  NSAuthorization.h
//  OSXvnc
//
//  Created by Jonathan Gillaspie on Fri Dec 12 2003.
//  Copyright (c) 2003 __MyCompanyName__. All rights reserved.
//

#import <Foundation/Foundation.h>

#include <Security/Authorization.h>
#include <Security/AuthorizationTags.h>

@interface NSAuthorization : NSObject {
    // Persistent AuthRef
    AuthorizationRef myAuthorizationRef;
}

- init;
- (BOOL) executeCommand:(NSString *) command withArgs: (NSArray *) argumentArray;
- (BOOL) executeCommand:(NSString *) command withArgs: (NSArray *) argumentArray synchronous: (BOOL) sync;

@end
