//
//  CPYHtoKeyManager.swift
//  Clipy
//
//  Created by 古林俊佑 on 2015/06/21.
//  Copyright (c) 2015年 Shunsuke Furubayashi. All rights reserved.
//

import Cocoa

class CPYHotKeyManager: NSObject {

    // MARK: - Properties
    static let sharedManager = CPYHotKeyManager()
    
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
        self.initManager()
    }
    
    private func initManager() {
        let defaults = NSUserDefaults.standardUserDefaults()
        defaults.addObserver(self, forKeyPath: kCPYPrefHotKeysKey, options: NSKeyValueObservingOptions.New, context: nil)
    }
    
    deinit {
        let defaults = NSUserDefaults.standardUserDefaults()
        defaults.removeObserver(self, forKeyPath: kCPYPrefHotKeysKey)
    }
    
    // MARK: - KVO
    override func observeValueForKeyPath(keyPath: String?, ofObject object: AnyObject?, change: [String : AnyObject]?, context: UnsafeMutablePointer<Void>) {
        if keyPath == kCPYPrefHotKeysKey {
            self.registerHotKeys()
        }
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
        
        let hotKeyMap = self.sharedManager.hotkeyMap
        for (key, value) in hotKeyMap {
            if let dict = value as? [String: AnyObject] {
                let indexNubmer = dict[kIndex] as! NSNumber
                let index = indexNubmer.integerValue
                let newKeyCombo = newCombos[index]
                hotKeyCombos.updateValue(newKeyCombo.plistRepresentation(), forKey: key)
            }
        }

        return hotKeyCombos
    }
    
    // MARK: - Public Methods
    func registerHotKeys() {
        let hotKeyCenter = PTHotKeyCenter.sharedCenter()
        
        let hotKeyCombs = NSUserDefaults.standardUserDefaults().objectForKey(kCPYPrefHotKeysKey) as! [String: AnyObject]
        
        let defaultHotKeyCombos = CPYHotKeyManager.defaultHotKeyCombos()
        for (key, _) in defaultHotKeyCombos {
            var keyComboPlist: AnyObject? = hotKeyCombs[key]
            if keyComboPlist == nil {
                keyComboPlist = defaultHotKeyCombos[key]
            }
            let keyCombo = PTKeyCombo(plistRepresentation: keyComboPlist!)
            
            let hotKey = PTHotKey(identifier: key, keyCombo: keyCombo)
            
            let hotKeyDict = self.hotkeyMap[key] as! [String: AnyObject]
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
    
    // MARK: - HotKey Action Methods
    func popUpClipMenu(sender: AnyObject) {
        CPYMenuManager.sharedManager.popUpMenuForType(.Main)
    }
    
    func popUpHistoryMenu(sender: AnyObject) {
        CPYMenuManager.sharedManager.popUpMenuForType(.History)
    }
    
    func popUpSnippetsMenu(sender: AnyObject) {
        CPYMenuManager.sharedManager.popUpMenuForType(.Snippets)
    }
    
}
