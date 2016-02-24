//
//  AppDelegate.swift
//  Clipy
//
//  Created by 古林俊佑 on 2015/06/21.
//  Copyright (c) 2015年 Shunsuke Furubayashi. All rights reserved.
//

import Cocoa
import Sparkle
import Fabric
import Crashlytics

@NSApplicationMain
class AppDelegate: NSObject {

    // MARK: - Properties
    let snippetEditorController = CPYSnippetEditorWindowController(windowNibName: "CPYSnippetEditorWindowController")
    let defaults = NSUserDefaults.standardUserDefaults()
    
    // MARK: - Init
    override func awakeFromNib() {
        super.awakeFromNib()
        initController()
    }

    private func initController() {
        CPYUtilities.registerUserDefaultKeys()
        
        // Migrate Realm
        CPYUtilities.migrationRealm()

        // Show menubar icon
        CPYMenuManager.sharedManager

        defaults.addObserver(self, forKeyPath: kCPYPrefLoginItemKey, options: .New, context: nil)
    }
    
    deinit {
        NSNotificationCenter.defaultCenter().removeObserver(self)
        NSWorkspace.sharedWorkspace().notificationCenter.removeObserver(self)
    }
    
    // MARK: - KVO 
    override func observeValueForKeyPath(keyPath: String?, ofObject object: AnyObject?, change: [String : AnyObject]?, context: UnsafeMutablePointer<Void>) {
        if keyPath == kCPYPrefLoginItemKey {
            toggleLoginItemState()
        }
    }

    // MARK: - Override Methods
    override func validateMenuItem(menuItem: NSMenuItem) -> Bool {
        if menuItem.action == Selector("clearAllHistory") {
            if CPYClipManager.sharedManager.loadClips().count == 0 {
                return false
            }
        }
        return true
    }
    
    // MARK: - Class Methods
    static func storeTypesDictinary() -> [String: NSNumber] {
        let storeTypes = CPYClipData.availableTypesString().reduce([String: NSNumber]()) { (var dict, type) in
            dict[type] = NSNumber(bool: true)
            return dict
        }
        return storeTypes
    }

    // MARK: - Menu Actions
    func showPreferenceWindow() {
        NSApp.activateIgnoringOtherApps(true)
        CPYPreferenceWindowController.sharedPrefsWindowController().showWindow(self)
    }
    
    func showSnippetEditorWindow() {
        NSApp.activateIgnoringOtherApps(true)
        snippetEditorController.showWindow(self)
    }
    
    func clearAllHistory() {
        let isShowAlert = defaults.boolForKey(kCPYPrefShowAlertBeforeClearHistoryKey)
        if isShowAlert {
            let alert = NSAlert()
            alert.messageText = NSLocalizedString("Clear History", comment: "")
            alert.informativeText = NSLocalizedString("Are you sure you want to clear your clipboard history?", comment: "")
            alert.addButtonWithTitle(NSLocalizedString("Clear History", comment: ""))
            alert.addButtonWithTitle(NSLocalizedString("Cancel", comment: ""))
            alert.showsSuppressionButton = true
            
            NSApp.activateIgnoringOtherApps(true)
        
            let result = alert.runModal()
            if result != NSAlertFirstButtonReturn { return }
            
            if alert.suppressionButton?.state == NSOnState {
                defaults.setBool(false, forKey: kCPYPrefShowAlertBeforeClearHistoryKey)
            }
            defaults.synchronize()
        }
        
        CPYClipManager.sharedManager.clearAll()
    }
    
    func selectClipMenuItem(sender: NSMenuItem) {
        Answers.logCustomEventWithName("selectClipMenuItem", customAttributes: nil)
        CPYClipManager.sharedManager.copyClipToPasteboardAtIndex(sender.tag)
        CPYUtilities.paste()
    }
    
    func selectSnippetMenuItem(sender: AnyObject) {
        let snippet = sender.representedObject
        if snippet == nil {
            NSBeep()
            return
        }
        
        if let content = (snippet as? CPYSnippet)?.content {
            CPYClipManager.sharedManager.copyStringToPasteboard(content)
            CPYUtilities.paste()
        }
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

        // 起動する選択時
        if alert.runModal() == NSAlertFirstButtonReturn {
            defaults.setBool(true, forKey: kCPYPrefLoginItemKey)
            toggleLoginItemState()
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
            NMLoginItems.removePathFromLoginItems(appPath)
            NMLoginItems.addPathToLoginItems(appPath, hide: false)
        } else {
            NMLoginItems.removePathFromLoginItems(appPath)
        }
    }
    
    private func toggleLoginItemState() {
        let isInLoginItems = NSUserDefaults.standardUserDefaults().boolForKey(kCPYPrefLoginItemKey)
        toggleAddingToLoginItems(isInLoginItems)
    }
    
    // MARK: - Version Up Methods
    private func checkUpdates() {
        let feed = "https://clipy-app.com/appcast.xml"
        if let feedURL = NSURL(string: feed) {
            SUUpdater.sharedUpdater().feedURL = feedURL
        }
    }

}

// MARK: - NSApplication Delegate
extension AppDelegate: NSApplicationDelegate {

    func applicationDidFinishLaunching(aNotification: NSNotification) {
        // Fabric
        defaults.registerDefaults(["NSApplicationCrashOnExceptions": true])
        Fabric.with([Answers.self, Crashlytics.self])
        Answers.logCustomEventWithName("applicationDidFinishLaunching", customAttributes: nil)
        
        CPYUtilities.registerUserDefaultKeys()

        let queue = NSOperationQueue()
        // Regist Hotkeys
        queue.addOperationWithBlock {
            CPYHotKeyManager.sharedManager.registerHotKeys()
        }
        // Show Login Item
        if !defaults.boolForKey(kCPYPrefLoginItemKey) && !defaults.boolForKey(kCPYPrefSuppressAlertForLoginItemKey) {
            promptToAddLoginItems()
        }
        
        // Sparkleでアップデート確認
        let updater = SUUpdater.sharedUpdater()
        checkUpdates()
        updater.automaticallyChecksForUpdates = defaults.boolForKey(kCPYEnableAutomaticCheckKey)
        updater.updateCheckInterval = NSTimeInterval(defaults.integerForKey(kCPYUpdateCheckIntervalKey))
    
        // スリープ時にタイマーを停止する
        addSleepNotifications()
        
        queue.waitUntilAllOperationsAreFinished()
    }
    
    func applicationWillTerminate(aNotification: NSNotification) {
        CPYHotKeyManager.sharedManager.unRegisterHotKeys()
    }
}

// MARK: - NSNotificationCenter 
extension AppDelegate {
    private func addSleepNotifications() {
        NSWorkspace.sharedWorkspace().notificationCenter.addObserver(self, selector: "receiveSleepNotification", name: NSWorkspaceWillSleepNotification, object: nil)
        NSWorkspace.sharedWorkspace().notificationCenter.addObserver(self, selector: "receiveWakeNotification", name: NSWorkspaceDidWakeNotification, object: nil)
    }
    
    func receiveSleepNotification() {
        CPYClipManager.sharedManager.stopPasteboardObservingTimer()
    }
    
    func receiveWakeNotification() {
        CPYClipManager.sharedManager.startPasteboardObservingTimer()
    }
}
