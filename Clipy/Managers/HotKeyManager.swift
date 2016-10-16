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
    let defaults = NSUserDefaults.standardUserDefaults()

    private(set) var mainKeyCombo: KeyCombo?
    private(set) var historyKeyCombo: KeyCombo?
    private(set) var snippetKeyCombo: KeyCombo?
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
        if !defaults.boolForKey(Constants.HotKey.migrateNewKeyCombo) {
            // Migrate New HotKey Settings
            migrateNewKeyCombo()
            defaults.setBool(true, forKey: Constants.HotKey.migrateNewKeyCombo)
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

    func changeKeyCombo(type: MenuType, keyCombo: KeyCombo?) {
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
    private func registerHotKey(type: MenuType, keyCombo: KeyCombo?) {
        // Save
        saveKeyCombo(type, keyCombo: keyCombo)
        // Unregister
        HotKeyCenter.sharedCenter.unregisterHotKey(type.rawValue)
        guard let keyCombo = keyCombo else { return }
        // Register HotKey
        let hotKey = HotKey(identifier: type.rawValue, keyCombo: keyCombo, target: self, action: type.hotKeySelector)
        hotKey.register()
    }

    private func saveKeyCombo(type: MenuType, keyCombo: KeyCombo?) {
        if let keyCombo = keyCombo {
            defaults.setArchiveData(keyCombo, forKey: type.userDefaultsKey)
        } else {
            defaults.removeObjectForKey(type.userDefaultsKey)
        }
        defaults.synchronize()
    }

    private func migrateNewKeyCombo() {
        if let keyCombos = defaults.objectForKey(Constants.UserDefaults.hotKeys) as? [String: AnyObject] {
            // Main HotKey
            if let combo = keyCombos[Constants.Menu.clip] as? [String: AnyObject], keyCode = combo["keyCode"] as? Int, modifiers = combo["modifiers"] as? Int {
                if let keyCombo = KeyCombo(keyCode: keyCode, carbonModifiers: modifiers) {
                    defaults.setArchiveData(keyCombo, forKey: Constants.HotKey.mainKeyCombo)
                }
            }
            // History HotKey
            if let combo = keyCombos[Constants.Menu.history] as? [String: AnyObject], keyCode = combo["keyCode"] as? Int, modifiers = combo["modifiers"] as? Int {
                if let keyCombo = KeyCombo(keyCode: keyCode, carbonModifiers: modifiers) {
                    defaults.setArchiveData(keyCombo, forKey: Constants.HotKey.historyKeyCombo)
                }
            }
            // Snippet HotKey
            if let combo = keyCombos[Constants.Menu.snippet] as? [String: AnyObject], keyCode = combo["keyCode"] as? Int, modifiers = combo["modifiers"] as? Int {
                if let keyCombo = KeyCombo(keyCode: keyCode, carbonModifiers: modifiers) {
                    defaults.setArchiveData(keyCombo, forKey: Constants.HotKey.snippetKeyCombo)
                }
            }
        }
    }

    static func defaultHotKeyCombos() -> [String: AnyObject] {
        // Main menu key combo  (command + shift + v)
        // History menu key combo (command + control + v)
        // Snipeets menu key combo (command+ shift + b)
        let defaultKeyCombos: [String: AnyObject] = [Constants.Menu.clip: ["keyCode": 9, "modifiers": 768],
                                                     Constants.Menu.history: ["keyCode": 9, "modifiers": 4352],
                                                     Constants.Menu.snippet: ["keyCode": 11, "modifiers": 768]]
        return defaultKeyCombos
    }
}

// MARK: - Snippet Folder HetKey
extension HotKeyManager {

    private var folderKeyCombos: [String: KeyCombo]? {
        get {
            guard let data = defaults.objectForKey(Constants.HotKey.folderKeyCombos) as? NSData else { return nil }
            guard let folderKeyCombos = NSKeyedUnarchiver.unarchiveObjectWithData(data) as? [String: KeyCombo] else { return nil }
            return folderKeyCombos
        }
        set {
            if let value = newValue {
                let data = NSKeyedArchiver.archivedDataWithRootObject(value)
                defaults.setObject(data, forKey: Constants.HotKey.folderKeyCombos)
            } else {
                defaults.removeObjectForKey(Constants.HotKey.folderKeyCombos)
            }
            defaults.synchronize()
        }
    }

    func folderKeyCombo(identifier: String) -> KeyCombo? {
        guard let folderKeyCombos = self.folderKeyCombos else { return nil }
        return folderKeyCombos[identifier]
    }

    func addFolderHotKey(identifier: String, keyCombo: KeyCombo) {
        unregisterFolderHotKey(identifier)
        registerFolderHotKey(identifier, keyCombo: keyCombo)

        var folderKeyCombos = self.folderKeyCombos ?? [String: KeyCombo]()
        folderKeyCombos[identifier] = keyCombo
        self.folderKeyCombos = folderKeyCombos
    }

    func removeFolderHotKey(identifier: String) {
        unregisterFolderHotKey(identifier)

        guard var folderKeyCombos = self.folderKeyCombos else { return }
        folderKeyCombos.removeValueForKey(identifier)
        self.folderKeyCombos = folderKeyCombos
    }

    private func setupFolderHotKeys() {
        guard let folderKeyCombos = folderKeyCombos else { return }
        for (identifier, keyCombo) in folderKeyCombos {
            registerFolderHotKey(identifier, keyCombo: keyCombo)
        }
    }

    private func registerFolderHotKey(identifier: String, keyCombo: KeyCombo) {
        let hotKey = HotKey(identifier: identifier, keyCombo: keyCombo, target: self, action: #selector(HotKeyManager.popUpSnippetFolder(_:)))
        hotKey.register()
    }

    private func unregisterFolderHotKey(identifier: String) {
        HotKeyCenter.sharedCenter.unregisterHotKey(identifier)
    }

    func popUpSnippetFolder(object: AnyObject) {
        guard let hotKey = object as? HotKey else { return }
        let realm = try! Realm()
        guard let folder = realm.objectForPrimaryKey(CPYFolder.self, key: hotKey.identifier) else { return }
        if !folder.enable { return }

        MenuManager.sharedManager.popUpSnippetFolder(folder)
    }

}
