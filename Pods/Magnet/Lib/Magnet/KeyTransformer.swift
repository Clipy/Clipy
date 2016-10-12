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
    public static func carbonToCocoaFlags(carbonFlags: Int) -> NSEventModifierFlags {
        var cocoaFlags: NSEventModifierFlags = NSEventModifierFlags(rawValue: 0)

        if (carbonFlags & cmdKey) != 0 {
            cocoaFlags.insert(.CommandKeyMask)
        }
        if (carbonFlags & optionKey) != 0 {
            cocoaFlags.insert(.AlternateKeyMask)
        }
        if (carbonFlags & controlKey) != 0 {
            cocoaFlags.insert(.ControlKeyMask)
        }
        if (carbonFlags & shiftKey) != 0 {
            cocoaFlags.insert(.ShiftKeyMask)
        }

        return cocoaFlags
    }

    public static func cocoaToCarbonFlags(cocoaFlags: NSEventModifierFlags) -> Int {
        var carbonFlags: Int = 0

        if cocoaFlags.contains(.CommandKeyMask) {
            carbonFlags |= cmdKey
        }
        if cocoaFlags.contains(.AlternateKeyMask) {
            carbonFlags |= optionKey
        }
        if cocoaFlags.contains(.ControlKeyMask) {
            carbonFlags |= controlKey
        }
        if cocoaFlags.contains(.ShiftKeyMask) {
            carbonFlags |= shiftKey
        }

        return carbonFlags
    }

    public static func supportedCarbonFlags(carbonFlags: Int) -> Bool {
        return carbonToCocoaFlags(carbonFlags).rawValue != 0
    }

    public static func supportedCocoaFlags(cocoaFlogs: NSEventModifierFlags) -> Bool {
        return cocoaToCarbonFlags(cocoaFlogs) != 0
    }

    public static func singleCarbonFlags(carbonFlags: Int) -> Bool {
        let commandSelected = (carbonFlags & cmdKey) != 0
        let altSelected     = (carbonFlags & optionKey) != 0
        let controlSelected = (carbonFlags & controlKey) != 0
        let shiftSelected   = (carbonFlags & shiftKey) != 0
        let hash = commandSelected.hashValue + altSelected.hashValue + controlSelected.hashValue + shiftSelected.hashValue
        return hash == 1
    }

    public static func singleCocoaFlags(cocoaFlags: NSEventModifierFlags) -> Bool {
        let commandSelected = cocoaFlags.contains(.CommandKeyMask)
        let altSelected     = cocoaFlags.contains(.AlternateKeyMask)
        let controlSelected = cocoaFlags.contains(.ControlKeyMask)
        let shiftSelected   = cocoaFlags.contains(.ShiftKeyMask)
        let hash = commandSelected.hashValue + altSelected.hashValue + controlSelected.hashValue + shiftSelected.hashValue
        return hash == 1
    }
}

// MARK: - Function
public extension KeyTransformer {
    public static func containsFunctionKey(keyCode: Int) -> Bool {
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
    public static func modifiersToString(carbonModifiers: Int) -> [String] {
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

    public static func modifiersToString(cocoaModifiers: NSEventModifierFlags) -> [String] {
        var strings = [String]()

        if cocoaModifiers.contains(.CommandKeyMask) {
            strings.append("⌘")
        }
        if cocoaModifiers.contains(.AlternateKeyMask) {
            strings.append("⌥")
        }
        if cocoaModifiers.contains(.ControlKeyMask) {
            strings.append("⌃")
        }
        if cocoaModifiers.contains(.ShiftKeyMask) {
            strings.append("⇧")
        }

        return strings
    }
}
