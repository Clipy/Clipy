//
//  KeyCodeTransformer.swift
//  Magnet
//
//  Created by 古林俊佑 on 2016/06/26.
//  Copyright © 2016年 Shunsuke Furubayashi. All rights reserved.
//

import Foundation
import Carbon

public class KeyCodeTransformer {
    // MARK: - Properties
    static let sharedTransformer = KeyCodeTransformer()
}

// MARK: - Transform
public extension KeyCodeTransformer {
    public func transformValue(keyCode: Int, carbonModifiers: Int) -> String {
        return transformValue(keyCode, modifiers: carbonModifiers)
    }

    public func transformValue(keyCode: Int, cocoaModifiers: NSEventModifierFlags) -> String {
        return transformValue(keyCode, modifiers: KeyTransformer.cocoaToCarbonFlags(cocoaModifiers))
    }

    private func transformValue(keyCode: Int, modifiers: Int) -> String {
        // Return Special KeyCode
        if let unmappedString = transformSpecialKeyCode(keyCode) {
            return unmappedString
        }

        let source = TISCopyCurrentASCIICapableKeyboardLayoutInputSource().takeUnretainedValue()
        let layoutData = TISGetInputSourceProperty(source, kTISPropertyUnicodeKeyLayoutData)
        let dataRef = unsafeBitCast(layoutData, CFDataRef.self)

        let keyLayout = unsafeBitCast(CFDataGetBytePtr(dataRef), UnsafePointer<CoreServices.UCKeyboardLayout>.self)

        let keyTranslateOptions = OptionBits(CoreServices.kUCKeyTranslateNoDeadKeysBit)
        var deadKeyState: UInt32 = 0
        let maxChars = 256
        var chars = [UniChar](count:maxChars, repeatedValue:0)
        var length = 0

        let error = CoreServices.UCKeyTranslate(keyLayout,
                                                UInt16(keyCode),
                                                UInt16(CoreServices.kUCKeyActionDisplay),
                                                UInt32(modifiers),
                                                UInt32(LMGetKbdType()),
                                                keyTranslateOptions,
                                                &deadKeyState,
                                                maxChars,
                                                &length,
                                                &chars)

        if error != noErr { return "" }

        return NSString(characters: &chars, length: length).uppercaseString
    }

    private func transformSpecialKeyCode(keyCode: Int) -> String? {
        return specialKeyCodeStrings[keyCode]
    }
}

// MARK: - Mapping
private extension KeyCodeTransformer {
    private var specialKeyCodeStrings: [Int: String] {
        return [
            kVK_F1: "F1",
            kVK_F2: "F2",
            kVK_F3: "F3",
            kVK_F4: "F4",
            kVK_F5: "F5",
            kVK_F6: "F6",
            kVK_F7: "F7",
            kVK_F8: "F8",
            kVK_F9: "F9",
            kVK_F10: "F10",
            kVK_F11: "F11",
            kVK_F12: "F12",
            kVK_F13: "F13",
            kVK_F14: "F14",
            kVK_F15: "F15",
            kVK_F16: "F16",
            kVK_F17: "F17",
            kVK_F18: "F18",
            kVK_F19: "F19",
            kVK_F20: "F20",
            kVK_Space: "Space",
            kVK_Delete: unicharToString(0x232B),            // ⌫
            kVK_ForwardDelete: unicharToString(0x2326),     // ⌦
            kVK_ANSI_Keypad0: unicharToString(0x2327),      // ⌧
            kVK_LeftArrow: unicharToString(0x2190),         // ←
            kVK_RightArrow: unicharToString(0x2192),        // →
            kVK_UpArrow: unicharToString(0x2191),           // ↑
            kVK_DownArrow: unicharToString(0x2193),         // ↓
            kVK_End: unicharToString(0x2198),               // ↘
            kVK_Home: unicharToString(0x2196),              // ↖
            kVK_Escape: unicharToString(0x238B),            // ⎋
            kVK_PageDown: unicharToString(0x21DF),          // ⇟
            kVK_PageUp: unicharToString(0x21DE),            // ⇞
            kVK_Return: unicharToString(0x21A9),            // ↩
            kVK_ANSI_KeypadEnter: unicharToString(0x2305),  // ⌅
            kVK_Tab: unicharToString(0x21E5),               // ⇥
            kVK_Help: "?⃝"
        ]
    }
}

// MARK: - Charactor
private extension KeyCodeTransformer {
    private func unicharToString(char: unichar) -> String {
        return String(format: "%C", char)
    }
}
