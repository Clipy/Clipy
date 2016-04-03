//
//  CPYHtoKeyManager.swift
//  Clipy
//
//  Created by 古林俊佑 on 2015/06/21.
//  Copyright (c) 2015年 Shunsuke Furubayashi. All rights reserved.
//

import Cocoa
import RxCocoa
import RxSwift
import RxOptional
import NSObject_Rx

class CPYHotKeyManager: NSObject {

    // MARK: - Properties
    static let sharedManager = CPYHotKeyManager()
    private let defaults = NSUserDefaults.standardUserDefaults()

    var hotkeyMap: [String: AnyObject] {

        var map = [String: AnyObject]()
        var dict = [String: AnyObject]()

        dict = [kIndex: NSNumber(unsignedInteger: 0), kSelector: "popUpClipMenu:"]
        map.updateValue(dict, forKey: kClipMenuIdentifier)

        dict = [kIndex: NSNumber(unsignedInteger: 1), kSelector: "popUpHistoryMenu:"]
        map.updateValue(dict, forKey: kHistoryMenuIdentifier)

        dict = [kIndex: NSNumber(unsignedInteger: 2), kSelector: "popUpSnippetsMenu:"]
        map.updateValue(dict, forKey: kSnippetsMenuIdentifier)

        return map
    }

    // MARK: - Init
    override init() {
        super.init()
        bind()
    }

    // MARK: - Class Methods
    static func defaultHotKeyCombos() -> [String: AnyObject] {
        var hotKeyCombos = [String: AnyObject]()
        var newCombos: [PTKeyCombo] = []

        var keyCombo: PTKeyCombo!

        // Main menu key combo  (command + shift + v)
        keyCombo = PTKeyCombo(keyCode: 9, modifiers: 768)
        newCombos.append(keyCombo)

        // History menu key combo (command + control + v)
        keyCombo = PTKeyCombo(keyCode: 9, modifiers: 4352)
        newCombos.append(keyCombo)

        // Snipeets menu key combo (command+ shift + b)
        keyCombo = PTKeyCombo(keyCode: 11, modifiers: 768)
        newCombos.append(keyCombo)

        let hotKeyMap = sharedManager.hotkeyMap
        for (key, value) in hotKeyMap {
            if let dict = value as? [String: AnyObject], let indexNumber = dict[kIndex] as? NSNumber {
                let index = indexNumber.integerValue
                let newKeyCombo = newCombos[index]
                hotKeyCombos.updateValue(newKeyCombo.plistRepresentation(), forKey: key)
            }
        }

        return hotKeyCombos
    }

    // MARK: - Public Methods
    // swiftlint:disable force_cast
    func registerHotKeys() {
        let hotKeyCenter = PTHotKeyCenter.sharedCenter()

        let hotKeyCombs = defaults.objectForKey(kCPYPrefHotKeysKey) as! [String: AnyObject]

        let defaultHotKeyCombos = CPYHotKeyManager.defaultHotKeyCombos()
        for (key, _) in defaultHotKeyCombos {
            var keyComboPlist: AnyObject? = hotKeyCombs[key]
            if keyComboPlist == nil {
                keyComboPlist = defaultHotKeyCombos[key]
            }
            let keyCombo = PTKeyCombo(plistRepresentation: keyComboPlist!)

            let hotKey = PTHotKey(identifier: key, keyCombo: keyCombo)

            let hotKeyDict = hotkeyMap[key] as! [String: AnyObject]
            let selectorName = hotKeyDict[kSelector] as! String
            hotKey.setTarget(self)
            hotKey.setAction(Selector(selectorName))

            hotKeyCenter.registerHotKey(hotKey)
        }
    }

    func unRegisterHotKeys() {
        let hotKeyCenter = PTHotKeyCenter.sharedCenter()
        for hotKey in hotKeyCenter.allHotKeys() as! [PTHotKey] {
            hotKeyCenter.unregisterHotKey(hotKey)
        }
    }
    // swiftlint:enable force_cast

    // MARK: - HotKey Action Methods
    func popUpClipMenu(sender: AnyObject) {
        MenuManager.sharedManager.popUpMenu(.Main)
    }

    func popUpHistoryMenu(sender: AnyObject) {
        MenuManager.sharedManager.popUpMenu(.History)
    }

    func popUpSnippetsMenu(sender: AnyObject) {
        MenuManager.sharedManager.popUpMenu(.Snippet)
    }

}

// MARK: - Binding
private extension CPYHotKeyManager {
    private func bind() {
        defaults.rx_observe([String: AnyObject].self, kCPYPrefHotKeysKey, options: [.New])
            .filterNil()
            .subscribeNext { [weak self] keys in
                 self?.registerHotKeys()
            }.addDisposableTo(rx_disposeBag)
    }
}
