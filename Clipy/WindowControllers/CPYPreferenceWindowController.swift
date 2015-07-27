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
    // Hot Keys
    @IBOutlet weak var mainShortcutRecorder: SRRecorderControl!
    @IBOutlet weak var historyShortcutRecorder: SRRecorderControl!
    @IBOutlet weak var snippetsShortcutRecorder: SRRecorderControl!
    private var shortcutRecorders = [SRRecorderControl]()
    internal var storeTypes: NSMutableDictionary!
    
    // MARK: - Init
    override init(window: NSWindow?) {
        super.init(window: window)
        let defaults = NSUserDefaults.standardUserDefaults()
        self.storeTypes = (defaults.objectForKey(kCPYPrefStoreTypesKey) as! NSMutableDictionary).mutableCopy() as! NSMutableDictionary
    }
    
    required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }

    override func windowDidLoad() {
        super.windowDidLoad()
        if let window = self.window {
            window.delegate = self
            window.center()
            window.releasedWhenClosed = false
        }
        self.prepareHotKeys()
    }
    
    // MARK: - Override Methods
    override func showWindow(sender: AnyObject?) {
        super.showWindow(sender)
        self.window?.makeKeyAndOrderFront(self)
    }

    override func setupToolbar() {
        if let image = NSImage(named: NSImageNamePreferencesGeneral) {
            self.addView(self.generalPreferenceView, label: NSLocalizedString("General", comment: ""), image: image)
        }
        if let image = NSImage(named: "Menu") {
            self.addView(self.menuPreferenceView, label: NSLocalizedString("Menu", comment: ""), image: image)
        }
        if let image = NSImage(named: "icon_application") {
            self.addView(self.typePreferenceView, label: NSLocalizedString("Type", comment: ""), image: image)
        }
        if let image = NSImage(named: "PTKeyboardIcon") {
            self.addView(self.shortcutPreferenceView, label: NSLocalizedString("Shortcuts", comment: ""), image: image)
        }
        if let image = NSImage(named: "SparkleIcon") {
            self.addView(self.updatePreferenceView, label: NSLocalizedString("Updates", comment: ""), image: image)
        }
        
        self.crossFade = true
        self.shiftSlowsAnimation = false
    }
  
    // MARK: - Private Methods
    private func prepareHotKeys() {
        self.shortcutRecorders = [self.mainShortcutRecorder, self.historyShortcutRecorder, self.snippetsShortcutRecorder]
        
        let hotKeyMap = CPYHotKeyManager.sharedManager.hotkeyMap
        let hotKeyCombos = NSUserDefaults.standardUserDefaults().objectForKey(kCPYPrefHotKeysKey) as! [String: AnyObject]
        for identifier in hotKeyCombos.keys {
            
            let keyComboPlist = hotKeyCombos[identifier] as! [String: AnyObject]
            let keyCode = Int(keyComboPlist["keyCode"]! as! NSNumber)
            let modifiers = UInt(keyComboPlist["modifiers"]! as! NSNumber)
            
            if let keys = hotKeyMap[identifier] as? [String: AnyObject] {
                let index = keys[kIndex] as! Int
                let recorder = self.shortcutRecorders[index]
                let keyCombo = KeyCombo(flags: recorder.carbonToCocoaFlags(modifiers), code: keyCode)
                recorder.keyCombo = keyCombo
                recorder.animates = true
            }
        }
    }
    
    private func changeHotKeyByShortcutRecorder(aRecorder: SRRecorderControl!, keyCombo: KeyCombo) {
        let newKeyCombo = PTKeyCombo(keyCode: keyCombo.code, modifiers: aRecorder.cocoaToCarbonFlags(keyCombo.flags))
        
        var identifier = ""
        if aRecorder == self.mainShortcutRecorder {
            identifier = kClipMenuIdentifier
        } else if aRecorder == self.historyShortcutRecorder {
            identifier = kHistoryMenuIdentifier
        } else if aRecorder == self.snippetsShortcutRecorder {
            identifier = kSnippetsMenuIdentifier
        }
        
        let hotKeyCenter = PTHotKeyCenter.sharedCenter()
        let oldHotKey = hotKeyCenter.hotKeyWithIdentifier(identifier)
        hotKeyCenter.unregisterHotKey(oldHotKey)
        
        let defaults = NSUserDefaults.standardUserDefaults()
        var hotKeyPrefs = defaults.objectForKey(kCPYPrefHotKeysKey) as! [String: AnyObject]
        hotKeyPrefs.updateValue(newKeyCombo.plistRepresentation(), forKey: identifier)
        defaults.setObject(hotKeyPrefs, forKey: kCPYPrefHotKeysKey)
        defaults.synchronize()
    }
    
    // MARK: - SRRecoederControl Delegate
    func shortcutRecorder(aRecorder: SRRecorderControl!, keyComboDidChange newKeyCombo: KeyCombo) {
        if contains(self.shortcutRecorders, aRecorder) {
            self.changeHotKeyByShortcutRecorder(aRecorder, keyCombo: newKeyCombo)
        }
    }

    
    func windowWillClose(notification: NSNotification) {
        let defaults = NSUserDefaults.standardUserDefaults()
        defaults.setObject(self.storeTypes, forKey: kCPYPrefStoreTypesKey)
        
        if let window = self.window {
            if !window.makeFirstResponder(window) {
                window.endEditingFor(nil)
            }
        }
        NSApp.deactivate()
    }
    
}
