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
    @Shared(.reordersClipsAfterPasting)
    private var reordersClipsAfterPasting

    private var isPastePlainText: Bool {
        guard pastesPlainTextWithModifier else { return false }
        return isPressedModifier(plainTextPasteModifier)
    }
    private var isDeleteHistory: Bool {
        guard deletesHistoryWithModifier else { return false }
        return isPressedModifier(historyDeletionModifier)
    }
    private var isPasteAndDeleteHistory: Bool {
        guard pastesAndDeletesHistoryWithModifier else { return false }
        return isPressedModifier(pasteAndDeleteHistoryModifier)
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

    /// Pastes the history item at `index` (0 = most recent) using the same ordering as the history menu.
    /// Modifier actions (plain text, delete) are skipped because the triggering hotkey's own modifiers are still held.
    /// Returns `false` when no item exists at `index`.
    @discardableResult
    func pasteHistoryItem(at index: Int) -> Bool {
        let historyDetails = pasteboardHistoryRepository.fetchHistoryDetails(
            sortsByCreatedAt: !reordersClipsAfterPasting,
            includesThumbnailAsset: false,
            limit: index + 1
        )
        guard historyDetails.indices.contains(index),
              let content = pasteboardHistoryRepository.fetchContent(id: historyDetails[index].history.id) else {
            NSSound.beep()
            return false
        }
        writeToPasteboard(content)
        paste()
        return true
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

        writeToPasteboard(content)
    }

    private func writeToPasteboard(_ content: PasteboardContent) {
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
