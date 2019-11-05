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

import Foundation
import Cocoa
import Magnet
import RealmSwift

final class HotKeyService: NSObject {

    // MARK: - Properties
    static var defaultKeyCombos: [String: Any] = {
        // MainMenu:    ⌘ + Shift + V
        // HistoryMenu: ⌘ + Control + V
        // SnipeetMenu: ⌘ + Shift B
        return [Constants.Menu.clip: ["keyCode": 9, "modifiers": 768],
                Constants.Menu.history: ["keyCode": 9, "modifiers": 4352],
                Constants.Menu.snippet: ["keyCode": 11, "modifiers": 768]]
    }()

    fileprivate(set) var mainKeyCombo: KeyCombo?
    fileprivate(set) var historyKeyCombo: KeyCombo?
    fileprivate(set) var snippetKeyCombo: KeyCombo?
    fileprivate(set) var clearHistoryKeyCombo: KeyCombo?

}

// MARK: - Actions
extension HotKeyService {
    @objc func popupMainMenu() {
        AppEnvironment.current.menuService.popUpMenu(.main)
    }

    @objc func popupHistoryMenu() {
        AppEnvironment.current.menuService.popUpMenu(.history)
    }

    @objc func popUpSnippetMenu() {
        AppEnvironment.current.menuService.popUpMenu(.snippet)
    }

    @objc func popUpClearHistoryAlert() {
        guard let appDelegate = NSApp.delegate as? AppDelegate else { return }
        appDelegate.clearAllHistory()
    }
}

// MARK: - Setup
extension HotKeyService {
    func setupDefaultHotKeys() {
        // Migration new framework
        if !AppEnvironment.current.defaults.bool(forKey: Constants.HotKey.migrateNewKeyCombo) {
            migrationKeyCombos()
            AppEnvironment.current.defaults.set(true, forKey: Constants.HotKey.migrateNewKeyCombo)
            AppEnvironment.current.defaults.synchronize()
        }
        // Snippet hotkey
        setupSnippetHotKeys()

        // Main menu
        change(with: .main, keyCombo: savedKeyCombo(forKey: Constants.HotKey.mainKeyCombo))
        // History menu
        change(with: .history, keyCombo: savedKeyCombo(forKey: Constants.HotKey.historyKeyCombo))
        // Snippet menu
        change(with: .snippet, keyCombo: savedKeyCombo(forKey: Constants.HotKey.snippetKeyCombo))
        // Clear History
        changeClearHistoryKeyCombo(savedKeyCombo(forKey: Constants.HotKey.clearHistoryKeyCombo))
    }

    func change(with type: MenuType, keyCombo: KeyCombo?) {
        switch type {
        case .main:
            mainKeyCombo = keyCombo
        case .history:
            historyKeyCombo = keyCombo
        case .snippet:
            snippetKeyCombo = keyCombo
        }
        register(with: type, keyCombo: keyCombo)
    }

    func changeClearHistoryKeyCombo(_ keyCombo: KeyCombo?) {
        clearHistoryKeyCombo = keyCombo
        AppEnvironment.current.defaults.set(keyCombo?.archive(), forKey: Constants.HotKey.clearHistoryKeyCombo)
        AppEnvironment.current.defaults.synchronize()
        // Reset hotkey
        HotKeyCenter.shared.unregisterHotKey(with: "ClearHistory")
        // Register new hotkey
        guard let keyCombo = keyCombo else { return }
        let hotkey = HotKey(identifier: "ClearHistory", keyCombo: keyCombo, target: self, action: #selector(HotKeyService.popUpClearHistoryAlert))
        hotkey.register()
    }

    private func savedKeyCombo(forKey key: String) -> KeyCombo? {
        guard let data = AppEnvironment.current.defaults.object(forKey: key) as? Data else { return nil }
        guard let keyCombo = NSKeyedUnarchiver.unarchiveObject(with: data) as? KeyCombo else { return nil }
        return keyCombo
    }
}

// MARK: - Register
private extension HotKeyService {
    func register(with type: MenuType, keyCombo: KeyCombo?) {
        save(with: type, keyCombo: keyCombo)
        // Reset hotkey
        HotKeyCenter.shared.unregisterHotKey(with: type.rawValue)
        // Register new hotkey
        guard let keyCombo = keyCombo else { return }
        let hotKey = HotKey(identifier: type.rawValue, keyCombo: keyCombo, target: self, action: type.hotKeySelector)
        hotKey.register()
    }

    func save(with type: MenuType, keyCombo: KeyCombo?) {
        AppEnvironment.current.defaults.set(keyCombo?.archive(), forKey: type.userDefaultsKey)
        AppEnvironment.current.defaults.synchronize()
    }
}

// MARK: - Migration
fileprivate extension HotKeyService {
    /**
     *  Migration for changing the storage with v1.1.0
     *  Changed framework, PTHotKey to Magnet
     */
    func migrationKeyCombos() {
        guard let keyCombos = AppEnvironment.current.defaults.object(forKey: Constants.UserDefaults.hotKeys) as? [String: Any] else { return }

        // Main menu
        if let (keyCode, modifiers) = parse(with: keyCombos, forKey: Constants.Menu.clip) {
            if let keyCombo = KeyCombo(keyCode: keyCode, carbonModifiers: modifiers) {
                AppEnvironment.current.defaults.set(keyCombo.archive(), forKey: Constants.HotKey.mainKeyCombo)
            }
        }
        // History menu
        if let (keyCode, modifiers) = parse(with: keyCombos, forKey: Constants.Menu.history) {
            if let keyCombo = KeyCombo(keyCode: keyCode, carbonModifiers: modifiers) {
                AppEnvironment.current.defaults.set(keyCombo.archive(), forKey: Constants.HotKey.historyKeyCombo)
            }
        }
        // Snippet menu
        if let (keyCode, modifiers) = parse(with: keyCombos, forKey: Constants.Menu.snippet) {
            if let keyCombo = KeyCombo(keyCode: keyCode, carbonModifiers: modifiers) {
                AppEnvironment.current.defaults.set(keyCombo.archive(), forKey: Constants.HotKey.snippetKeyCombo)
            }
        }
    }

    private func parse(with keyCombos: [String: Any], forKey key: String) -> (Int, Int)? {
        guard let combos = keyCombos[key] as? [String: Any] else { return nil }
        guard let keyCode = combos["keyCode"] as? Int, let modifiers = combos["modifiers"] as? Int else { return nil }
        return (keyCode, modifiers)
    }
}

// MARK: - Snippet HotKey
extension HotKeyService {
    private var folderKeyCombos: [String: KeyCombo]? {
        get {
            guard let data = AppEnvironment.current.defaults.object(forKey: Constants.HotKey.folderKeyCombos) as? Data else { return nil }
            return NSKeyedUnarchiver.unarchiveObject(with: data) as? [String: KeyCombo]
        }
        set {
            if let value = newValue {
                AppEnvironment.current.defaults.set(NSKeyedArchiver.archivedData(withRootObject: value), forKey: Constants.HotKey.folderKeyCombos)
            } else {
                AppEnvironment.current.defaults.removeObject(forKey: Constants.HotKey.folderKeyCombos)
            }
            AppEnvironment.current.defaults.synchronize()
        }
    }

    func snippetKeyCombo(forIdentifier identifier: String) -> KeyCombo? {
        return folderKeyCombos?[identifier]
    }

    func registerSnippetHotKey(with identifier: String, keyCombo: KeyCombo) {
        // Reset hotkey
        unregisterSnippetHotKey(with: identifier)
        // Register new hotkey
        let hotKey = HotKey(identifier: identifier, keyCombo: keyCombo, target: self, action: #selector(HotKeyService.popupSnippetFolder(_:)))
        hotKey.register()
        // Save key combos
        var keyCombos = folderKeyCombos ?? [String: KeyCombo]()
        keyCombos[identifier] = keyCombo
        folderKeyCombos = keyCombos
    }

    func unregisterSnippetHotKey(with identifier: String) {
        // Unregister
        HotKeyCenter.shared.unregisterHotKey(with: identifier)
        // Save key combos
        var keyCombos = folderKeyCombos ?? [String: KeyCombo]()
        keyCombos.removeValue(forKey: identifier)
        folderKeyCombos = keyCombos
    }

    @objc func popupSnippetFolder(_ object: AnyObject) {
        guard let hotKey = object as? HotKey else { return }
        let realm = try! Realm()
        guard let folder = realm.object(ofType: CPYFolder.self, forPrimaryKey: hotKey.identifier) else {
            // When already deleted folder, remove keycombos
            unregisterSnippetHotKey(with: hotKey.identifier)
            return
        }
        if !folder.enable { return }

        AppEnvironment.current.menuService.popUpSnippetFolder(folder)
    }

    fileprivate func setupSnippetHotKeys() {
        folderKeyCombos?.forEach {
            let hotKey = HotKey(identifier: $0, keyCombo: $1, target: self, action: #selector(HotKeyService.popupSnippetFolder(_:)))
            hotKey.register()
        }
    }
}
