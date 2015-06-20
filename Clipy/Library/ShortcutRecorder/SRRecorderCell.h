//
//  SRRecorderCell.h
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
#import "SRCommon.h"

#define SRMinWidth 50
#define SRMaxHeight 22

#define SRTransitionFPS 30.0f
#define SRTransitionDuration 0.35f
//#define SRTransitionDuration 2.35
#define SRTransitionFrames (SRTransitionFPS*SRTransitionDuration)
#define SRAnimationAxisIsY YES
#define ShortcutRecorderNewStyleDrawing

#define SRAnimationOffsetRect(X,Y)	(SRAnimationAxisIsY ? NSOffsetRect(X,0.0f,-NSHeight(Y)) : NSOffsetRect(X,NSWidth(Y),0.0f))

@class SRRecorderControl, SRValidator;
@protocol SRRecorderCellDelegate;

enum SRRecorderStyle
{
    SRGradientBorderStyle = 0,
    SRGreyStyle = 1,
	SRGreyStyleAnimated = 2
	
};
typedef enum SRRecorderStyle SRRecorderStyle;

@interface SRRecorderCell : NSActionCell <NSCoding>
{	
	NSGradient          *recordingGradient;
	
	BOOL                isRecording;
	BOOL                mouseInsideTrackingArea;
	BOOL                mouseDown;
	
	SRRecorderStyle		style;
	
	BOOL				isAnimating;
	CGFloat				transitionProgress;
	BOOL				isAnimatingNow;
	BOOL				isAnimatingTowardsRecording;
	BOOL				comboJustChanged;
	
	NSTrackingRectTag   removeTrackingRectTag;
	NSTrackingRectTag   snapbackTrackingRectTag;
	
	KeyCombo            keyCombo;
	BOOL				hasKeyChars;
	NSString		    *keyChars;
	NSString		    *keyCharsIgnoringModifiers;
	
	NSUInteger        allowedFlags;
	NSUInteger        requiredFlags;
	NSUInteger        recordingFlags;
	
	BOOL				allowsKeyOnly;
	BOOL				escapeKeysRecord;
	
	NSSet               *cancelCharacterSet;
	
    SRValidator         *validator;
    
	BOOL				globalHotKeys;
	void				*hotKeyModeToken;
}

- (void)resetTrackingRects;

#pragma mark *** Aesthetics ***

+ (BOOL)styleSupportsAnimation:(SRRecorderStyle)style;

@property (nonatomic) BOOL animates;
@property (nonatomic) SRRecorderStyle style;

#pragma mark *** Delegate ***

@property (nonatomic, weak) IBOutlet id<SRRecorderCellDelegate> delegate;

#pragma mark *** Responder Control ***

@property (nonatomic, readonly) BOOL becomeFirstResponder;
@property (nonatomic, readonly) BOOL resignFirstResponder;

#pragma mark *** Key Combination Control ***

- (BOOL)performKeyEquivalent:(NSEvent *)theEvent;
- (void)flagsChanged:(NSEvent *)theEvent;

@property (nonatomic) NSUInteger allowedFlags;

@property (nonatomic) NSUInteger requiredFlags;

@property (nonatomic) BOOL allowsKeyOnly;
- (void)setAllowsKeyOnly:(BOOL)nAllowsKeyOnly escapeKeysRecord:(BOOL)nEscapeKeysRecord;
@property (nonatomic) BOOL escapeKeysRecord;

@property (nonatomic) BOOL canCaptureGlobalHotKeys;

@property (nonatomic) KeyCombo keyCombo;

#pragma mark *** Autosave Control ***

@property (nonatomic, copy) NSString *autosaveName;

// Returns the displayed key combination if set
@property (nonatomic, readonly, copy) NSString *keyComboString;

@property (nonatomic, readonly, copy) NSString *keyChars;
@property (nonatomic, readonly, copy) NSString *keyCharsIgnoringModifiers;

@end

// Delegate Methods
@protocol SRRecorderCellDelegate <NSObject>

- (BOOL)shortcutRecorderCell:(SRRecorderCell *)aRecorderCell isKeyCode:(NSInteger)keyCode andFlagsTaken:(NSUInteger)flags reason:(NSString **)aReason;
- (void)shortcutRecorderCell:(SRRecorderCell *)aRecorderCell keyComboDidChange:(KeyCombo)newCombo;

@end
