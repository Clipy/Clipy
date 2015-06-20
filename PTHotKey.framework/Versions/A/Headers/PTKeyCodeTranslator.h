//
//  PTKeyCodeTranslator.h
//  Chercher
//
//  Created by Finlay Dobbie on Sat Oct 11 2003.
//  Copyright (c) 2003 Cliché Software. All rights reserved.
//

#import <Carbon/Carbon.h>

@interface PTKeyCodeTranslator : NSObject
{
    TISInputSourceRef	keyboardLayout;
    const UCKeyboardLayout	*uchrData;
    UInt32		keyTranslateState;
    UInt32		deadKeyState;
}

+ (id)currentTranslator;

- (id)initWithKeyboardLayout:(TISInputSourceRef)aLayout;
- (NSString *)translateKeyCode:(short)keyCode;

- (TISInputSourceRef)keyboardLayout;

@end
