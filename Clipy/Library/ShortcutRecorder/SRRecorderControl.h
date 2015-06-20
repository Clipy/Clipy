//
//  SRRecorderControl.h
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

#import <Cocoa/Cocoa.h>
#import "SRRecorderCell.h"

@protocol SRRecorderDelegate;

@interface SRRecorderControl : NSControl <SRRecorderCellDelegate>
{
}

#pragma mark *** Aesthetics ***
@property (nonatomic) BOOL animates;
@property (nonatomic) SRRecorderStyle style;

#pragma mark *** Delegate ***
@property (nonatomic, weak) IBOutlet id<SRRecorderDelegate> delegate;

#pragma mark *** Key Combination Control ***

@property (nonatomic) NSUInteger allowedFlags;

@property (nonatomic) BOOL allowsKeyOnly;
- (void)setAllowsKeyOnly:(BOOL)nAllowsKeyOnly escapeKeysRecord:(BOOL)nEscapeKeysRecord;
@property (nonatomic) BOOL escapeKeysRecord;

@property (nonatomic) BOOL canCaptureGlobalHotKeys;

@property (nonatomic) NSUInteger requiredFlags;

@property (nonatomic) KeyCombo keyCombo;

@property (nonatomic, readonly, copy) NSString *keyChars;
@property (nonatomic, readonly, copy) NSString *keyCharsIgnoringModifiers;

#pragma mark *** Autosave Control ***

@property (nonatomic, copy) NSString *autosaveName;

#pragma mark -

// Returns the displayed key combination if set
@property (nonatomic, readonly, copy) NSString *keyComboString;

#pragma mark *** Conversion Methods ***

- (NSUInteger)cocoaToCarbonFlags:(NSUInteger)cocoaFlags;
- (NSUInteger)carbonToCocoaFlags:(NSUInteger)carbonFlags;

#pragma mark *** Binding Methods ***

@end

// Delegate Methods
@protocol SRRecorderDelegate <NSObject>

- (BOOL)shortcutRecorder:(SRRecorderControl *)aRecorder isKeyCode:(NSInteger)keyCode andFlagsTaken:(NSUInteger)flags reason:(NSString **)aReason;
- (void)shortcutRecorder:(SRRecorderControl *)aRecorder keyComboDidChange:(KeyCombo)newKeyCombo;

@end
