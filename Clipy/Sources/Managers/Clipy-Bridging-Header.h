//
//  Use this file to import your target's public headers that you would like to expose to Swift.
//
#import <Cocoa/Cocoa.h>

// Adds undocumented "appearance" argument to "popUpMenuPositioningItem":
@interface NSMenu (MISSINGOrder)
- (BOOL)popUpMenuPositioningItem:(nullable NSMenuItem *)item atLocation:(NSPoint)location inView:(nullable NSView *)view appearance:(nullable NSAppearance *)appearance NS_AVAILABLE_MAC(10_6);
@end
