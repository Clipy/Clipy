//
//  CPYPreferenceWindowController.swift
//  Clipy
//
//  Created by 古林俊佑 on 2015/06/28.
//  Copyright (c) 2015年 Shunsuke Furubayashi. All rights reserved.
//

import Cocoa

class CPYPreferenceWindowController: DBPrefsWindowController, NSWindowDelegate {

    // MARK: - Propertis
    // Views
    @IBOutlet var generalPreferenceView: NSView!
    @IBOutlet var menuPreferenceView: NSView!
    @IBOutlet var typePreferenceView: NSView!
    @IBOutlet var shortcutPreferenceView: NSView!
    @IBOutlet var updatePreferenceView: NSView!
    @IBOutlet weak var versionTextField: NSTextField!
    // Hot Keys
    @IBOutlet weak var mainShortcutRecorder: SRRecorderControl!
    @IBOutlet weak var historyShortcutRecorder: SRRecorderControl!
    @IBOutlet weak var snippetsShortcutRecorder: SRRecorderControl!
    private var shortcutRecorders = [SRRecorderControl]()
    var storeTypes: NSMutableDictionary!
    private let defaults = NSUserDefaults.standardUserDefaults()
    
    // MARK: - Init
    override init(window: NSWindow?) {
        super.init(window: window)
        storeTypes = (defaults.objectForKey(kCPYPrefStoreTypesKey) as! NSMutableDictionary).mutableCopy() as! NSMutableDictionary
    }
    
    required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }

    override func windowDidLoad() {
        super.windowDidLoad()
        if let window = window {
            window.delegate = self
            window.center()
            window.releasedWhenClosed = false
        }
        prepareHotKeys()
        if let versionString = NSBundle.mainBundle().objectForInfoDictionaryKey("CFBundleShortVersionString") as? String {
            versionTextField.stringValue = "v\(versionString)"
        }
    }
    
    // MARK: - Override Methods
    override func showWindow(sender: AnyObject?) {
        super.showWindow(sender)
        window?.makeKeyAndOrderFront(self)
    }

    override func setupToolbar() {
        if let image = NSImage(named: NSImageNamePreferencesGeneral) {
            addView(generalPreferenceView, label: LocalizedString.General.value, image: image)
        }
        if let image = NSImage(assetIdentifier: .Menu) {
            addView(menuPreferenceView, label: LocalizedString.Menu.value, image: image)
        }
        if let image = NSImage(assetIdentifier: .IconApplication) {
            addView(typePreferenceView, label: LocalizedString.Type.value, image: image)
        }
        if let image = NSImage(assetIdentifier: .IconKeyboard) {
            addView(shortcutPreferenceView, label: LocalizedString.Shortcuts.value, image: image)
        }
        if let image = NSImage(assetIdentifier: .IconSparkle) {
            addView(updatePreferenceView, label: LocalizedString.Updates.value, image: image)
        }
        
        crossFade = true
        shiftSlowsAnimation = false
    }
  
    // MARK: - Private Methods
    private func prepareHotKeys() {
        shortcutRecorders = [mainShortcutRecorder, historyShortcutRecorder, snippetsShortcutRecorder]
        
        let hotKeyMap = CPYHotKeyManager.sharedManager.hotkeyMap
        let hotKeyCombos = NSUserDefaults.standardUserDefaults().objectForKey(kCPYPrefHotKeysKey) as! [String: AnyObject]
        for identifier in hotKeyCombos.keys {
            
            let keyComboPlist = hotKeyCombos[identifier] as! [String: AnyObject]
            let keyCode = Int(keyComboPlist["keyCode"]! as! NSNumber)
            let modifiers = UInt(keyComboPlist["modifiers"]! as! NSNumber)
            
            if let keys = hotKeyMap[identifier] as? [String: AnyObject] {
                let index = keys[kIndex] as! Int
                let recorder = shortcutRecorders[index]
                let keyCombo = KeyCombo(flags: recorder.carbonToCocoaFlags(modifiers), code: keyCode)
                recorder.keyCombo = keyCombo
                recorder.animates = true
            }
        }
    }
    
    private func changeHotKeyByShortcutRecorder(aRecorder: SRRecorderControl!, keyCombo: KeyCombo) {
        let newKeyCombo = PTKeyCombo(keyCode: keyCombo.code, modifiers: aRecorder.cocoaToCarbonFlags(keyCombo.flags))
        
        var identifier = ""
        if aRecorder == mainShortcutRecorder {
            identifier = kClipMenuIdentifier
        } else if aRecorder == historyShortcutRecorder {
            identifier = kHistoryMenuIdentifier
        } else if aRecorder == snippetsShortcutRecorder {
            identifier = kSnippetsMenuIdentifier
        }
        
        let hotKeyCenter = PTHotKeyCenter.sharedCenter()
        let oldHotKey = hotKeyCenter.hotKeyWithIdentifier(identifier)
        hotKeyCenter.unregisterHotKey(oldHotKey)

        var hotKeyPrefs = defaults.objectForKey(kCPYPrefHotKeysKey) as! [String: AnyObject]
        hotKeyPrefs.updateValue(newKeyCombo.plistRepresentation(), forKey: identifier)
        defaults.setObject(hotKeyPrefs, forKey: kCPYPrefHotKeysKey)
        defaults.synchronize()
    }
    
    // MARK: - SRRecoederControl Delegate
    func shortcutRecorder(aRecorder: SRRecorderControl!, keyComboDidChange newKeyCombo: KeyCombo) {
        if shortcutRecorders.contains(aRecorder) {
            changeHotKeyByShortcutRecorder(aRecorder, keyCombo: newKeyCombo)
        }
    }

    
    func windowWillClose(notification: NSNotification) {
        defaults.setObject(storeTypes, forKey: kCPYPrefStoreTypesKey)
        
        if let window = window {
            if !window.makeFirstResponder(window) {
                window.endEditingFor(nil)
            }
        }
        NSApp.deactivate()
    }
    
}
