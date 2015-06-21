//
//  CPYUtilitiesObjC.m
//  Clipy
//
//  Created by 古林俊佑 on 2015/06/21.
//  Copyright (c) 2015年 Shunsuke Furubayashi. All rights reserved.
//

#import "CPYUtilitiesObjC.h"

#import <Carbon/Carbon.h>

static NSInteger vKeyCode = 0;

@implementation CPYUtilitiesObjC

#pragma mark - Class Methods
+ (BOOL)postCommandV {
    CGEventSourceRef sourceRef = CGEventSourceCreate(kCGEventSourceStateCombinedSessionState);
    if (!sourceRef) {
        NSLog(@"No event source");
        return NO;
    }
    
    if (vKeyCode == 0) {
        NSMutableDictionary *stringToKeyCodeDict = [[NSMutableDictionary alloc] init];
        NSUInteger i;
        for ( i = 0U; i < 128U; i++ ) {
            NSString *string = [self transformKeyCode:i];
            if ( ( string ) && ( [string length] ) ) {
                NSNumber *keyCode = [NSNumber numberWithUnsignedInteger:i];
                [stringToKeyCodeDict setObject:keyCode forKey:string];
            }
        }
        
        NSNumber *keyCodeNum = [stringToKeyCodeDict objectForKey:@"V"];
        
        if (!keyCodeNum) {
            return NO;
        }
        
        vKeyCode = [keyCodeNum unsignedIntegerValue];
    }
    
    CGEventRef eventDown, eventUp;
    
    eventDown = CGEventCreateKeyboardEvent(sourceRef, (CGKeyCode)vKeyCode, true);	// v key down
    CGEventSetFlags(eventDown, kCGEventFlagMaskCommand);							// command key press
    eventUp = CGEventCreateKeyboardEvent(sourceRef, (CGKeyCode)vKeyCode, false);	// v key up
    CGEventSetFlags(eventUp, kCGEventFlagMaskCommand);								// command key up
    
    CGEventPost(kCGSessionEventTap, eventDown);
    CGEventPost(kCGSessionEventTap, eventUp);
    
    CFRelease(eventDown);
    CFRelease(eventUp);
    CFRelease(sourceRef);
    
    return YES;
}

+ (NSString *)transformKeyCode:(NSInteger)keyCode {
    // Can be -1 when empty
    if ( keyCode < 0 ) return nil;
    
    OSStatus err;
    TISInputSourceRef tisSource = TISCopyCurrentKeyboardLayoutInputSource();
    
    if(!tisSource) return nil;
    
    CFDataRef layoutData;
    UInt32 keysDown = 0;
    layoutData = (CFDataRef)TISGetInputSourceProperty(tisSource, kTISPropertyUnicodeKeyLayoutData);
    CFRelease(tisSource);
    
    if(!layoutData) return nil;
    const UCKeyboardLayout *keyLayout = (const UCKeyboardLayout *)CFDataGetBytePtr(layoutData);
    
    UniCharCount length = 4, realLength;
    UniChar chars[4];
    
    err = UCKeyTranslate(keyLayout,
                         keyCode,
                         kUCKeyActionDisplay,
                         0,
                         LMGetKbdType(),
                         kUCKeyTranslateNoDeadKeysBit,
                         &keysDown,
                         length,
                         &realLength,
                         chars);
    
    if ( err != noErr ) return nil;
    
    NSString *keyString = [[NSString stringWithCharacters:chars length:1] uppercaseString];
    
    return keyString;
    
}

+ (void)popupMenu:(NSMenu *)menu event:(NSEvent *)event {
    [NSMenu popUpContextMenu:menu withEvent:event forView:nil];
}

@end
