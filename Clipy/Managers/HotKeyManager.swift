//
//  HotKeyManager.swift
//  Clipy
//
//  Created by 古林俊佑 on 2016/06/20.
//  Copyright © 2016年 Shunsuke Furubayashi. All rights reserved.
//

import Foundation
import RxSwift
import Magnet
import RealmSwift

final class HotKeyManager: NSObject {
    // MARK: - Properties
    static let sharedManager = HotKeyManager()
    let defaults = UserDefaults.standard

    fileprivate(set) var mainKeyCombo: KeyCombo?
    fileprivate(set) var historyKeyCombo: KeyCombo?
    fileprivate(set) var snippetKeyCombo: KeyCombo?
}

// MARK: - Action
extension HotKeyManager {
    func popUpClipMenu() {
        MenuManager.sharedManager.popUpMenu(.Main)
    }

    func popUpHistoryMenu() {
        MenuManager.sharedManager.popUpMenu(.History)
    }

    func popUpSnippetMenu() {
        MenuManager.sharedManager.popUpMenu(.Snippet)
    }
}

// MARK: - KeyCombo Setting
extension HotKeyManager {
    func setupDefaultHoyKey() {
        if !defaults.bool(forKey: Constants.HotKey.migrateNewKeyCombo) {
            // Migrate New HotKey Settings
            migrateNewKeyCombo()
            defaults.set(true, forKey: Constants.HotKey.migrateNewKeyCombo)
            defaults.synchronize()
        }
        setupFolderHotKeys()

        // Main HotKey
        if let keyCombo = defaults.archiveDataForKey(KeyCombo.self, key: Constants.HotKey.mainKeyCombo) {
            changeKeyCombo(.Main, keyCombo: keyCombo)
        }
        // History HotKey
        if let keyCombo = defaults.archiveDataForKey(KeyCombo.self, key: Constants.HotKey.historyKeyCombo) {
            changeKeyCombo(.History, keyCombo: keyCombo)
        }
        // Snippet HotKey
        if let keyCombo = defaults.archiveDataForKey(KeyCombo.self, key: Constants.HotKey.snippetKeyCombo) {
            changeKeyCombo(.Snippet, keyCombo: keyCombo)
        }
    }

    func changeKeyCombo(_ type: MenuType, keyCombo: KeyCombo?) {
        switch type {
        case .Main:
            mainKeyCombo = keyCombo
        case .History:
            historyKeyCombo = keyCombo
        case .Snippet:
            snippetKeyCombo = keyCombo
        }
        registerHotKey(type, keyCombo: keyCombo)
    }
}

// MARK: - Register
extension HotKeyManager {
    fileprivate func registerHotKey(_ type: MenuType, keyCombo: KeyCombo?) {
        // Save
        saveKeyCombo(type, keyCombo: keyCombo)
        // Unregister
        HotKeyCenter.shared.unregisterHotKey(with: type.rawValue)
        guard let keyCombo = keyCombo else { return }
        // Register HotKey
        let hotKey = HotKey(identifier: type.rawValue, keyCombo: keyCombo, target: self, action: type.hotKeySelector)
        hotKey.register()
    }

    fileprivate func saveKeyCombo(_ type: MenuType, keyCombo: KeyCombo?) {
        if let keyCombo = keyCombo {
            defaults.setArchiveData(keyCombo, forKey: type.userDefaultsKey)
        } else {
            defaults.removeObject(forKey: type.userDefaultsKey)
        }
        defaults.synchronize()
    }

    fileprivate func migrateNewKeyCombo() {
        if let keyCombos = defaults.object(forKey: Constants.UserDefaults.hotKeys) as? [String: Any] {
            // Main HotKey
            if let combo = keyCombos[Constants.Menu.clip] as? [String: Any], let keyCode = combo["keyCode"] as? Int, let modifiers = combo["modifiers"] as? Int {
                if let keyCombo = KeyCombo(keyCode: keyCode, carbonModifiers: modifiers) {
                    defaults.setArchiveData(keyCombo, forKey: Constants.HotKey.mainKeyCombo)
                }
            }
            // History HotKey
            if let combo = keyCombos[Constants.Menu.history] as? [String: Any], let keyCode = combo["keyCode"] as? Int, let modifiers = combo["modifiers"] as? Int {
                if let keyCombo = KeyCombo(keyCode: keyCode, carbonModifiers: modifiers) {
                    defaults.setArchiveData(keyCombo, forKey: Constants.HotKey.historyKeyCombo)
                }
            }
            // Snippet HotKey
            if let combo = keyCombos[Constants.Menu.snippet] as? [String: Any], let keyCode = combo["keyCode"] as? Int, let modifiers = combo["modifiers"] as? Int {
                if let keyCombo = KeyCombo(keyCode: keyCode, carbonModifiers: modifiers) {
                    defaults.setArchiveData(keyCombo, forKey: Constants.HotKey.snippetKeyCombo)
                }
            }
        }
    }

    static func defaultHotKeyCombos() -> [String: Any] {
        // Main menu key combo  (command + shift + v)
        // History menu key combo (command + control + v)
        // Snipeets menu key combo (command+ shift + b)
        let defaultKeyCombos: [String: Any] = [Constants.Menu.clip: ["keyCode": 9, "modifiers": 768],
                                                     Constants.Menu.history: ["keyCode": 9, "modifiers": 4352],
                                                     Constants.Menu.snippet: ["keyCode": 11, "modifiers": 768]]
        return defaultKeyCombos
    }
}

// MARK: - Snippet Folder HetKey
extension HotKeyManager {

    fileprivate var folderKeyCombos: [String: KeyCombo]? {
        get {
            guard let data = defaults.object(forKey: Constants.HotKey.folderKeyCombos) as? Data else { return nil }
            guard let folderKeyCombos = NSKeyedUnarchiver.unarchiveObject(with: data) as? [String: KeyCombo] else { return nil }
            return folderKeyCombos
        }
        set {
            if let value = newValue {
                let data = NSKeyedArchiver.archivedData(withRootObject: value)
                defaults.set(data, forKey: Constants.HotKey.folderKeyCombos)
            } else {
                defaults.removeObject(forKey: Constants.HotKey.folderKeyCombos)
            }
            defaults.synchronize()
        }
    }

    func folderKeyCombo(_ identifier: String) -> KeyCombo? {
        guard let folderKeyCombos = self.folderKeyCombos else { return nil }
        return folderKeyCombos[identifier]
    }

    func addFolderHotKey(_ identifier: String, keyCombo: KeyCombo) {
        unregisterFolderHotKey(identifier)
        registerFolderHotKey(identifier, keyCombo: keyCombo)

        var folderKeyCombos = self.folderKeyCombos ?? [String: KeyCombo]()
        folderKeyCombos[identifier] = keyCombo
        self.folderKeyCombos = folderKeyCombos
    }

    func removeFolderHotKey(_ identifier: String) {
        unregisterFolderHotKey(identifier)

        guard var folderKeyCombos = self.folderKeyCombos else { return }
        folderKeyCombos.removeValue(forKey: identifier)
        self.folderKeyCombos = folderKeyCombos
    }

    fileprivate func setupFolderHotKeys() {
        guard let folderKeyCombos = folderKeyCombos else { return }
        for (identifier, keyCombo) in folderKeyCombos {
            registerFolderHotKey(identifier, keyCombo: keyCombo)
        }
    }

    fileprivate func registerFolderHotKey(_ identifier: String, keyCombo: KeyCombo) {
        let hotKey = HotKey(identifier: identifier, keyCombo: keyCombo, target: self, action: #selector(HotKeyManager.popUpSnippetFolder(_:)))
        hotKey.register()
    }

    fileprivate func unregisterFolderHotKey(_ identifier: String) {
        HotKeyCenter.shared.unregisterHotKey(with: identifier)
    }

    func popUpSnippetFolder(_ object: AnyObject) {
        guard let hotKey = object as? HotKey else { return }
        let realm = try! Realm()
        guard let folder = realm.object(ofType: CPYFolder.self, forPrimaryKey: hotKey.identifier) else { return }
        if !folder.enable { return }

        MenuManager.sharedManager.popUpSnippetFolder(folder)
    }

}
