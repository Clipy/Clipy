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

    private enum Type: String {
        case Main       = "ClipMenu"
        case History    = "HistoryMenu"
        case Snippet    = "SnippetMenu"

        var selector: Selector {
            switch self {
            case .Main:
                return #selector(HotKeyManager.popUpClipMenu)
            case .History:
                return #selector(HotKeyManager.popUpHistoryMenu)
            case .Snippet:
                return #selector(HotKeyManager.popUpSnippetMenu)
            }
        }

        var defaultsKey: String {
            switch self {
            case .Main:
                return Constants.HotKey.mainKeyCombo
            case .History:
                return Constants.HotKey.historyKeyCombo
            case .Snippet:
                return Constants.HotKey.snippetKeyCombo
            }
        }
    }

    // MARK: - Initialize
    override init() {
        super.init()
        bind()
    }
}

// MARK: - Action
private extension HotKeyManager {
    @objc private func popUpClipMenu() {
        MenuManager.sharedManager.popUpMenu(.Main)
    }

    @objc private func popUpHistoryMenu() {
        MenuManager.sharedManager.popUpMenu(.History)
    }

    @objc private func popUpSnippetMenu() {
        MenuManager.sharedManager.popUpMenu(.Snippet)
    }
}

// MARK: - KeyCombo
extension HotKeyManager {
    private func changeHotKey(type: Type, keyCombo: KeyCombo?) {
        // Unregister HotKey
        HotKeyCenter.sharedCenter.unregisterHotKey(type.rawValue)
        if let keyCombo = keyCombo {
            // Register HotKey
            let hotKey = HotKey(identifier: type.rawValue, keyCombo: keyCombo, target: self, action: type.selector)
            hotKey.register()
            // Save KeyCombo
            let data = NSKeyedArchiver.archivedDataWithRootObject(keyCombo)
            defaults.setObject(data, forKey: type.defaultsKey)
        } else {
            // Remove KeyCombo
            defaults.removeObjectForKey(type.defaultsKey)
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
        if let data = defaults.objectForKey(Constants.HotKey.mainKeyCombo) as? NSData, let keyCombo = NSKeyedUnarchiver.unarchiveObjectWithData(data) as? KeyCombo {
            mainKeyCombo.value = keyCombo
        }
        // History HotKey
        if let data = defaults.objectForKey(Constants.HotKey.historyKeyCombo) as? NSData, let keyCombo = NSKeyedUnarchiver.unarchiveObjectWithData(data) as? KeyCombo {
            historyKeyCombo.value = keyCombo
        }
        // Snippet HotKey
        if let data = defaults.objectForKey(Constants.HotKey.snippetKeyCombo) as? NSData, let keyCombo = NSKeyedUnarchiver.unarchiveObjectWithData(data) as? KeyCombo {
            snippetKeyCombo.value = keyCombo
        }
    }

    private func migrateNewKeyCombo() {
        if let keyCombos = defaults.objectForKey(Constants.UserDefaults.hotKeys) as? [String: AnyObject] {
            // Main HotKey
            if let combo = keyCombos[Constants.Menu.clip] as? [String: AnyObject], keyCode = combo["keyCode"] as? Int, modifiers = combo["modifiers"] as? Int {
                if let keyCombo = KeyCombo(keyCode: keyCode, carbonModifiers: modifiers) {
                    let archiveData = NSKeyedArchiver.archivedDataWithRootObject(keyCombo)
                    defaults.setObject(archiveData, forKey: Constants.HotKey.mainKeyCombo)
                }
            }
            // History HotKey
            if let combo = keyCombos[Constants.Menu.history] as? [String: AnyObject], keyCode = combo["keyCode"] as? Int, modifiers = combo["modifiers"] as? Int {
                if let keyCombo = KeyCombo(keyCode: keyCode, carbonModifiers: modifiers) {
                    let archiveData = NSKeyedArchiver.archivedDataWithRootObject(keyCombo)
                    defaults.setObject(archiveData, forKey: Constants.HotKey.historyKeyCombo)
                }
            }
            // Snippet HotKey
            if let combo = keyCombos[Constants.Menu.snippet] as? [String: AnyObject], keyCode = combo["keyCode"] as? Int, modifiers = combo["modifiers"] as? Int {
                if let keyCombo = KeyCombo(keyCode: keyCode, carbonModifiers: modifiers) {
                    let archiveData = NSKeyedArchiver.archivedDataWithRootObject(keyCombo)
                    defaults.setObject(archiveData, forKey: Constants.HotKey.snippetKeyCombo)
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
