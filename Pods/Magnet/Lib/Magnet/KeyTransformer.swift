//
//  KeyTransformer.swift
//  Magnet
//
//  Created by 古林俊佑 on 2016/06/18.
//  Copyright © 2016年 Shunsuke Furubayashi. All rights reserved.
//

import Cocoa
import Carbon

public final class KeyTransformer {}

// MARK: - Cocoa & Carbon
public extension KeyTransformer {
    public static func cocoaFlags(from carbonFlags: Int) -> NSEvent.ModifierFlags {
        var cocoaFlags: NSEvent.ModifierFlags = NSEvent.ModifierFlags(rawValue: 0)

        if (carbonFlags & cmdKey) != 0 {
            cocoaFlags.insert(.command)
        }
        if (carbonFlags & optionKey) != 0 {
            cocoaFlags.insert(.option)
        }
        if (carbonFlags & controlKey) != 0 {
            cocoaFlags.insert(.control)
        }
        if (carbonFlags & shiftKey) != 0 {
            cocoaFlags.insert(.shift)
        }

        return cocoaFlags
    }

    public static func carbonFlags(from cocoaFlags: NSEvent.ModifierFlags) -> Int {
        var carbonFlags: Int = 0

        if cocoaFlags.contains(.command) {
            carbonFlags |= cmdKey
        }
        if cocoaFlags.contains(.option) {
            carbonFlags |= optionKey
        }
        if cocoaFlags.contains(.control) {
            carbonFlags |= controlKey
        }
        if cocoaFlags.contains(.shift) {
            carbonFlags |= shiftKey
        }

        return carbonFlags
    }

    public static func supportedCarbonFlags(_ carbonFlags: Int) -> Bool {
        return cocoaFlags(from: carbonFlags).rawValue != 0
    }

    public static func supportedCocoaFlags(_ cocoaFlogs: NSEvent.ModifierFlags) -> Bool {
        return carbonFlags(from: cocoaFlogs) != 0
    }

    public static func singleCarbonFlags(_ carbonFlags: Int) -> Bool {
        let commandSelected = (carbonFlags & cmdKey) != 0
        let optionSelected  = (carbonFlags & optionKey) != 0
        let controlSelected = (carbonFlags & controlKey) != 0
        let shiftSelected   = (carbonFlags & shiftKey) != 0
        let hash = commandSelected.intValue + optionSelected.intValue + controlSelected.intValue + shiftSelected.intValue
        return hash == 1
    }

    public static func singleCocoaFlags(_ cocoaFlags: NSEvent.ModifierFlags) -> Bool {
        let commandSelected = cocoaFlags.contains(.command)
        let optionSelected  = cocoaFlags.contains(.option)
        let controlSelected = cocoaFlags.contains(.control)
        let shiftSelected   = cocoaFlags.contains(.shift)
        let hash = commandSelected.intValue + optionSelected.intValue + controlSelected.intValue + shiftSelected.intValue
        return hash == 1
    }
}

// MARK: - Function
public extension KeyTransformer {
    public static func containsFunctionKey(_ keyCode: Int) -> Bool {
        switch keyCode {
        case kVK_F1: fallthrough
        case kVK_F2: fallthrough
        case kVK_F3: fallthrough
        case kVK_F4: fallthrough
        case kVK_F5: fallthrough
        case kVK_F6: fallthrough
        case kVK_F7: fallthrough
        case kVK_F8: fallthrough
        case kVK_F9: fallthrough
        case kVK_F10: fallthrough
        case kVK_F11: fallthrough
        case kVK_F12: fallthrough
        case kVK_F13: fallthrough
        case kVK_F14: fallthrough
        case kVK_F15: fallthrough
        case kVK_F16: fallthrough
        case kVK_F17: fallthrough
        case kVK_F18: fallthrough
        case kVK_F19: fallthrough
        case kVK_F20:
            return true
        default:
            return false
        }
    }
}

// MARK: - Modifiers
public extension KeyTransformer {
    public static func modifiersToString(_ carbonModifiers: Int) -> [String] {
        var strings = [String]()

        if (carbonModifiers & cmdKey) != 0 {
            strings.append("⌘")
        }
        if (carbonModifiers & optionKey) != 0 {
            strings.append("⌥")
        }
        if (carbonModifiers & controlKey) != 0 {
            strings.append("⌃")
        }
        if (carbonModifiers & shiftKey) != 0 {
            strings.append("⇧")
        }

        return strings
    }

    public static func modifiersToString(_ cocoaModifiers: NSEvent.ModifierFlags) -> [String] {
        var strings = [String]()

        if cocoaModifiers.contains(.command) {
            strings.append("⌘")
        }
        if cocoaModifiers.contains(.option) {
            strings.append("⌥")
        }
        if cocoaModifiers.contains(.control) {
            strings.append("⌃")
        }
        if cocoaModifiers.contains(.shift) {
            strings.append("⇧")
        }

        return strings
    }
}
