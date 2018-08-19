//
//  Sauce.swift
//
//  Sauce
//  GitHub: https://github.com/clipy
//  HP: https://clipy-app.com
//
//  Created by Econa77 on 2018/07/30.
//
//  Copyright © 2018 Clipy Project.
//

import Foundation

public extension NSNotification.Name {
    public static let SauceSelectedKeyboardInputSourceChanged = Notification.Name("SauceSelectedKeyboardInputSourceChanged")
    public static let SauceEnabledKeyboardInputSoucesChanged = Notification.Name("SauceEnabledKeyboardInputSoucesChanged")
}

public final class Sauce {

    // MARK: - Properties
    public static let shared = Sauce()

    private let layout: KeyboardLayout

    // MARK: - Initialize
    init(layout: KeyboardLayout = KeyboardLayout()) {
        self.layout = layout
    }

}

// MARK: - KeyCodes
public extension Sauce {
    public func keyCode(by key: Key) -> CGKeyCode {
        return currentKeyCode(by: key) ?? currentASCIICapableKeyCode(by: key) ?? key.QWERTYKeyCode
    }

    public func currentKeyCode(by key: Key) -> CGKeyCode? {
        return layout.currentKeyCode(by: key)
    }

    public func currentASCIICapableKeyCode(by key: Key) -> CGKeyCode? {
        return layout.currentASCIICapableKeyCode(by: key)
    }

    public func keyCode(with source: InputSource, key: Key) -> CGKeyCode? {
        return layout.keyCode(with: source, key: key)
    }

    public func ASCIICapableInputSources() -> [InputSource] {
        return layout.ASCIICapableInputSources
    }
}

// MARK: - Characters
public extension Sauce {
    public func character(by keyCode: Int, modifiers: Int) -> String? {
        return currentCharacter(by: keyCode, modifiers: modifiers) ?? currentASCIICapableCharacter(by: keyCode, modifiers: modifiers)
    }

    public func currentCharacter(by keyCode: Int, modifiers: Int) -> String? {
        return layout.currentCharacter(by: keyCode, modifiers: modifiers)
    }

    public func currentASCIICapableCharacter(by keyCode: Int, modifiers: Int) -> String? {
        return layout.currentASCIICapableCharacter(by: keyCode, modifiers: modifiers)
    }

    public func character(with source: InputSource, keyCode: Int, modifiers: Int) -> String? {
        return layout.character(with: source, keyCode: keyCode, modifiers: modifiers)
    }
}
