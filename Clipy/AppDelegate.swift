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
        MenuManager.sharedManager.setup()
        ClipManager.sharedManager.setup()
        HistoryManager.sharedManager.setup()
        
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
            if CPYClip.allObjects().count == 0 {
                return false
            }
        }
        return true
    }
    
    // MARK: - Class Methods
    static func storeTypesDictinary() -> [String: NSNumber] {
        let storeTypes = CPYClipData.availableTypesString.reduce([String: NSNumber]()) { (var dict, type) in
            dict[type] = NSNumber(bool: true)
            return dict
        }
        return storeTypes
    }

    // MARK: - Menu Actions
    func showPreferenceWindow() {
        NSApp.activateIgnoringOtherApps(true)
        CPYPreferencesWindowController.sharedController.showWindow(self)
//        CPYPreferenceWindowController.sharedPrefsWindowController().showWindow(self)
    }
    
    func showSnippetEditorWindow() {
        NSApp.activateIgnoringOtherApps(true)
        snippetEditorController.showWindow(self)
    }
    
    func clearAllHistory() {
        let isShowAlert = defaults.boolForKey(kCPYPrefShowAlertBeforeClearHistoryKey)
        if isShowAlert {
            let alert = NSAlert()
            alert.messageText = LocalizedString.ClearHistory.value
            alert.informativeText = LocalizedString.ConfirmClearHistory.value
            alert.addButtonWithTitle(LocalizedString.ClearHistory.value)
            alert.addButtonWithTitle(LocalizedString.Cancel.value)
            alert.showsSuppressionButton = true
            
            NSApp.activateIgnoringOtherApps(true)
        
            let result = alert.runModal()
            if result != NSAlertFirstButtonReturn { return }
            
            if alert.suppressionButton?.state == NSOnState {
                defaults.setBool(false, forKey: kCPYPrefShowAlertBeforeClearHistoryKey)
            }
            defaults.synchronize()
        }
        
        ClipManager.sharedManager.clearAll()
    }
    
    func selectClipMenuItem(sender: NSMenuItem) {
        Answers.logCustomEventWithName("selectClipMenuItem", customAttributes: nil)
        if let clip = sender.representedObject as? CPYClip where !clip.invalidated {
            PasteboardManager.sharedManager.copyClipToPasteboard(clip)
            CPYUtilities.paste()
        } else {
            NSBeep()
        }
    }
    
    func selectSnippetMenuItem(sender: AnyObject) {
        if let snippet = sender.representedObject as? CPYSnippet {
            PasteboardManager.sharedManager.copyStringToPasteboard(snippet.content)
            CPYUtilities.paste()
        } else {
            NSBeep()
        }
    }
    
    // MARK: - Login Item Methods
    private func promptToAddLoginItems() {
        let alert = NSAlert()
        alert.messageText = LocalizedString.LaunchClipy.value
        alert.informativeText = LocalizedString.LaunchSettingInfo.value
        alert.addButtonWithTitle(LocalizedString.LaunchOnStartup.value)
        alert.addButtonWithTitle(LocalizedString.DontLaunch.value)
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
        ClipManager.sharedManager.stopTimer()
    }
    
    func receiveWakeNotification() {
        ClipManager.sharedManager.startTimer()
    }
}
