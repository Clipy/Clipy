//
//  AppDelegate.swift
//  Clipy
//
//  Created by 古林俊佑 on 2015/06/21.
//  Copyright (c) 2015年 Shunsuke Furubayashi. All rights reserved.
//

import Cocoa
import Sparkle
import RxCocoa
import RxSwift
import RxOptional
import NSObject_Rx
import LoginServiceKit
import Magnet
import Screeen
import RxScreeen
import RealmSwift

@NSApplicationMain
class AppDelegate: NSObject {

    // MARK: - Properties
    let defaults = NSUserDefaults.standardUserDefaults()
    let screenshotObserver = ScreenShotObserver()

    // MARK: - Init
    override func awakeFromNib() {
        super.awakeFromNib()
        // Migrate Realm
        Realm.migration()
    }

    // MARK: - Override Methods
    override func validateMenuItem(menuItem: NSMenuItem) -> Bool {
        if menuItem.action == #selector(AppDelegate.clearAllHistory) {
            let realm = try! Realm()
            return !realm.objects(CPYClip.self).isEmpty
        }
        return true
    }

    // MARK: - Class Methods
    static func storeTypesDictinary() -> [String: NSNumber] {
        var storeTypes = [String: NSNumber]()
        CPYClipData.availableTypesString.forEach { storeTypes[$0] = NSNumber(bool: true) }
        return storeTypes
    }

    // MARK: - Menu Actions
    func showPreferenceWindow() {
        NSApp.activateIgnoringOtherApps(true)
        CPYPreferencesWindowController.sharedController.showWindow(self)
    }

    func showSnippetEditorWindow() {
        NSApp.activateIgnoringOtherApps(true)
        CPYSnippetsEditorWindowController.sharedController.showWindow(self)
    }

    func clearAllHistory() {
        let isShowAlert = defaults.boolForKey(Constants.UserDefaults.showAlertBeforeClearHistory)
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
                defaults.setBool(false, forKey: Constants.UserDefaults.showAlertBeforeClearHistory)
            }
            defaults.synchronize()
        }

        ClipManager.sharedManager.clearAll()
    }

    func selectClipMenuItem(sender: NSMenuItem) {
        CPYUtilities.sendCustomLog(with: "selectClipMenuItem")
        guard let primaryKey = sender.representedObject as? String else {
            CPYUtilities.sendCustomLog(with: "Cannot fetch clip primary key")
            NSBeep()
            return
        }
        let realm = try! Realm()
        guard let clip = realm.objectForPrimaryKey(CPYClip.self, key: primaryKey) else {
            CPYUtilities.sendCustomLog(with: "Cannot fetch clip data")
            NSBeep()
            return
        }

        PasteboardManager.sharedManager.copyClipToPasteboard(clip)
        PasteboardManager.paste()
    }

    func selectSnippetMenuItem(sender: AnyObject) {
        CPYUtilities.sendCustomLog(with: "selectSnippetMenuItem")
        guard let primaryKey = sender.representedObject as? String else {
            CPYUtilities.sendCustomLog(with: "Cannot fetch snippet primary key")
            NSBeep()
            return
        }
        let realm = try! Realm()
        guard let snippet = realm.objectForPrimaryKey(CPYSnippet.self, key: primaryKey) else {
            CPYUtilities.sendCustomLog(with: "Cannot fetch snippet data")
            NSBeep()
            return
        }
        PasteboardManager.sharedManager.copyStringToPasteboard(snippet.content)
        PasteboardManager.paste()
    }

    func terminateApplication() {
        NSApplication.sharedApplication().terminate(nil)
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

        //  Launch on system startup
        if alert.runModal() == NSAlertFirstButtonReturn {
            defaults.setBool(true, forKey: Constants.UserDefaults.loginItem)
            toggleLoginItemState()
        }
        // Do not show this message again
        if alert.suppressionButton?.state == NSOnState {
            defaults.setBool(true, forKey: Constants.UserDefaults.suppressAlertForLoginItem)
        }
        defaults.synchronize()
    }

    private func toggleAddingToLoginItems(enable: Bool) {
        let appPath = NSBundle.mainBundle().bundlePath
        LoginServiceKit.removePathFromLoginItems(appPath)
        if enable {
            LoginServiceKit.addPathToLoginItems(appPath)
        }
    }

    private func toggleLoginItemState() {
        let isInLoginItems = NSUserDefaults.standardUserDefaults().boolForKey(Constants.UserDefaults.loginItem)
        toggleAddingToLoginItems(isInLoginItems)
    }
}

// MARK: - NSApplication Delegate
extension AppDelegate: NSApplicationDelegate {

    func applicationDidFinishLaunching(aNotification: NSNotification) {
        // UserDefaults
        CPYUtilities.registerUserDefaultKeys()

        // SDKs
        CPYUtilities.initSDKs()

        // Regist Hotkeys
        HotKeyManager.sharedManager.setupDefaultHoyKey()

        // Show Login Item
        if !defaults.boolForKey(Constants.UserDefaults.loginItem) && !defaults.boolForKey(Constants.UserDefaults.suppressAlertForLoginItem) {
            promptToAddLoginItems()
        }

        // Sparkle
        let updater = SUUpdater.sharedUpdater()
        updater.feedURL = Constants.Application.appcastURL
        updater.automaticallyChecksForUpdates = defaults.boolForKey(Constants.Update.enableAutomaticCheck)
        updater.updateCheckInterval = NSTimeInterval(defaults.integerForKey(Constants.Update.checkInterval))

        // Binding Events
        bind()

        // Managers
        MenuManager.sharedManager.setup()
        ClipManager.sharedManager.setup()
        HistoryManager.sharedManager.setup()
    }

    func applicationWillTerminate(aNotification: NSNotification) {
        HotKeyCenter.sharedCenter.unregisterAll()
    }
}

// MARK: - Bind
private extension AppDelegate {
    private func bind() {
        // Login Item
        defaults.rx_observe(Bool.self, Constants.UserDefaults.loginItem, options: [.New])
            .filterNil()
            .subscribeNext { [weak self] enabled in
                self?.toggleLoginItemState()
            }.addDisposableTo(rx_disposeBag)
        // Observe Screenshot
        defaults.rx_observe(Bool.self, Constants.Beta.observerScreenshot)
            .filterNil()
            .subscribeNext { [weak self] enabled in
                self?.screenshotObserver.isEnabled = enabled
            }.addDisposableTo(rx_disposeBag)
        // Observe Screenshot image
        screenshotObserver.rx_addedImage
            .subscribeNext { image in
                ClipManager.sharedManager.createclip(image)
            }.addDisposableTo(rx_disposeBag)
        // Sleep Notification
        NSWorkspace.sharedWorkspace().notificationCenter.rx_notification(NSWorkspaceWillSleepNotification)
            .subscribeNext { notification in
                ClipManager.sharedManager.stopTimer()
            }.addDisposableTo(rx_disposeBag)
        NSWorkspace.sharedWorkspace().notificationCenter.rx_notification(NSWorkspaceDidWakeNotification)
            .subscribeNext { notification in
                ClipManager.sharedManager.startTimer()
            }.addDisposableTo(rx_disposeBag)
    }
}
