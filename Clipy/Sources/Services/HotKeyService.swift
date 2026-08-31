//
//  HotKeyService.swift
//
//  Clipy
//  GitHub: https://github.com/clipy
//  HP: https://clipy-app.com
//
//  Created by Econa77 on 2016/11/19.
//
//  Copyright © 2015-2018 Clipy Project.
//

import Cocoa
import Dependencies
import Foundation
import Magnet
import Sharing

final class HotKeyService: NSObject {
    // MARK: - Properties
    @Dependency(\.snippetRepository)
    private var snippetRepository
    @Dependency(\.firebase)
    private var firebase
    @Dependency(\.menuManager)
    private var menuManager
    @Dependency(\.clipService)
    private var clipService

    @Shared(.mainKeyCombo)
    private var mainKeyCombo
    @Shared(.historyKeyCombo)
    private var historyKeyCombo
    @Shared(.snippetKeyCombo)
    private var snippetKeyCombo
    @Shared(.editSnippetsKeyCombo)
    private var editSnippetsKeyCombo
    @Shared(.clearHistoryKeyCombo)
    private var clearHistoryKeyCombo
    @Shared(.folderKeyCombos)
    private var folderKeyCombos
}

// MARK: - Setup
extension HotKeyService {
    func setupDefaultHotKeys() {
        // Snippet hotkey
        setupSnippetHotKeys()

        // Main menu
        change(with: .main, keyCombo: mainKeyCombo)
        // History menu
        change(with: .history, keyCombo: historyKeyCombo)
        // Snippet menu
        change(with: .snippet, keyCombo: snippetKeyCombo)
        // Edit Snippets
        changeEditSnippetsKeyCombo(editSnippetsKeyCombo)
        // Clear History
        changeClearHistoryKeyCombo(clearHistoryKeyCombo)
    }

    func change(with type: MenuType, keyCombo: KeyCombo?) {
        switch type {
        case .main:
            $mainKeyCombo.withLock { $0 = keyCombo }
        case .history:
            $historyKeyCombo.withLock { $0 = keyCombo }
        case .snippet:
            $snippetKeyCombo.withLock { $0 = keyCombo }
        }
        // Reset hotkey
        HotKeyCenter.shared.unregisterHotKey(with: type.rawValue)
        // Register new hotkey
        guard let keyCombo = keyCombo else { return }
        let hotKey = HotKey(identifier: type.rawValue, keyCombo: keyCombo) { [weak self] _ in
            self?.menuManager.popUpMenu(type)
            self?.firebase.logEvent(event: .popUpMenu(type))
        }
        hotKey.register()
    }

    func changeClearHistoryKeyCombo(_ keyCombo: KeyCombo?) {
        $clearHistoryKeyCombo.withLock { $0 = keyCombo }
        // Reset hotkey
        HotKeyCenter.shared.unregisterHotKey(with: "ClearHistory")
        // Register new hotkey
        guard let keyCombo = keyCombo else { return }
        let hotkey = HotKey(identifier: "ClearHistory", keyCombo: keyCombo) { [weak self] _ in
            self?.clipService.clearAllHistory()
        }
        hotkey.register()
    }

    func changeEditSnippetsKeyCombo(_ keyCombo: KeyCombo?) {
        $editSnippetsKeyCombo.withLock { $0 = keyCombo }
        // Reset hotkey
        HotKeyCenter.shared.unregisterHotKey(with: "EditSnippets")
        // Register new hotkey
        guard let keyCombo = keyCombo else { return }
        let hotkey = HotKey(identifier: "EditSnippets", keyCombo: keyCombo) { _ in
            guard let appDelegate = NSApp.delegate as? AppDelegate else { return }
            appDelegate.showSnippetEditorWindow()
        }
        hotkey.register()
    }
}

// MARK: - Snippet HotKey
extension HotKeyService {
    func snippetKeyCombo(forIdentifier identifier: String) -> KeyCombo? {
        folderKeyCombos[identifier]
    }

    func registerSnippetHotKey(with identifier: String, keyCombo: KeyCombo) {
        // Reset hotkey
        HotKeyCenter.shared.unregisterHotKey(with: identifier)
        // Register new hotkey
        let hotKey = HotKey(identifier: identifier, keyCombo: keyCombo) { [weak self] _ in
            guard let self else { return }
            guard let folderID = UUID(uuidString: identifier) else { return }
            guard let folderDetail = snippetRepository.fetchFolderDetail(id: SnippetFolder.ID(rawValue: folderID)) else {
                // When already deleted folder, remove keycombos
                unregisterSnippetHotKey(with: identifier)
                return
            }
            guard folderDetail.folder.isEnabled else { return }
            menuManager.popUpSnippetFolder(folderDetail)
            firebase.logEvent(event: .popUpSnippetFolderMenu)
        }
        hotKey.register()
        // Save key combos
        $folderKeyCombos.withLock { $0[identifier] = keyCombo }
    }

    func unregisterSnippetHotKey(with identifier: String) {
        // Unregister
        HotKeyCenter.shared.unregisterHotKey(with: identifier)
        // Save key combos
        _ = $folderKeyCombos.withLock { $0.removeValue(forKey: identifier) }
    }

    private func setupSnippetHotKeys() {
        folderKeyCombos.forEach { identifier, keyCombo in
            registerSnippetHotKey(with: identifier, keyCombo: keyCombo)
        }
    }
}

extension DependencyValues {
    var hotKeyService: HotKeyService {
        get { self[HotKeyServiceKey.self] }
        set { self[HotKeyServiceKey.self] = newValue }
    }

    private enum HotKeyServiceKey: DependencyKey {
        static let liveValue = HotKeyService()
    }
}
