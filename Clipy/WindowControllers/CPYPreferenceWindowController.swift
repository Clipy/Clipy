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
    // Exclude List
    @IBOutlet var excludeListPanel: NSPanel!
    @IBOutlet weak var excludeTableView: NSTableView! {
        didSet {
            self.excludeTableView.setDelegate(self)
            self.excludeTableView.setDataSource(self)
        }
    }
    internal var excludeList = [AnyObject]()
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
        self.excludeList = NSUserDefaults.standardUserDefaults().objectForKey(kCPYPrefExcludeAppsKey) as! [AnyObject]
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
        
        let image = NSWorkspace.sharedWorkspace().iconForFileType(NSFileTypeForHFSTypeCode(OSType(kGenericApplicationIcon)))
        image.size = NSMakeSize(32, 32)
        self.addView(self.typePreferenceView, label: NSLocalizedString("Type", comment: ""), image: image)
        
        if let image = NSImage(named: "PTKeyboardIcon") {
            self.addView(self.shortcutPreferenceView, label: NSLocalizedString("Shortcuts", comment: ""), image: image)
        }
        if let image = NSImage(named: "SparkleIcon") {
            self.addView(self.updatePreferenceView, label: NSLocalizedString("Updates", comment: ""), image: image)
        }
        
        self.crossFade = true
        self.shiftSlowsAnimation = false
    }
 
    // MARK: - Exclude List
    @IBAction func openExcludeOptions(sender: AnyObject) {
        NSApp.beginSheet(self.excludeListPanel, modalForWindow: self.window!, modalDelegate: self, didEndSelector: Selector("excludeListPanelDidEnd:returnCode:"), contextInfo: nil)
    }
    
    internal func excludeListPanelDidEnd(sheet: NSPanel , returnCode: NSInteger) {
        sheet.orderOut(self)
    }
    
    @IBAction func addToExcludeList(sender: AnyObject) {
        let openPanel = NSOpenPanel()
        openPanel.allowedFileTypes = ["app"]
        openPanel.allowsMultipleSelection = true
        openPanel.resolvesAliases = true
        openPanel.prompt = "Add"
        
        openPanel.beginSheetModalForWindow(self.excludeListPanel, completionHandler: { (result) -> Void in
            if result == NSOKButton {
                self.addToExcludeListPanelDidEnd(openPanel)
            }
            openPanel.orderOut(self)
        })
    }
    
    internal func addToExcludeListPanelDidEnd(sheet: NSOpenPanel) {
        
        var appInfo = [String: String]()
        
        for url in sheet.URLs {
            if let bundle = NSBundle(URL: url as! NSURL), infoDict = bundle.infoDictionary {
                
                if let bundleIdentifier = infoDict[kCFBundleIdentifierKey as NSString] as? String {
                    var appName: AnyObject? = infoDict[kCFBundleNameKey as NSString]
                    if appName == nil {
                        appName = infoDict[kCFBundleExecutableKey as NSString]
                        if appName == nil {
                            continue
                        }
                    }
                    appInfo = [kCPYBundleIdentifierKey: bundleIdentifier, kCPYNameKey: appName as! String]
                    self.addAppInfoToExcludeList(appInfo)
                }
            }
        }
    }
    
    @IBAction func removeToExcludeList(sender: AnyObject) {
        let selectedRowIndexes = self.excludeTableView.selectedRowIndexes
        if selectedRowIndexes.count == 0 {
            return
        }
        self.excludeList.removeAtIndex(selectedRowIndexes.firstIndex)
        self.excludeTableView.reloadData()
    }
    
    @IBAction func doneExcludeListPanel(sender: AnyObject) {
        NSApp.endSheet(self.excludeListPanel, returnCode: NSOKButton)
        NSUserDefaults.standardUserDefaults().setObject(self.excludeList, forKey: kCPYPrefExcludeAppsKey)
        NSUserDefaults.standardUserDefaults().synchronize()
    }
    
    @IBAction func cancelExcludeListPanel(sender: AnyObject) {
        NSApp.endSheet(self.excludeListPanel, returnCode: NSCancelButton)
        self.excludeList = NSUserDefaults.standardUserDefaults().objectForKey(kCPYPrefExcludeAppsKey) as! [AnyObject]
        self.excludeTableView.reloadData()
    }
    
    private func addAppInfoToExcludeList(appInfo: [String: String]) {
        let excludeApps = (self.excludeList as! [[String: String]])
        var isAlreadyExclude = false
        for alreadyAppInfo in excludeApps {
            if alreadyAppInfo[kCPYBundleIdentifierKey] == appInfo[kCPYBundleIdentifierKey] {
                isAlreadyExclude = true
            }
        }
        if !isAlreadyExclude {
            self.excludeList.append(appInfo)
            self.excludeTableView.reloadData()
        }
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

// MARK: - NSTableView DataSource
extension CPYPreferenceWindowController: NSTableViewDataSource {
    
    func numberOfRowsInTableView(tableView: NSTableView) -> Int {
        return self.excludeList.count
    }
    
    func tableView(tableView: NSTableView, objectValueForTableColumn tableColumn: NSTableColumn?, row: Int) -> AnyObject? {
        if tableColumn?.identifier == "ImageAndTextCellColumn" {
            if let appInfo = self.excludeList[row] as? [String: String] {
                if let appName = appInfo[kCPYNameKey] {
                    return appName
                }
            }
        }
        return ""
    }
}

// MARK: - NSTableView Delegate
extension CPYPreferenceWindowController: NSTableViewDelegate {
    func tableView(tableView: NSTableView, willDisplayCell cell: AnyObject, forTableColumn tableColumn: NSTableColumn?, row: Int) {
        (cell as! CPYImageAndTextCell).cellImageType = .Application
    }
    
    func tableView(tableView: NSTableView, shouldSelectRow row: Int) -> Bool {
        return true
    }
}
