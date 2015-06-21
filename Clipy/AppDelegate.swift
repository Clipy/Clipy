//
//  AppDelegate.swift
//  Clipy
//
//  Created by 古林俊佑 on 2015/06/21.
//  Copyright (c) 2015年 Shunsuke Furubayashi. All rights reserved.
//

import Cocoa
import Sparkle

@NSApplicationMain
class AppDelegate: NSObject {

    // MARK: - Properties
    // lazy var snippetEditorController = CPYSnippetEditorWindowController.windowController()
    
    // MARK: - Init
    override func awakeFromNib() {
        super.awakeFromNib()
        self.initController()
    }
    
    deinit {
        NSNotificationCenter.defaultCenter().removeObserver(self)
        let defaults = NSUserDefaults.standardUserDefaults()
        defaults.removeObserver(self, forKeyPath: kCPYEnableAutomaticCheckPreReleaseKey)
    }
    
    private func initController() {
        CPYUtilities.registerUserDefaultKeys()
        
        // Show menubar icon
        // CPYMenuManager.sharedManager.createStatusItem()
        
        // KVO
        let defaults = NSUserDefaults.standardUserDefaults()
        defaults.addObserver(self, forKeyPath: kCPYEnableAutomaticCheckPreReleaseKey, options: NSKeyValueObservingOptions.New, context: nil)
        
        // Notification
        let notificationCenter = NSNotificationCenter.defaultCenter()
        // notificationCenter.addObserver(self, selector: "handlePreferencePanelWillClose:", name: kCPYPreferencePanelWillCloseNotification, object: nil)
    }

    // MARK: - Override Methods
    override func validateMenuItem(menuItem: NSMenuItem) -> Bool {
        let action = menuItem.action
        if action == Selector("clearAllHistory") {
            /*
            if let numberOfClips = CPYClipManager.sharedManager.loadClips()?.count {
                if numberOfClips == 0 {
                    return false
                }
            }*/
        }
        return true
    }
    
    // MARK: - KVO
    override func observeValueForKeyPath(keyPath: String, ofObject object: AnyObject, change: [NSObject : AnyObject], context: UnsafeMutablePointer<Void>) {
        if keyPath == kCPYEnableAutomaticCheckPreReleaseKey {
            let checkPreRelease = object[kCPYCheckNewRelease] as! NSString
            self.toggleCheckPreReleaseUpdates(checkPreRelease.boolValue)
        }
    }
    
    // MARK: - Class Methods
    static func defaultExcludeList() -> [AnyObject] {
        let appInfo = [kCPYBundleIdentifierKey: "org.openoffice.script", kCPYNameKey: "OpenOffice.org"]
        let excludeList = [appInfo]
        return excludeList
    }
    
    static func storeTypesDictinary() -> [String: NSNumber] {
        var storeTypes = [String: NSNumber]()
        for name in CPYClipData.availableTypesString() {
            storeTypes[name] = NSNumber(bool: true)
        }
        return storeTypes
    }
    
    
    // MARK: - Menu Actions
    internal func showPreferenceWindow() {
        // CPYPreferenceWindowController.sharedPrefsWindowController().showWindow(nil)
    }
    
    internal func showSnippetEditorWindow() {
        // self.snippetEditorController.showWindow(self)
    }
    
    internal func clearAllHistory() {
        let defaults = NSUserDefaults.standardUserDefaults()
        
        let isShowAlert = defaults.boolForKey(kCPYPrefShowAlertBeforeClearHistoryKey)
        if isShowAlert {
            let alert = NSAlert()
            alert.messageText = NSLocalizedString("Clear History", comment: "")
            alert.informativeText = NSLocalizedString("Are you sure you want to clear your clipboard history?", comment: "")
            alert.addButtonWithTitle(NSLocalizedString("Clear History", comment: ""))
            alert.addButtonWithTitle(NSLocalizedString("Cancel", comment: ""))
            alert.showsSuppressionButton = true
            
            NSApp.activateIgnoringOtherApps(true)
            
            if alert.suppressionButton?.state == NSOnState {
                defaults.setBool(false, forKey: kCPYPrefShowAlertBeforeClearHistoryKey)
            }
            
            let result = alert.runModal()
            if result != NSAlertFirstButtonReturn {
                return
            }
        }
        
        //CPYClipManager.sharedManager.clearAll()
    }
    
    internal func selectClipMenuItem(sender: NSMenuItem) {
//        CPYClipManager.sharedManager.copyClipToPasteboardAtIndex(sender.tag)
//        CPYUtil.paste()
    }
    
    internal func selectSnippetMenuItem(sender: AnyObject) {
//        let snippet = sender.representedObject
//        if snippet == nil {
//            NSBeep()
//            return
//        }
//        
//        if let content = (snippet as? CPYSnippet)?.content {
//            CPYClipManager.sharedManager.copyStringToPasteboard(content)
//            CPYUtil.paste()
//        }
    }
    
    // MARK: - Login Item Methods
    private func promptToAddLoginItems() {
        let alert = NSAlert()
        alert.messageText = NSLocalizedString("Launch Clipy on system startup?", comment: "")
        alert.informativeText = NSLocalizedString("You can change this setting in the Preferences if you want.", comment: "")
        alert.addButtonWithTitle(NSLocalizedString("Launch on system startup", comment: ""))
        alert.addButtonWithTitle(NSLocalizedString("Don't Launch", comment: ""))
        alert.showsSuppressionButton = true
        NSApp.activateIgnoringOtherApps(true)
        
        let defaults = NSUserDefaults.standardUserDefaults()
        
        // 起動する選択時
        if alert.runModal() == NSAlertFirstButtonReturn {
            defaults.setBool(true, forKey: kCPYPrefLoginItemKey)
            self.toggleLoginItemState()
        }
        // Do not show this message again
        if alert.suppressionButton?.state == NSOnState {
            defaults.setBool(true, forKey: kCPYPrefSuppressAlertForLoginItemKey)
        }
        defaults.synchronize()
    }
    
    private func toggleAddingToLoginItems(enable: Bool) {
        let appPath = NSBundle.mainBundle().bundlePath
        if enable {
            NMLoginItems.addPathToLoginItems(appPath, hide: false)
        } else {
            NMLoginItems.removePathFromLoginItems(appPath)
        }
    }
    
    private func toggleLoginItemState() {
        let isInLoginItems = NSUserDefaults.standardUserDefaults().boolForKey(kCPYPrefLoginItemKey)
        self.toggleAddingToLoginItems(isInLoginItems)
    }
    
    // MARK: - NSNotificationCenter Methods
    internal func handlePreferencePanelWillClose(notification: NSNotification) {
        self.toggleLoginItemState()
    }
    
    // MARK: - Version Up Methods
    private func toggleCheckPreReleaseUpdates(enable: Bool) {
        /*
        NSString *feed = (flag)
        ? [CMUtilities infoValueForKey:@"SUPreReleaseFeedURL"]
        : [CMUtilities infoValueForKey:@"SUFeedURL"];
        
        NSURL *feedURL = [[NSURL alloc] initWithString:feed];
        [[SUUpdater sharedUpdater] setFeedURL:feedURL];
        [feedURL release], feedURL = nil;
        */
    }

}

// MARK: - NSApplication Delegate
extension AppDelegate: NSApplicationDelegate {

    func applicationDidFinishLaunching(aNotification: NSNotification) {
        CPYUtilities.registerUserDefaultKeys()
        
        let defaults = NSUserDefaults.standardUserDefaults()
        
        let queue = NSOperationQueue()
        // Regist Hotkeys
        queue.addOperationWithBlock { () -> Void in
            CPYHotKeyManager.sharedManager.registerHotKeys()
        }
        // Show Login Item
        if !defaults.boolForKey(kCPYPrefLoginItemKey) && !defaults.boolForKey(kCPYPrefSuppressAlertForLoginItemKey) {
            self.promptToAddLoginItems()
        }
        
        /*
        // Sparkleでアップデート確認
        let updater = SUUpdater.sharedUpdater()
        self.toggleCheckPreReleaseUpdates(defaults.boolForKey(kCPYEnableAutomaticCheckPreReleaseKey))
        updater.automaticallyChecksForUpdates = true // [defaults boolForKey:CMEnableAutomaticCheckKey]
        //updater.updateCheckInterval = [defaults integerForKey:CMUpdateCheckIntervalKey]
        */
        
        defaults.addObserver(self, forKeyPath: kCPYEnableAutomaticCheckPreReleaseKey, options: NSKeyValueObservingOptions.New, context: nil)
        
        queue.waitUntilAllOperationsAreFinished()
    }
    
    func applicationWillTerminate(aNotification: NSNotification) {
        CPYHotKeyManager.sharedManager.unRegisterHotKeys()
    }
 
}
