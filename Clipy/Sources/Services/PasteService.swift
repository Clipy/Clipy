//
//  PasteService.swift
//
//  Clipy
//  GitHub: https://github.com/clipy
//  HP: https://clipy-app.com
//
//  Created by Econa77 on 2016/11/23.
//
//  Copyright © 2015-2018 Clipy Project.
//

import Cocoa
import Dependencies
import Foundation
import Sauce

final class PasteService {

    // MARK: - Properties
    private var pasteMenu: NSMenuItem? = {
        NSApp.mainMenu?.items
            .flatMap { $0.submenu?.items ?? [] }
            .first { $0.action == #selector(NSText.paste(_:)) }
    }()
    private let lock = NSRecursiveLock(name: "com.clipy-app.Clipy.Pastable")

    @Dependency(\.defaultAppStorage)
    private var appStorage

    private var isPastePlainText: Bool {
        guard appStorage.bool(forKey: Constants.Beta.pastePlainText) else { return false }

        let modifierSetting = appStorage.integer(forKey: Constants.Beta.pastePlainTextModifier)
        return isPressedModifier(modifierSetting)
    }
    private var isDeleteHistory: Bool {
        guard appStorage.bool(forKey: Constants.Beta.deleteHistory) else { return false }

        let modifierSetting = appStorage.integer(forKey: Constants.Beta.deleteHistoryModifier)
        return isPressedModifier(modifierSetting)
    }
    private var isPasteAndDeleteHistory: Bool {
        guard appStorage.bool(forKey: Constants.Beta.pasteAndDeleteHistory) else { return false }

        let modifierSetting = appStorage.integer(forKey: Constants.Beta.pasteAndDeleteHistoryModifier)
        return isPressedModifier(modifierSetting)
    }

    // MARK: - Modifiers
    private func isPressedModifier(_ flag: Int) -> Bool {
        let flags = NSEvent.modifierFlags
        if flag == 0 && flags.contains(.command) {
            return true
        } else if flag == 1 && flags.contains(.shift) {
            return true
        } else if flag == 2 && flags.contains(.control) {
            return true
        } else if flag == 3 && flags.contains(.option) {
            return true
        }
        return false
    }
}

// MARK: - Copy
extension PasteService {
    func paste(id: PasteboardHistory.ID, content: PasteboardContent) {
        // Handling modifier actions
        let isPastePlainText = self.isPastePlainText
        let isPasteAndDeleteHistory = self.isPasteAndDeleteHistory
        let isDeleteHistory = self.isDeleteHistory
        guard isPastePlainText || isPasteAndDeleteHistory || isDeleteHistory else {
            copyToPasteboard(with: content)
            paste()
            return
        }

        // Increment change count for don't copy paste item
        if isPasteAndDeleteHistory {
            AppEnvironment.current.clipService.incrementChangeCount()
        }
        // Paste history
        if isPastePlainText {
            copyToPasteboard(with: content.stringValue ?? "")
            paste()
        } else if isPasteAndDeleteHistory {
            copyToPasteboard(with: content)
            paste()
        }
        // Delete clip
        if isDeleteHistory || isPasteAndDeleteHistory {
            AppEnvironment.current.clipService.delete(id: id)
        }
    }

    func copyToPasteboard(with string: String) {
        lock.lock(); defer { lock.unlock() }

        let pasteboard = NSPasteboard.general
        pasteboard.declareTypes([.string], owner: nil)
        pasteboard.setString(string, forType: .string)
    }

    private func copyToPasteboard(with content: PasteboardContent) {
        lock.lock(); defer { lock.unlock() }

        if isPastePlainText {
            copyToPasteboard(with: content.stringValue ?? "")
            return
        }

        let pasteboard = NSPasteboard.general
        content.writeObjects(to: pasteboard)
    }
}

// MARK: - Paste
extension PasteService {
    func paste() {
        guard appStorage.bool(forKey: Constants.UserDefaults.inputPasteCommand) else { return }
        // Check Accessibility Permission
        guard Accessibility.isAccessibilityEnabled(isPrompt: false) else {
            Accessibility.showAccessibilityAuthenticationAlert()
            return
        }
        let modifier = pasteMenu?.keyEquivalentModifierMask ?? .command
        let keyCode = Sauce.shared.keyCode(for: pasteMenu?.key ?? .v, modifiers: .cocoa(modifier))
        let modifierFlags = CGEventFlags(rawValue: UInt64(modifier.rawValue))
        DispatchQueue.main.async {
            let source = CGEventSource(stateID: .combinedSessionState)
            // Disable local keyboard events while pasting
            source?.setLocalEventsFilterDuringSuppressionState([.permitLocalMouseEvents, .permitSystemDefinedEvents], state: .eventSuppressionStateSuppressionInterval)
            // Press paste keyboard shortcut
            let keyDown = CGEvent(keyboardEventSource: source, virtualKey: keyCode, keyDown: true)
            keyDown?.flags = modifierFlags
            // Release paste keyboard shortcut
            let keyUp = CGEvent(keyboardEventSource: source, virtualKey: keyCode, keyDown: false)
            keyUp?.flags = modifierFlags
            // Post Paste Command
            keyDown?.post(tap: .cgAnnotatedSessionEventTap)
            keyUp?.post(tap: .cgAnnotatedSessionEventTap)
        }
    }
}
