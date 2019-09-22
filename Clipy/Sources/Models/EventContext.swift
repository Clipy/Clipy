// 
//  NSEventContext.swift
//
//  Clipy
//  GitHub: https://github.com/clipy
//  HP: https://clipy-app.com
// 
//  Created by 胡继续 on 2019/9/21.
// 
//  Copyright © 2015-2019 Clipy Project.
//

import Foundation

class EventContext {

    let modifierFlags: NSEvent.ModifierFlags
    let keyRepeatDelay: TimeInterval
    let keyRepeatInterval: TimeInterval
    let pressedMouseButtons: Int
    let doubleClickInterval: TimeInterval
    let mouseLocation: NSPoint
    let isMouseCoalescingEnabled: Bool
    let isSwipeTrackingFromScrollEventsEnabled: Bool

    init(
        modifierFlags: NSEvent.ModifierFlags = NSEvent.modifierFlags,
        keyRepeatDelay: TimeInterval = NSEvent.keyRepeatDelay,
        keyRepeatInterval: TimeInterval = NSEvent.keyRepeatInterval,
        pressedMouseButtons: Int = NSEvent.pressedMouseButtons,
        doubleClickInterval: TimeInterval = NSEvent.doubleClickInterval,
        mouseLocation: NSPoint = NSEvent.mouseLocation,
        isMouseCoalescingEnabled: Bool = NSEvent.isMouseCoalescingEnabled,
        isSwipeTrackingFromScrollEventsEnabled: Bool = NSEvent.isSwipeTrackingFromScrollEventsEnabled
    ) {
        self.modifierFlags = modifierFlags
        self.keyRepeatDelay = keyRepeatDelay
        self.keyRepeatInterval = keyRepeatInterval
        self.pressedMouseButtons = pressedMouseButtons
        self.doubleClickInterval = doubleClickInterval
        self.mouseLocation = mouseLocation
        self.isMouseCoalescingEnabled = isMouseCoalescingEnabled
        self.isSwipeTrackingFromScrollEventsEnabled = isSwipeTrackingFromScrollEventsEnabled
    }
}
