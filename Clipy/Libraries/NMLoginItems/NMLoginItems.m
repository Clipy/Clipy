//
//  NMLoginItems.m
//
//  Some code copyright 2009 Naotaka Morimoto.
//
//	Much of this code was taken and adapted from GTMLoginItems of Google
//	Toolbox for Mac and QSBPreferenceWindowController of Quick Search Box 
//	for the Mac by Google Inc.
//	This code is also released under Apache License, Version 2.0.
//

//
//  Copyright (c) 2008-2009 Google Inc. All rights reserved.
//
//  Redistribution and use in source and binary forms, with or without
//  modification, are permitted provided that the following conditions are
//  met:
//
//    * Redistributions of source code must retain the above copyright
//  notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
//  copyright notice, this list of conditions and the following disclaimer
//  in the documentation and/or other materials provided with the
//  distribution.
//    * Neither the name of Google Inc. nor the names of its
//  contributors may be used to endorse or promote products derived from
//  this software without specific prior written permission.
//
//  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
//  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
//  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
//  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
//  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
//  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
//  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
//  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
//  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
//  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
//  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//

#import "NMLoginItems.h"

#ifndef kLSSharedFileListLoginItemHidden		// defined on Mac OS X 10.6
#define kLSSharedFileListLoginItemHidden CFSTR("com.apple.loginitem.HideOnLaunch")
#endif

@implementation NMLoginItems

+ (BOOL)pathInLoginItems:(NSString *)path
{
	if (!path) {
		return NO;
	}
	
	LSSharedFileListRef loginItemList = LSSharedFileListCreate(NULL,
															   kLSSharedFileListSessionLoginItems,
															   NULL);
	if (!loginItemList) {
		return NO;
	}
	
	UInt32 snapshotSeed;
	NSArray *loginItems = (NSArray *)LSSharedFileListCopySnapshot(loginItemList, &snapshotSeed);
	if (!loginItems) {
		return NO;
	}
	
	NSURL *url = [NSURL fileURLWithPath:path];
	BOOL found = NO;
	
	for (id item in loginItems) {
		CFURLRef itemURL;
		
		if (LSSharedFileListItemResolve((LSSharedFileListItemRef)item, 0, &itemURL,	NULL) == noErr) {
			if ([url isEqual:(NSURL *)itemURL]) {
				found = YES;
			}
		}
		
		CFRelease(itemURL);
		
		if (found) {
			break;
		}
	}
	
	[loginItems release], loginItems = nil;
	CFRelease(loginItemList);
	
	return found;
}

+ (void)addPathToLoginItems:(NSString *)path hide:(BOOL)hide
{	
	if (!path) {
		return;
	}
	
	LSSharedFileListRef loginItemList = LSSharedFileListCreate(NULL,
															   kLSSharedFileListSessionLoginItems,
															   NULL);
	if (!loginItemList) {
		return;
	}
	
	NSURL *url = [NSURL fileURLWithPath:path];
	NSDictionary *propertiesToSet = [NSDictionary dictionaryWithObject:[NSNumber numberWithBool:hide]
																forKey:(id)kLSSharedFileListLoginItemHidden];
	
	LSSharedFileListItemRef item = LSSharedFileListInsertItemURL(loginItemList, 
																 kLSSharedFileListItemLast, 
																 NULL, 
																 NULL, 
																 (CFURLRef)url, 
																 (CFDictionaryRef)propertiesToSet, 
																 NULL);
	if (item) {
		CFRelease(item);
	}
	
	CFRelease(loginItemList);
}

+ (void)removePathFromLoginItems:(NSString *)path
{
	if (!path) {
		return;
	}
	
	LSSharedFileListRef loginItemList = LSSharedFileListCreate(NULL,
															   kLSSharedFileListSessionLoginItems,
															   NULL);
	if (!loginItemList) {
		return;
	}
	
	UInt32 snapshotSeed;
	NSArray *loginItems = (NSArray *)LSSharedFileListCopySnapshot(loginItemList, &snapshotSeed);
	if (!loginItems) {
		return;
	}
	
	NSURL *url = [NSURL fileURLWithPath:path];
	
	for (id item in loginItems) {
		CFURLRef itemURL;
		
		if (LSSharedFileListItemResolve((LSSharedFileListItemRef)item, 0, &itemURL,	NULL) == noErr) {
			if ([url isEqual:(NSURL *)itemURL]) {
				OSStatus status = LSSharedFileListItemRemove(loginItemList, (LSSharedFileListItemRef)item);
				if (status) {
					NSLog(@"Unable to remove %@ from open at login (%d)", itemURL, status);
				}
			}
		}
		// CFRelease(itemURL);
	}
	
	[loginItems release], loginItems = nil;
	CFRelease(loginItemList);
}

@end
