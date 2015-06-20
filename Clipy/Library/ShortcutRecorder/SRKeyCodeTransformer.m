//
//  SRKeyCodeTransformer.h
//  ShortcutRecorder
//
//  Copyright 2006-2007 Contributors. All rights reserved.
//
//  License: BSD
//
//  Contributors:
//      David Dauer
//      Jesper
//      Jamie Kirkpatrick

#import "SRKeyCodeTransformer.h"
#import <Carbon/Carbon.h>
#import <CoreServices/CoreServices.h>
#import "SRCommon.h"

static NSMutableDictionary  *stringToKeyCodeDict = nil;
static NSDictionary         *keyCodeToStringDict = nil;
static NSArray              *padKeysArray        = nil;

@interface SRKeyCodeTransformer( Private )
+ (void) regenerateStringToKeyCodeMapping;
@end

#pragma mark -

@implementation SRKeyCodeTransformer

//---------------------------------------------------------- 
//  initialize
//---------------------------------------------------------- 
+ (void) initialize;
{
    if ( self != [SRKeyCodeTransformer class] )
        return;
    
    // Some keys need a special glyph
	keyCodeToStringDict = @{
		@122: @"F1",
		@120: @"F2",
		@99:  @"F3",
		@118: @"F4",
		@96:  @"F5",
		@97:  @"F6",
		@98:  @"F7",
		@100: @"F8",
		@101: @"F9",
		@109: @"F10",
		@103: @"F11",
		@111: @"F12",
		@105: @"F13",
		@107: @"F14",
		@113: @"F15",
		@106: @"F16",
		@64:  @"F17",
		@79:  @"F18",
		@80:  @"F19",
		@49:  SRLoc(@"Space"),
		@51:  SRChar(KeyboardDeleteLeftGlyph),
		@117: SRChar(KeyboardDeleteRightGlyph),
		@71:  SRChar(KeyboardPadClearGlyph),
		@123: SRChar(KeyboardLeftArrowGlyph),
		@124: SRChar(KeyboardRightArrowGlyph),
		@126: SRChar(KeyboardUpArrowGlyph),
		@125: SRChar(KeyboardDownArrowGlyph),
		@119: SRChar(KeyboardSoutheastArrowGlyph),
		@115: SRChar(KeyboardNorthwestArrowGlyph),
		@53:  SRChar(KeyboardEscapeGlyph),
		@121: SRChar(KeyboardPageDownGlyph),
		@116: SRChar(KeyboardPageUpGlyph),
		@36:  SRChar(KeyboardReturnR2LGlyph),
		@76:  SRChar(KeyboardReturnGlyph),
		@48:  SRChar(KeyboardTabRightGlyph),
		@114: SRChar(KeyboardHelpGlyph)};    
    
    // We want to identify if the key was pressed on the numpad
	padKeysArray = @[@65, // ,
		@67, // *
		@69, // +
		@75, // /
		@78, // -
		@81, // =
		@82, // 0
		@83, // 1
		@84, // 2
		@85, // 3
		@86, // 4
		@87, // 5
		@88, // 6
		@89, // 7
		@91, // 8
		@92];
    
    // generate the string to keycode mapping dict...
    stringToKeyCodeDict = [[NSMutableDictionary alloc] init];
    [self regenerateStringToKeyCodeMapping];

	[[NSDistributedNotificationCenter defaultCenter] addObserver:self
														selector:@selector(regenerateStringToKeyCodeMapping)
															name:(NSString*)kTISNotifySelectedKeyboardInputSourceChanged
														  object:nil];
}

//---------------------------------------------------------- 
//  allowsReverseTransformation
//---------------------------------------------------------- 
+ (BOOL) allowsReverseTransformation
{
    return YES;
}

//---------------------------------------------------------- 
//  transformedValueClass
//---------------------------------------------------------- 
+ (Class) transformedValueClass;
{
    return [NSString class];
}


//---------------------------------------------------------- 
//  init
//---------------------------------------------------------- 
- (instancetype)init
{
	if((self = [super init]))
	{
	}
	return self;
}

//---------------------------------------------------------- 
//  dealloc
//---------------------------------------------------------- 
- (void)dealloc
{

}

//---------------------------------------------------------- 
//  transformedValue: 
//---------------------------------------------------------- 
- (id) transformedValue:(id)value
{
    if ( ![value isKindOfClass:[NSNumber class]] )
        return nil;
    
    // Can be -1 when empty
    NSInteger keyCode = [value shortValue];
	if ( keyCode < 0 ) return nil;
	
	// We have some special gylphs for some special keys...
	NSString *unmappedString = keyCodeToStringDict[@(keyCode)];
	if ( unmappedString != nil ) return unmappedString;
	
	BOOL isPadKey = [padKeysArray containsObject: @(keyCode)];	
	
	OSStatus err;
	TISInputSourceRef tisSource = TISCopyCurrentKeyboardInputSource();
	if(!tisSource) return nil;
	
	CFDataRef layoutData;
	UInt32 keysDown = 0;
	layoutData = (CFDataRef)TISGetInputSourceProperty(tisSource, kTISPropertyUnicodeKeyLayoutData);
	
	CFRelease(tisSource);
	
	// For non-unicode layouts such as Chinese, Japanese, and Korean, get the ASCII capable layout
	if(!layoutData) {
		tisSource = TISCopyCurrentASCIICapableKeyboardLayoutInputSource();
		layoutData = (CFDataRef)TISGetInputSourceProperty(tisSource, kTISPropertyUnicodeKeyLayoutData);
		CFRelease(tisSource);
	}

	if (!layoutData) return nil;
	
	const UCKeyboardLayout *keyLayout = (const UCKeyboardLayout *)CFDataGetBytePtr(layoutData);
	
	UniCharCount length = 4, realLength;
	UniChar chars[4];
	
	err = UCKeyTranslate( keyLayout, 
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
	
	return ( isPadKey ? [NSString stringWithFormat: SRLoc(@"Pad %@"), keyString] : keyString );
}

//---------------------------------------------------------- 
//  reverseTransformedValue: 
//---------------------------------------------------------- 
- (id) reverseTransformedValue:(id)value
{
    if ( ![value isKindOfClass:[NSString class]] )
        return nil;
    
    // try and retrieve a mapped keycode from the reverse mapping dict...
    return stringToKeyCodeDict[value];
}

@end

#pragma mark -

@implementation SRKeyCodeTransformer( Private )

//---------------------------------------------------------- 
//  regenerateStringToKeyCodeMapping: 
//---------------------------------------------------------- 
+ (void) regenerateStringToKeyCodeMapping;
{
    SRKeyCodeTransformer *transformer = [[self alloc] init];
    [stringToKeyCodeDict removeAllObjects];
    
    // loop over every keycode (0 - 127) finding its current string mapping...
    for (NSUInteger i = 0; i < 128; ++i )
    {
        NSNumber *keyCode = @(i);
        NSString *string = [transformer transformedValue:keyCode];
        if ( ( string ) && ( [string length] ) )
        {
            stringToKeyCodeDict[string] = keyCode;
        }
    }
}

@end
