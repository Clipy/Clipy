//
//  PTHotKey+ShortcutRecorder.h
//  ShortcutRecorder
//
//  Created by Ilya Kulakov on 27.02.11.
//  Copyright 2011 Wireload. All rights reserved.
//

#import <Cocoa/Cocoa.h>
#import "PTHotKey.h"


@interface PTHotKey (ShortcutRecorder)

+ (PTHotKey *)hotKeyWithIdentifier:(id)anIdentifier
                          keyCombo:(NSDictionary *)aKeyCombo
                            target:(id)aTarget
                            action:(SEL)anAction;

+ (PTHotKey *)hotKeyWithIdentifier:(id)anIdentifier
                          keyCombo:(NSDictionary *)aKeyCombo
                            target:(id)aTarget
                            action:(SEL)anAction
                        keyUpAction:(SEL)aKeyUpAction;

+ (PTHotKey *)hotKeyWithIdentifier:(id)anIdentifier
                          keyCombo:(NSDictionary *)aKeyCombo
                            target:(id)aTarget
                            action:(SEL)anAction
                        withObject:(id)anObject;
@end
