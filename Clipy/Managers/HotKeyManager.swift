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

final class HotKeyManager: NSObject {
    // MARK: - Properties
    static let sharedManager = HotKeyManager()
    let defaults = NSUserDefaults.standardUserDefaults()

    let mainKeyCombo = Variable<KeyCombo?>(nil)
    let historyKeyCombo = Variable<KeyCombo?>(nil)
    let snippetKeyCombo = Variable<KeyCombo?>(nil)

    // MARK: - Initialize
    override init() {
        super.init()
        bind()
    }
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

// MARK: - KeyCombo
extension HotKeyManager {
    private func changeHotKey(type: MenuType, keyCombo: KeyCombo?) {
        // Unregister HotKey
        HotKeyCenter.sharedCenter.unregisterHotKey(type.rawValue)
        if let keyCombo = keyCombo {
            // Register HotKey
            let hotKey = HotKey(identifier: type.rawValue, keyCombo: keyCombo, target: self, action: type.hotKeySelector)
            hotKey.register()
            // Save KeyCombo
            defaults.setArchiveData(keyCombo, forKey: type.userDefaultsKey)
        } else {
            // Remove KeyCombo
            defaults.removeObjectForKey(type.userDefaultsKey)
        }
        defaults.synchronize()
    }

    func setupDefaultHoyKey() {
        if !defaults.boolForKey(Constants.HotKey.migrateNewKeyCombo) {
            // Migrate New HotKey Settings
            migrateNewKeyCombo()
            defaults.setBool(true, forKey: Constants.HotKey.migrateNewKeyCombo)
            defaults.synchronize()
        }

        // Main HotKey
        if let keyCombo = defaults.archiveDataForKey(KeyCombo.self, key: Constants.HotKey.mainKeyCombo) {
            mainKeyCombo.value = keyCombo
        }
        // History HotKey
        if let keyCombo = defaults.archiveDataForKey(KeyCombo.self, key: Constants.HotKey.historyKeyCombo) {
            historyKeyCombo.value = keyCombo
        }
        // Snippet HotKey
        if let keyCombo = defaults.archiveDataForKey(KeyCombo.self, key: Constants.HotKey.snippetKeyCombo) {
            snippetKeyCombo.value = keyCombo
        }
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

// MARK: - Binding
private extension HotKeyManager {
    private func bind() {
        // Main Shortcut
        mainKeyCombo.asObservable()
            .skip(1)
            .subscribeNext { [unowned self] keyCombo in
                self.changeHotKey(.Main, keyCombo: keyCombo)
            }.addDisposableTo(rx_disposeBag)
        // History Shortcut
        historyKeyCombo.asObservable()
            .skip(1)
            .subscribeNext { [unowned self] keyCombo in
                self.changeHotKey(.History, keyCombo: keyCombo)
            }.addDisposableTo(rx_disposeBag)
        // Snippet Shortcut
        snippetKeyCombo.asObservable()
            .skip(1)
            .subscribeNext { [unowned self] keyCombo in
                self.changeHotKey(.Snippet, keyCombo: keyCombo)
            }.addDisposableTo(rx_disposeBag)
    }
}
