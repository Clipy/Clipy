//
//  PTKeyCombo.h
//  Protein
//
//  Created by Quentin Carnicelli on Sat Aug 02 2003.
//  Copyright (c) 2003 Quentin D. Carnicelli. All rights reserved.
//

#import <Cocoa/Cocoa.h>

@interface PTKeyCombo : NSObject <NSCopying>
{
	NSInteger	mKeyCode;
	NSUInteger	mModifiers;
}

+ (id)clearKeyCombo;
+ (id)keyComboWithKeyCode: (NSInteger)keyCode modifiers: (NSUInteger)modifiers;
- (id)initWithKeyCode: (NSInteger)keyCode modifiers: (NSUInteger)modifiers;

- (id)initWithPlistRepresentation: (id)plist;
- (id)plistRepresentation;

- (BOOL)isEqual: (PTKeyCombo*)combo;

- (NSInteger)keyCode;
- (NSUInteger)modifiers;

- (BOOL)isClearCombo;
- (BOOL)isValidHotKeyCombo;

@end


@interface PTKeyCombo (UserDisplayAdditions)
- (NSString*)keyCodeString;
- (NSUInteger)modifierMask;
@end
