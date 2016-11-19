//
//  JaguarExtensions.h
//  OSXvnc
//
//  Created by Jonathan Gillaspie on Fri Jul 11 2003.
//  Copyright (c) 2003 RedstoneSoftware. All rights reserved.
//


#import <Foundation/Foundation.h>
#import <Carbon/Carbon.h>

#import "RFBBundleProtocol.h"
#import "VNCBundle.h"

@interface JaguarExtensions : VNCBundle <RFBBundleProtocol> {
}

+ (void) registerRendezvous;

@end

@interface RendezvousDelegate : NSObject

@end
