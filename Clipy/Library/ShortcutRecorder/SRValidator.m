//
//  SRValidator.h
//  ShortcutRecorder
//
//  Copyright 2006-2011 Contributors. All rights reserved.
//
//  License: BSD
//
//  Contributors:
//      David Dauer
//      Jesper
//      Jamie Kirkpatrick
//      Andy Kim

#import "SRValidator.h"
#import "SRCommon.h"

@implementation SRValidator

//---------------------------------------------------------- 
// iinitWithDelegate:
//---------------------------------------------------------- 
- (instancetype)initWithDelegate:(id)theDelegate;
{
    self = [super init];
    if ( !self )
        return nil;
    
    [self setDelegate:theDelegate];
    
    return self;
}

//---------------------------------------------------------- 
// isKeyCode:andFlagsTaken:error:
//---------------------------------------------------------- 
- (BOOL)isKeyCode:(NSInteger)keyCode andFlagsTaken:(NSUInteger)flags error:(NSError **)error;
{
    // if we have a delegate, it goes first...
	if ( _delegate )
	{
		NSString *delegateReason = nil;
		if ( [_delegate shortcutValidator:self
                               isKeyCode:keyCode 
                           andFlagsTaken:SRCarbonToCocoaFlags( flags )
                                  reason:&delegateReason])
		{
            if ( error )
            {
                NSString *description = [NSString stringWithFormat: 
                    SRLoc(@"The key combination %@ can't be used!"), 
                    SRStringForCarbonModifierFlagsAndKeyCode( flags, keyCode )];
                NSString *recoverySuggestion = [NSString stringWithFormat: 
                    SRLoc(@"The key combination \"%@\" can't be used because %@."), 
                    SRReadableStringForCarbonModifierFlagsAndKeyCode( flags, keyCode ),
                    ( delegateReason && [delegateReason length] ) ? delegateReason : @"it's already used"];
                NSDictionary *userInfo = @{NSLocalizedDescriptionKey: description,
										  NSLocalizedRecoverySuggestionErrorKey: recoverySuggestion,
										  NSLocalizedRecoveryOptionsErrorKey: @[@"OK"]};
                *error = [NSError errorWithDomain:NSCocoaErrorDomain code:0 userInfo:userInfo];
            }
			
			return YES;
		}
	}
	
    return NO;
    /*
	// then our implementation...
	CFArrayRef tempArray = NULL;
	OSStatus err = noErr;
	
	// get global hot keys...
	err = CopySymbolicHotKeys( &tempArray );

	if ( err != noErr ) return YES;

	// Not copying the array like this results in a leak on according to the Leaks Instrument
	NSArray *globalHotKeys = [NSArray arrayWithArray:(__bridge NSArray *)tempArray];
    
	if ( tempArray ) CFRelease(tempArray);
	
	NSEnumerator *globalHotKeysEnumerator = [globalHotKeys objectEnumerator];
	NSDictionary *globalHotKeyInfoDictionary;
	int32_t globalHotKeyFlags;
	NSInteger globalHotKeyCharCode;
	BOOL globalCommandMod = NO, globalOptionMod = NO, globalShiftMod = NO, globalCtrlMod = NO;
	BOOL localCommandMod = NO, localOptionMod = NO, localShiftMod = NO, localCtrlMod = NO;
	
	// Prepare local carbon comparison flags
	if ( flags & cmdKey )       localCommandMod = YES;
	if ( flags & optionKey )    localOptionMod = YES;
	if ( flags & shiftKey )     localShiftMod = YES;
	if ( flags & controlKey )   localCtrlMod = YES;
    
	while (( globalHotKeyInfoDictionary = [globalHotKeysEnumerator nextObject] ))
	{
		// Only check if global hotkey is enabled
		if ( (__bridge CFBooleanRef)globalHotKeyInfoDictionary[(NSString *)kHISymbolicHotKeyEnabled] != kCFBooleanTrue )
            continue;
		
        globalCommandMod    = NO;
        globalOptionMod     = NO;
        globalShiftMod      = NO;
        globalCtrlMod       = NO;
        
        globalHotKeyCharCode = [(NSNumber *)globalHotKeyInfoDictionary[(NSString *)kHISymbolicHotKeyCode] shortValue];
        
        CFNumberGetValue((CFNumberRef)globalHotKeyInfoDictionary[(NSString *)kHISymbolicHotKeyModifiers],kCFNumberSInt32Type,&globalHotKeyFlags);
        
        if ( globalHotKeyFlags & cmdKey )        globalCommandMod = YES;
        if ( globalHotKeyFlags & optionKey )     globalOptionMod = YES;
        if ( globalHotKeyFlags & shiftKey)       globalShiftMod = YES;
        if ( globalHotKeyFlags & controlKey )    globalCtrlMod = YES;
        
        NSString *localKeyString = SRStringForKeyCode( keyCode );
        if (![localKeyString length]) return YES;
        
        
        // compare unichar value and modifier flags
		if ( ( globalHotKeyCharCode == keyCode ) 
             && ( globalCommandMod == localCommandMod ) 
             && ( globalOptionMod == localOptionMod ) 
             && ( globalShiftMod == localShiftMod ) 
             && ( globalCtrlMod == localCtrlMod ) )
        {
            if ( error )
            {
                NSString *description = [NSString stringWithFormat: 
                    SRLoc(@"The key combination %@ can't be used!"), 
                    SRStringForCarbonModifierFlagsAndKeyCode( flags, keyCode )];
                NSString *recoverySuggestion = [NSString stringWithFormat: 
                    SRLoc(@"The key combination \"%@\" can't be used because it's already used by a system-wide keyboard shortcut. (If you really want to use this key combination, most shortcuts can be changed in the Keyboard & Mouse panel in System Preferences.)"), 
                    SRReadableStringForCarbonModifierFlagsAndKeyCode( flags, keyCode )];
				NSDictionary *userInfo = @{NSLocalizedDescriptionKey: description,
										  NSLocalizedRecoverySuggestionErrorKey: recoverySuggestion,
										  NSLocalizedRecoveryOptionsErrorKey: @[@"OK"]};
                *error = [NSError errorWithDomain:NSCocoaErrorDomain code:0 userInfo:userInfo];
            }
            return YES;
        }
	}

	// Check menus too
	return [self isKeyCode:keyCode andFlags:flags takenInMenu:[NSApp mainMenu] error:error];
     */
}

//---------------------------------------------------------- 
// isKeyCode:andFlags:takenInMenu:error:
//---------------------------------------------------------- 
- (BOOL) isKeyCode:(NSInteger)keyCode andFlags:(NSUInteger)flags takenInMenu:(NSMenu *)menu error:(NSError **)error;
{
    NSArray *menuItemsArray = [menu itemArray];
	NSEnumerator *menuItemsEnumerator = [menuItemsArray objectEnumerator];
	NSMenuItem *menuItem;
	NSUInteger menuItemModifierFlags;
	NSString *menuItemKeyEquivalent;
	
	BOOL menuItemCommandMod = NO, menuItemOptionMod = NO, menuItemShiftMod = NO, menuItemCtrlMod = NO;
	BOOL localCommandMod = NO, localOptionMod = NO, localShiftMod = NO, localCtrlMod = NO;
	
	// Prepare local carbon comparison flags
	if ( flags & cmdKey )       localCommandMod = YES;
	if ( flags & optionKey )    localOptionMod = YES;
	if ( flags & shiftKey )     localShiftMod = YES;
	if ( flags & controlKey )   localCtrlMod = YES;
	
	while (( menuItem = [menuItemsEnumerator nextObject] ))
	{
        // rescurse into all submenus...
		if ( [menuItem hasSubmenu] )
		{
			if ( [self isKeyCode:keyCode andFlags:flags takenInMenu:[menuItem submenu] error:error] ) 
			{
				return YES;
			}
		}
		
		if ( ( menuItemKeyEquivalent = [menuItem keyEquivalent] )
             && ( ![menuItemKeyEquivalent isEqualToString: @""] ) )
		{
			menuItemCommandMod = NO;
			menuItemOptionMod = NO;
			menuItemShiftMod = NO;
			menuItemCtrlMod = NO;
			
			menuItemModifierFlags = [menuItem keyEquivalentModifierMask];
            
			if ( menuItemModifierFlags & NSCommandKeyMask )     menuItemCommandMod = YES;
			if ( menuItemModifierFlags & NSAlternateKeyMask )   menuItemOptionMod = YES;
			if ( menuItemModifierFlags & NSShiftKeyMask )       menuItemShiftMod = YES;
			if ( menuItemModifierFlags & NSControlKeyMask )     menuItemCtrlMod = YES;
			
			NSString *localKeyString = SRStringForKeyCode( keyCode );
			
			// Compare translated keyCode and modifier flags
			if ( ( [[menuItemKeyEquivalent uppercaseString] isEqualToString: localKeyString] ) 
                 && ( menuItemCommandMod == localCommandMod ) 
                 && ( menuItemOptionMod == localOptionMod ) 
                 && ( menuItemShiftMod == localShiftMod ) 
                 && ( menuItemCtrlMod == localCtrlMod ) )
			{
                if ( error )
                {
                    NSString *description = [NSString stringWithFormat: 
                        SRLoc(@"The key combination %@ can't be used!"),
                        SRStringForCarbonModifierFlagsAndKeyCode( flags, keyCode )];
                    NSString *recoverySuggestion = [NSString stringWithFormat: 
                        SRLoc(@"The key combination \"%@\" can't be used because it's already used by the menu item \"%@\"."), 
                        SRReadableStringForCocoaModifierFlagsAndKeyCode( menuItemModifierFlags, keyCode ),
                        [menuItem title]];
                    NSDictionary *userInfo = @{NSLocalizedDescriptionKey: description,
											  NSLocalizedRecoverySuggestionErrorKey: recoverySuggestion,
											  NSLocalizedRecoveryOptionsErrorKey: @[@"OK"]};
                    *error = [NSError errorWithDomain:NSCocoaErrorDomain code:0 userInfo:userInfo];
                }
				return YES;
			}
		}
	}
	return NO;    
}

#pragma mark -
#pragma mark accessors

@end
