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
import LoginServiceKit
import Magnet
import Screeen
import RxScreeen
import RealmSwift

@NSApplicationMain
class AppDelegate: NSObject {

    // MARK: - Properties
    let defaults = UserDefaults.standard
    let screenshotObserver = ScreenShotObserver()
    let disposeBag = DisposeBag()

    // MARK: - Init
    override func awakeFromNib() {
        super.awakeFromNib()
        // Migrate Realm
        Realm.migration()
    }

    // MARK: - Override Methods
    override func validateMenuItem(_ menuItem: NSMenuItem) -> Bool {
        if menuItem.action == #selector(AppDelegate.clearAllHistory) {
            let realm = try! Realm()
            return !realm.objects(CPYClip.self).isEmpty
        }
        return true
    }

    // MARK: - Class Methods
    static func storeTypesDictinary() -> [String: NSNumber] {
        var storeTypes = [String: NSNumber]()
        CPYClipData.availableTypesString.forEach { storeTypes[$0] = NSNumber(value: true) }
        return storeTypes
    }

    // MARK: - Menu Actions
    func showPreferenceWindow() {
        NSApp.activate(ignoringOtherApps: true)
        CPYPreferencesWindowController.sharedController.showWindow(self)
    }

    func showSnippetEditorWindow() {
        NSApp.activate(ignoringOtherApps: true)
        CPYSnippetsEditorWindowController.sharedController.showWindow(self)
    }

    func clearAllHistory() {
        let isShowAlert = defaults.bool(forKey: Constants.UserDefaults.showAlertBeforeClearHistory)
        if isShowAlert {
            let alert = NSAlert()
            alert.messageText = LocalizedString.ClearHistory.value
            alert.informativeText = LocalizedString.ConfirmClearHistory.value
            alert.addButton(withTitle: LocalizedString.ClearHistory.value)
            alert.addButton(withTitle: LocalizedString.Cancel.value)
            alert.showsSuppressionButton = true

            NSApp.activate(ignoringOtherApps: true)

            let result = alert.runModal()
            if result != NSAlertFirstButtonReturn { return }

            if alert.suppressionButton?.state == NSOnState {
                defaults.set(false, forKey: Constants.UserDefaults.showAlertBeforeClearHistory)
            }
            defaults.synchronize()
        }

        ClipService.shared.clearAll()
    }

    func selectClipMenuItem(_ sender: NSMenuItem) {
        CPYUtilities.sendCustomLog(with: "selectClipMenuItem")
        guard let primaryKey = sender.representedObject as? String else {
            CPYUtilities.sendCustomLog(with: "Cannot fetch clip primary key")
            NSBeep()
            return
        }
        let realm = try! Realm()
        guard let clip = realm.object(ofType: CPYClip.self, forPrimaryKey: primaryKey) else {
            CPYUtilities.sendCustomLog(with: "Cannot fetch clip data")
            NSBeep()
            return
        }

        PasteboardManager.sharedManager.copyClipToPasteboard(clip)
        PasteboardManager.paste()
    }

    func selectSnippetMenuItem(_ sender: AnyObject) {
        CPYUtilities.sendCustomLog(with: "selectSnippetMenuItem")
        guard let primaryKey = sender.representedObject as? String else {
            CPYUtilities.sendCustomLog(with: "Cannot fetch snippet primary key")
            NSBeep()
            return
        }
        let realm = try! Realm()
        guard let snippet = realm.object(ofType: CPYSnippet.self, forPrimaryKey: primaryKey) else {
            CPYUtilities.sendCustomLog(with: "Cannot fetch snippet data")
            NSBeep()
            return
        }
        PasteboardManager.sharedManager.copyStringToPasteboard(snippet.content)
        PasteboardManager.paste()
    }

    func terminateApplication() {
        NSApplication.shared().terminate(nil)
    }

    // MARK: - Login Item Methods
    fileprivate func promptToAddLoginItems() {
        let alert = NSAlert()
        alert.messageText = LocalizedString.LaunchClipy.value
        alert.informativeText = LocalizedString.LaunchSettingInfo.value
        alert.addButton(withTitle: LocalizedString.LaunchOnStartup.value)
        alert.addButton(withTitle: LocalizedString.DontLaunch.value)
        alert.showsSuppressionButton = true
        NSApp.activate(ignoringOtherApps: true)

        //  Launch on system startup
        if alert.runModal() == NSAlertFirstButtonReturn {
            defaults.set(true, forKey: Constants.UserDefaults.loginItem)
            toggleLoginItemState()
        }
        // Do not show this message again
        if alert.suppressionButton?.state == NSOnState {
            defaults.set(true, forKey: Constants.UserDefaults.suppressAlertForLoginItem)
        }
        defaults.synchronize()
    }

    fileprivate func toggleAddingToLoginItems(_ enable: Bool) {
        let appPath = Bundle.main.bundlePath
        LoginServiceKit.removeLoginItems(at: appPath)
        if enable {
            LoginServiceKit.addLoginItems(at: appPath)
        }
    }

    fileprivate func toggleLoginItemState() {
        let isInLoginItems = UserDefaults.standard.bool(forKey: Constants.UserDefaults.loginItem)
        toggleAddingToLoginItems(isInLoginItems)
    }
}

// MARK: - NSApplication Delegate
extension AppDelegate: NSApplicationDelegate {

    func applicationDidFinishLaunching(_ aNotification: Notification) {
        // UserDefaults
        CPYUtilities.registerUserDefaultKeys()

        // SDKs
        CPYUtilities.initSDKs()

        // Show Login Item
        if !defaults.bool(forKey: Constants.UserDefaults.loginItem) && !defaults.bool(forKey: Constants.UserDefaults.suppressAlertForLoginItem) {
            promptToAddLoginItems()
        }

        // Sparkle
        let updater = SUUpdater.shared()
        updater?.feedURL = Constants.Application.appcastURL
        updater?.automaticallyChecksForUpdates = defaults.bool(forKey: Constants.Update.enableAutomaticCheck)
        updater?.updateCheckInterval = TimeInterval(defaults.integer(forKey: Constants.Update.checkInterval))

        // Binding Events
        bind()

        // Services
        _ = ClipService.shared
        _ = DataCleanService.shared
        HotKeyService.shared.setupDefaultHotKeys()

        // Managers
        MenuManager.sharedManager.setup()
    }

    func applicationWillTerminate(_ aNotification: Notification) {
        HotKeyCenter.shared.unregisterAll()
    }
}

// MARK: - Bind
fileprivate extension AppDelegate {
    fileprivate func bind() {
        // Login Item
        defaults.rx.observe(Bool.self, Constants.UserDefaults.loginItem, options: [.new])
            .filterNil()
            .subscribe(onNext: { [weak self] enabled in
                self?.toggleLoginItemState()
            }).addDisposableTo(disposeBag)
        // Observe Screenshot
        defaults.rx.observe(Bool.self, Constants.Beta.observerScreenshot)
            .filterNil()
            .subscribe(onNext: { [weak self] enabled in
                self?.screenshotObserver.isEnabled = enabled
            }).addDisposableTo(disposeBag)
        // Observe Screenshot image
        screenshotObserver.rx.addedImage
            .subscribe(onNext: { image in
                ClipService.shared.create(with: image)
            }).addDisposableTo(disposeBag)
    }
}
