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
    public func currentKeyCode(by key: Key) -> CGKeyCode {
        return layout.currentKeyCode(by: key) ?? layout.currentASCIICapableKeyCode(by: key) ?? key.QWERTYKeyCode
    }

    public func keyCode(with source: InputSource, key: Key) -> CGKeyCode? {
        return layout.keyCode(with: source, key: key)
    }

    public func ASCIICapableInputSources() -> [InputSource] {
        return layout.ASCIICapableInputSources
    }
}
