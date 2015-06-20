//
//  SRRecorderControl.m
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

#import "SRRecorderControl.h"
#import "SRCommon.h"

#define SRCell (SRRecorderCell *)[self cell]

@interface SRRecorderControl (Private)

- (void)resetTrackingRects;

@end

@implementation SRRecorderControl

+ (void)initialize
{
    if (self == [SRRecorderControl class])
	{
        [self setCellClass: [SRRecorderCell class]];
    }
}

+ (Class)cellClass
{
    return [SRRecorderCell class];
}

- (instancetype)initWithFrame:(NSRect)frameRect
{
	self = [super initWithFrame: frameRect];
	
	[SRCell setDelegate: self];
	
	return self;
}

- (instancetype)initWithCoder:(NSCoder *)aDecoder
{
	self = [super initWithCoder: aDecoder];
    self = [super initWithFrame:self.frame];
    
	[SRCell setDelegate: self];
	
	return self;
}

- (void)encodeWithCoder:(NSCoder *)aCoder
{
	[super encodeWithCoder: aCoder];
}

- (void)dealloc
{
    [[NSNotificationCenter defaultCenter] removeObserver:self];
}

#pragma mark *** Cell Behavior ***

// We need keyboard access
- (BOOL)acceptsFirstResponder
{
    return YES;
}

// Allow the control to be activated with the first click on it even if it's window isn't the key window
- (BOOL)acceptsFirstMouse:(NSEvent *)theEvent
{
	return YES;
}

- (BOOL) becomeFirstResponder 
{
    BOOL okToChange = [SRCell becomeFirstResponder];
    if (okToChange) [super setKeyboardFocusRingNeedsDisplayInRect:[self bounds]];
    return okToChange;
}

- (BOOL) resignFirstResponder 
{
    BOOL okToChange = [SRCell resignFirstResponder];
    if (okToChange) [super setKeyboardFocusRingNeedsDisplayInRect:[self bounds]];
    return okToChange;
}

#pragma mark *** Aesthetics ***
- (BOOL)animates
{
	return [SRCell animates];
}

- (void)setAnimates:(BOOL)an
{
	[SRCell setAnimates:an];
}

- (SRRecorderStyle)style
{
	return [SRCell style];
}

- (void)setStyle:(SRRecorderStyle)nStyle
{
	[SRCell setStyle:nStyle];
}

#pragma mark *** Interface Stuff ***


// If the control is set to be resizeable in width, this will make sure that the tracking rects are always updated
- (void)viewDidMoveToWindow
{
    NSNotificationCenter *center = [NSNotificationCenter defaultCenter];
    
	if ([self window])
	{
		[center removeObserver: self];
		[center addObserver:self selector:@selector(viewFrameDidChange:) name:NSViewFrameDidChangeNotification object:self];
	
		[self resetTrackingRects];
	}
}

- (void)viewFrameDidChange:(NSNotification *)aNotification
{
	[self resetTrackingRects];
}

// Prevent from being too small
- (void)setFrameSize:(NSSize)newSize
{
	NSSize correctedSize = newSize;
	correctedSize.height = SRMaxHeight;
	if (correctedSize.width < SRMinWidth) correctedSize.width = SRMinWidth;
	
	[super setFrameSize: correctedSize];
}

- (void)setFrame:(NSRect)frameRect
{
	NSRect correctedFrarme = frameRect;
	correctedFrarme.size.height = SRMaxHeight;
	if (correctedFrarme.size.width < SRMinWidth) correctedFrarme.size.width = SRMinWidth;

	[super setFrame: correctedFrarme];
}

- (NSString *)keyChars
{
	return [SRCell keyChars];
}

- (NSString *)keyCharsIgnoringModifiers
{
	return [SRCell keyCharsIgnoringModifiers];	
}

#pragma mark *** Key Interception ***

// Like most NSControls, pass things on to the cell
- (BOOL)performKeyEquivalent:(NSEvent *)theEvent
{
	// Only if we're key, please. Otherwise hitting Space after having
	// tabbed past SRRecorderControl will put you into recording mode.
	if (([[[self window] firstResponder] isEqualTo:self])) { 
		if ([SRCell performKeyEquivalent:theEvent]) return YES;
	}

	return [super performKeyEquivalent: theEvent];
}

- (void)flagsChanged:(NSEvent *)theEvent
{
	[SRCell flagsChanged:theEvent];
}

- (void)keyDown:(NSEvent *)theEvent
{
	if ( [SRCell performKeyEquivalent: theEvent] )
        return;
    
    [super keyDown:theEvent];
}

#pragma mark *** Key Combination Control ***

- (NSUInteger)allowedFlags
{
	return [SRCell allowedFlags];
}

- (void)setAllowedFlags:(NSUInteger)flags
{
	[SRCell setAllowedFlags: flags];
}

- (BOOL)allowsKeyOnly {
	return [SRCell allowsKeyOnly];
}

- (void)setAllowsKeyOnly:(BOOL)nAllowsKeyOnly
{
	[SRCell setAllowsKeyOnly: nAllowsKeyOnly];
}

- (void)setAllowsKeyOnly:(BOOL)nAllowsKeyOnly escapeKeysRecord:(BOOL)nEscapeKeysRecord {
	[SRCell setAllowsKeyOnly:nAllowsKeyOnly escapeKeysRecord:nEscapeKeysRecord];
}

- (BOOL)escapeKeysRecord {
	return [SRCell escapeKeysRecord];
}

- (void)setEscapeKeysRecord:(BOOL)nEscapeKeysRecord
{
	[SRCell setEscapeKeysRecord: nEscapeKeysRecord];
}

- (BOOL)canCaptureGlobalHotKeys
{
	return [[self cell] canCaptureGlobalHotKeys];
}

- (void)setCanCaptureGlobalHotKeys:(BOOL)inState
{
	[[self cell] setCanCaptureGlobalHotKeys:inState];
}

- (NSUInteger)requiredFlags
{
	return [SRCell requiredFlags];
}

- (void)setRequiredFlags:(NSUInteger)flags
{
	[SRCell setRequiredFlags: flags];
}

- (KeyCombo)keyCombo
{
	return [SRCell keyCombo];
}

- (void)setKeyCombo:(KeyCombo)aKeyCombo
{
	[SRCell setKeyCombo: aKeyCombo];
}

#pragma mark *** Binding Methods ***

- (NSDictionary *)objectValue
{
    KeyCombo keyCombo = [self keyCombo];
    if (keyCombo.code == ShortcutRecorderEmptyCode || keyCombo.flags == ShortcutRecorderEmptyFlags)
	{
        return nil;
	}
	
    return @{@"characters"	: [self keyCharsIgnoringModifiers],
            @"keyCode"		: @(keyCombo.code),
            @"modifierFlags": @(keyCombo.flags)};
}

- (void)setObjectValue:(NSDictionary *)shortcut
{
    KeyCombo keyCombo = SRMakeKeyCombo(ShortcutRecorderEmptyCode, ShortcutRecorderEmptyFlags);
    if (shortcut != nil && [shortcut isKindOfClass:[NSDictionary class]])
	{
        NSNumber *keyCode = shortcut[@"keyCode"];
        NSNumber *modifierFlags = shortcut[@"modifierFlags"];
        if ([keyCode isKindOfClass:[NSNumber class]] && [modifierFlags isKindOfClass:[NSNumber class]])
		{
            keyCombo.code = [keyCode integerValue];
            keyCombo.flags = [modifierFlags unsignedIntegerValue];
        }
    }

	[self setKeyCombo: keyCombo];
}

- (Class)valueClassForBinding:(NSString *)binding
{
	if ([binding isEqualToString:@"value"])
	{
		return [NSDictionary class];
	}
	
	return [super valueClassForBinding:binding];
}

#pragma mark *** Autosave Control ***

- (NSString *)autosaveName
{
	return [SRCell autosaveName];
}

- (void)setAutosaveName:(NSString *)aName
{
	[SRCell setAutosaveName: aName];
}

#pragma mark -

- (NSString *)keyComboString
{
	return [SRCell keyComboString];
}

#pragma mark *** Conversion Methods ***

- (NSUInteger)cocoaToCarbonFlags:(NSUInteger)cocoaFlags
{
	return SRCocoaToCarbonFlags( cocoaFlags );
}

- (NSUInteger)carbonToCocoaFlags:(NSUInteger)carbonFlags;
{
	return SRCarbonToCocoaFlags( carbonFlags );
}

#pragma mark *** Delegate pass-through ***

- (BOOL)shortcutRecorderCell:(SRRecorderCell *)aRecorderCell isKeyCode:(NSInteger)keyCode andFlagsTaken:(NSUInteger)flags reason:(NSString **)aReason
{
	if ([_delegate respondsToSelector: @selector(shortcutRecorder:isKeyCode:andFlagsTaken:reason:)])
	{
		return [_delegate shortcutRecorder:self isKeyCode:keyCode andFlagsTaken:flags reason:aReason];
	}
	else
	{
		return NO;
	}
}

#define NilOrNull(o) ((o) == nil || (id)(o) == [NSNull null])

- (void)shortcutRecorderCell:(SRRecorderCell *)aRecorderCell keyComboDidChange:(KeyCombo)newKeyCombo
{
	if ([_delegate respondsToSelector: @selector(shortcutRecorder:keyComboDidChange:)])
		[_delegate shortcutRecorder:self keyComboDidChange:newKeyCombo];

    // propagate view changes to binding (see http://www.tomdalling.com/cocoa/implementing-your-own-cocoa-bindings)
    NSDictionary *bindingInfo = [self infoForBinding:@"value"];
	if (!bindingInfo)
	{
		return;
	}
	
	// apply the value transformer, if one has been set
    NSDictionary *value = [self objectValue];
	NSDictionary *bindingOptions = bindingInfo[NSOptionsKey];
	if (bindingOptions != nil)
	{
		NSValueTransformer *transformer = [bindingOptions valueForKey:NSValueTransformerBindingOption];
		if (NilOrNull(transformer))
		{
			NSString *transformerName = [bindingOptions valueForKey:NSValueTransformerNameBindingOption];
			if (!NilOrNull(transformerName))
				transformer = [NSValueTransformer valueTransformerForName:transformerName];
		}

		if (!NilOrNull(transformer))
		{
			if ([[transformer class] allowsReverseTransformation])
			{
				value = [transformer reverseTransformedValue:value];
			}
			else
			{
				NSLog(@"WARNING: value has value transformer, but it doesn't allow reverse transformations in %s", __PRETTY_FUNCTION__);
			}
		}
	}

	id boundObject = bindingInfo[NSObservedObjectKey];
	if (NilOrNull(boundObject))
	{
		NSLog(@"ERROR: NSObservedObjectKey was nil for value binding in %s", __PRETTY_FUNCTION__);
		return;
	}

	NSString *boundKeyPath = bindingInfo[NSObservedKeyPathKey];
    if (NilOrNull(boundKeyPath))
	{
		NSLog(@"ERROR: NSObservedKeyPathKey was nil for value binding in %s", __PRETTY_FUNCTION__);
		return;
	}

	[boundObject setValue:value forKeyPath:boundKeyPath];
}

@end

@implementation SRRecorderControl (Private)

- (void)resetTrackingRects
{
	[SRCell resetTrackingRects];
}

@end
