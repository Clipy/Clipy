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
import Sharing

final class PasteService {

    // MARK: - Properties
    private var pasteMenu: NSMenuItem? = {
        NSApp.mainMenu?.items
            .flatMap { $0.submenu?.items ?? [] }
            .first { $0.action == #selector(NSText.paste(_:)) }
    }()
    private let lock = NSRecursiveLock(name: "com.clipy-app.Clipy.Pastable")

    @Dependency(\.clipService)
    private var clipService
    @Dependency(\.pasteboardHistoryRepository)
    private var pasteboardHistoryRepository
    @Dependency(\.snippetRepository)
    private var snippetRepository

    @Shared(.pastesPlainTextWithModifier)
    private var pastesPlainTextWithModifier
    @Shared(.plainTextPasteModifier)
    private var plainTextPasteModifier
    @Shared(.deletesHistoryWithModifier)
    private var deletesHistoryWithModifier
    @Shared(.historyDeletionModifier)
    private var historyDeletionModifier
    @Shared(.pastesAndDeletesHistoryWithModifier)
    private var pastesAndDeletesHistoryWithModifier
    @Shared(.pasteAndDeleteHistoryModifier)
    private var pasteAndDeleteHistoryModifier
    @Shared(.pastesAutomatically)
    private var pastesAutomatically

    // MARK: - Modifiers
    private func isPressedModifier(_ setting: Int, in flags: NSEvent.ModifierFlags) -> Bool {
        switch setting {
        case 0:
            flags.contains(.command)
        case 1:
            flags.contains(.shift)
        case 2:
            flags.contains(.control)
        case 3:
            flags.contains(.option)
        default:
            false
        }
    }
}

// MARK: - Copy
extension PasteService {
    func paste(
        id: PasteboardHistory.ID,
        modifierFlags: NSEvent.ModifierFlags = NSEvent.modifierFlags
    ) {
        guard let content = pasteboardHistoryRepository.fetchContent(id: id) else {
            NSSound.beep()
            return
        }
        // Handling modifier actions
        let isPastePlainText = pastesPlainTextWithModifier && isPressedModifier(plainTextPasteModifier, in: modifierFlags)
        let isPasteAndDeleteHistory = pastesAndDeletesHistoryWithModifier && isPressedModifier(pasteAndDeleteHistoryModifier, in: modifierFlags)
        let isDeleteHistory = deletesHistoryWithModifier && isPressedModifier(historyDeletionModifier, in: modifierFlags)
        guard isPastePlainText || isPasteAndDeleteHistory || isDeleteHistory else {
            copyToPasteboard(with: content)
            paste()
            return
        }

        // Increment change count for don't copy paste item
        if isPasteAndDeleteHistory {
            clipService.incrementChangeCount()
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
            clipService.delete(id: id)
        }
    }

    func paste(id: Snippet.ID) {
        guard let snippet = snippetRepository.fetchSnippet(id: id), snippet.isEnabled else {
            NSSound.beep()
            return
        }
        copyToPasteboard(with: snippet.content)
        paste()
    }

    private func copyToPasteboard(with string: String) {
        lock.lock(); defer { lock.unlock() }

        let pasteboard = NSPasteboard.general
        pasteboard.declareTypes([.string], owner: nil)
        pasteboard.setString(string, forType: .string)
    }

    private func copyToPasteboard(with content: PasteboardContent) {
        lock.lock(); defer { lock.unlock() }

        let pasteboard = NSPasteboard.general
        content.writeObjects(to: pasteboard)
    }
}

// MARK: - Paste
extension PasteService {
    func paste() {
        guard pastesAutomatically else { return }
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

extension DependencyValues {
    var pasteService: PasteService {
        get { self[PasteServiceKey.self] }
        set { self[PasteServiceKey.self] = newValue }
    }

    private enum PasteServiceKey: DependencyKey {
        static let liveValue = PasteService()
    }
}
