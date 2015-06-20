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

#import <Cocoa/Cocoa.h>


@interface NMLoginItems : NSObject 

/// Check if the given path is in the current user's Login Items
//
//  Args:
//    path: path to the application
//
//  Returns:
//   YES if the path is in the Login Items
// 
+ (BOOL)pathInLoginItems:(NSString *)path;

/// Add the given path to the current user's Login Items. Does nothing if the
/// path is already there.
//
// Args:
//   path: path to add
//   hide: Set to YES to have the item launch hidden
//
+ (void)addPathToLoginItems:(NSString *)path hide:(BOOL)hide;

/// Remove the given path from the current user's Login Items. Does nothing if
/// the path is not there.
//
//  Args:
//    path: the path to remove
//
+ (void)removePathFromLoginItems:(NSString *)path;

@end
