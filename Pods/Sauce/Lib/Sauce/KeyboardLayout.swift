//
//  KeyboardLayout.swift
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
import Carbon

final class KeyboardLayout {

    // MARK: - Properties
    private(set) var currentInputSource: InputSource
    private(set) var currentASCIICapableInputSouce: InputSource
    private(set) var ASCIICapableInputSources = [InputSource]()
    private(set) var mappedKeyCodes = [InputSource: [Key: CGKeyCode]]()

    var currentKeyCodes: [Key: CGKeyCode]? {
        return mappedKeyCodes[currentInputSource]
    }

    private let distributedNotificationCenter: DistributedNotificationCenter
    private let notificationCenter: NotificationCenter

    // MARK: - Initialize
    init(distributedNotificationCenter: DistributedNotificationCenter = .default(), notificationCenter: NotificationCenter = .default) {
        self.distributedNotificationCenter = distributedNotificationCenter
        self.notificationCenter = notificationCenter
        self.currentInputSource = InputSource(source: TISCopyCurrentKeyboardInputSource().takeUnretainedValue())
        self.currentASCIICapableInputSouce = InputSource(source: TISCopyCurrentASCIICapableKeyboardInputSource().takeUnretainedValue())
        fetchASCIICapableInputSources()
        observeNotifications()
    }

    deinit {
        distributedNotificationCenter.removeObserver(self)
    }

}

// MARK: - KeyCodes
extension KeyboardLayout {
    func currentKeyCode(by key: Key) -> CGKeyCode? {
        return currentKeyCodes?[key]
    }

    func keyCodes(with source: InputSource) -> [Key: CGKeyCode]? {
        return mappedKeyCodes[source]
    }

    func keyCode(with source: InputSource, key: Key) -> CGKeyCode? {
        return mappedKeyCodes[source]?[key]
    }

    func currentASCIICapableKeyCode(by key: Key) -> CGKeyCode? {
        return keyCode(with: currentASCIICapableInputSouce, key: key)
    }
}

// MARK: - Characters
extension KeyboardLayout {
    func currentCharacter(by keyCode: Int, modifiers: Int) -> String? {
        return character(with: currentInputSource.source, keyCode: keyCode, modifiers: modifiers)
    }

    func character(with source: InputSource, keyCode: Int, modifiers: Int) -> String? {
        return character(with: source.source, keyCode: keyCode, modifiers: modifiers)
    }

    func currentASCIICapableCharacter(by keyCode: Int, modifiers: Int) -> String? {
        return character(with: currentASCIICapableInputSouce.source, keyCode: keyCode, modifiers: modifiers)
    }
}

// MARK: - Notifications
extension KeyboardLayout {
    private func observeNotifications() {
        distributedNotificationCenter.addObserver(self,
                                                  selector: #selector(selectedKeyboardInputSourceChanged),
                                                  name: NSNotification.Name(kTISNotifySelectedKeyboardInputSourceChanged as String),
                                                  object: nil)
        distributedNotificationCenter.addObserver(self,
                                                  selector: #selector(enabledKeyboardInputSourcesChanged),
                                                  name: Notification.Name(kTISNotifyEnabledKeyboardInputSourcesChanged as String),
                                                  object: nil)
    }

    @objc func selectedKeyboardInputSourceChanged() {
        let source = InputSource(source: TISCopyCurrentKeyboardInputSource().takeUnretainedValue())
        guard source != currentInputSource else { return }
        self.currentInputSource = source
        self.currentASCIICapableInputSouce = InputSource(source: TISCopyCurrentASCIICapableKeyboardInputSource().takeUnretainedValue())
        notificationCenter.post(name: .SauceSelectedKeyboardInputSourceChanged, object: nil)
    }

    @objc func enabledKeyboardInputSourcesChanged() {
        fetchASCIICapableInputSources()
        notificationCenter.post(name: .SauceEnabledKeyboardInputSoucesChanged, object: nil)
    }
}

// MAKR: - Layouts
private extension KeyboardLayout {
    func fetchASCIICapableInputSources() {
        ASCIICapableInputSources = []
        mappedKeyCodes = [:]
        guard let sources = TISCreateASCIICapableInputSourceList().takeUnretainedValue() as? [TISInputSource] else { return }
        ASCIICapableInputSources = sources.map { InputSource(source: $0) }
        ASCIICapableInputSources.forEach { mappingKeyCodes(with: $0) }
    }

    func mappingKeyCodes(with source: InputSource) {
        var keyCodes = [Key: CGKeyCode]()
        // Scan key codes
        guard let layoutData = TISGetInputSourceProperty(source.source, kTISPropertyUnicodeKeyLayoutData) else { return }
        let data = Unmanaged<CFData>.fromOpaque(layoutData).takeUnretainedValue() as Data

        for i in 0..<128 {
            guard let character = character(with: data, keyCode: i, modifiers: 0) else { continue }
            guard let key = Key(character: character) else { continue }
            keyCodes[key] = CGKeyCode(i)
        }
        mappedKeyCodes[source] = keyCodes
    }

    func character(with source: TISInputSource, keyCode: Int, modifiers: Int) -> String? {
        guard let layoutData = TISGetInputSourceProperty(source, kTISPropertyUnicodeKeyLayoutData) else { return nil }
        let data = Unmanaged<CFData>.fromOpaque(layoutData).takeUnretainedValue() as Data
        return character(with: data, keyCode: keyCode, modifiers: modifiers)
    }

    func character(with layoutData: Data, keyCode: Int, modifiers: Int) -> String? {
        // In the case of the special key code, it does not depend on the keyboard layout
        if let specialKeyCode = SpecialKeyCode(keyCode: keyCode) { return specialKeyCode.character }

        var deadKeyState: UInt32 = 0
        let maxChars = 256
        var chars = [UniChar](repeating: 0, count: maxChars)
        var length = 0
        let error = layoutData.withUnsafeBytes { (ptr: UnsafeRawBufferPointer) -> OSStatus in
            let keyboardPtr = ptr.bindMemory(to: UCKeyboardLayout.self)
            //var kb = ptr.load(as: UCKeyboardLayout.self)
            return CoreServices.UCKeyTranslate(keyboardPtr.baseAddress,
                                               UInt16(keyCode),
                                               UInt16(CoreServices.kUCKeyActionDisplay),
                                               UInt32(modifiers),
                                               UInt32(LMGetKbdType()),
                                               OptionBits(CoreServices.kUCKeyTranslateNoDeadKeysBit),
                                               &deadKeyState,
                                               maxChars,
                                               &length,
                                               &chars)
        }
//        let error = layoutData.withUnsafeBytes { (bytes: UnsafePointer<UCKeyboardLayout>) in
//            return CoreServices.UCKeyTranslate(bytes,
//                                               UInt16(keyCode),
//                                               UInt16(CoreServices.kUCKeyActionDisplay),
//                                               UInt32(modifiers),
//                                               UInt32(LMGetKbdType()),
//                                               OptionBits(CoreServices.kUCKeyTranslateNoDeadKeysBit),
//                                               &deadKeyState,
//                                               maxChars,
//                                               &length,
//                                               &chars)
//        }
        guard error == noErr else { return nil }
        return NSString(characters: &chars, length: length) as String
    }
}
