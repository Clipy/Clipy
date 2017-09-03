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
        let isShowAlert = AppEnvironment.current.defaults.bool(forKey: Constants.UserDefaults.showAlertBeforeClearHistory)
        if isShowAlert {
            let alert = NSAlert()
            alert.messageText = LocalizedString.clearHistory.value
            alert.informativeText = LocalizedString.confirmClearHistory.value
            alert.addButton(withTitle: LocalizedString.clearHistory.value)
            alert.addButton(withTitle: LocalizedString.cancel.value)
            alert.showsSuppressionButton = true

            NSApp.activate(ignoringOtherApps: true)

            let result = alert.runModal()
            if result != NSAlertFirstButtonReturn { return }

            if alert.suppressionButton?.state == NSOnState {
                AppEnvironment.current.defaults.set(false, forKey: Constants.UserDefaults.showAlertBeforeClearHistory)
            }
            AppEnvironment.current.defaults.synchronize()
        }

        AppEnvironment.current.clipService.clearAll()
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

        let modifierFlags = NSEvent.modifierFlags()

        switch modifierFlags {
        case .option:
            AppEnvironment.current.clipService.clear(clip: clip)

        default:
            AppEnvironment.current.pasteService.copyToPasteboard(with: clip)
            AppEnvironment.current.pasteService.paste()
        }
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
        AppEnvironment.current.pasteService.copyToPasteboard(with: snippet.content)
        AppEnvironment.current.pasteService.paste()
    }

    func terminateApplication() {
        NSApplication.shared().terminate(nil)
    }

    // MARK: - Login Item Methods
    fileprivate func promptToAddLoginItems() {
        let alert = NSAlert()
        alert.messageText = LocalizedString.launchClipy.value
        alert.informativeText = LocalizedString.launchSettingInfo.value
        alert.addButton(withTitle: LocalizedString.launchOnStartup.value)
        alert.addButton(withTitle: LocalizedString.dontLaunch.value)
        alert.showsSuppressionButton = true
        NSApp.activate(ignoringOtherApps: true)

        //  Launch on system startup
        if alert.runModal() == NSAlertFirstButtonReturn {
            AppEnvironment.current.defaults.set(true, forKey: Constants.UserDefaults.loginItem)
            toggleLoginItemState()
        }
        // Do not show this message again
        if alert.suppressionButton?.state == NSOnState {
            AppEnvironment.current.defaults.set(true, forKey: Constants.UserDefaults.suppressAlertForLoginItem)
        }
        AppEnvironment.current.defaults.synchronize()
    }

    fileprivate func toggleAddingToLoginItems(_ enable: Bool) {
        let appPath = Bundle.main.bundlePath
        LoginServiceKit.removeLoginItems(at: appPath)
        if enable {
            LoginServiceKit.addLoginItems(at: appPath)
        }
    }

    fileprivate func toggleLoginItemState() {
        let isInLoginItems = AppEnvironment.current.defaults.bool(forKey: Constants.UserDefaults.loginItem)
        toggleAddingToLoginItems(isInLoginItems)
    }
}

// MARK: - NSApplication Delegate
extension AppDelegate: NSApplicationDelegate {

    func applicationDidFinishLaunching(_ aNotification: Notification) {
        // Environments
        AppEnvironment.replaceCurrent(environment: AppEnvironment.fromStorage())
        // UserDefaults
        CPYUtilities.registerUserDefaultKeys()
        // SDKs
        CPYUtilities.initSDKs()

        // Show Login Item
        if !AppEnvironment.current.defaults.bool(forKey: Constants.UserDefaults.loginItem) && !AppEnvironment.current.defaults.bool(forKey: Constants.UserDefaults.suppressAlertForLoginItem) {
            promptToAddLoginItems()
        }

        // Sparkle
        let updater = SUUpdater.shared()
        updater?.feedURL = Constants.Application.appcastURL
        updater?.automaticallyChecksForUpdates = AppEnvironment.current.defaults.bool(forKey: Constants.Update.enableAutomaticCheck)
        updater?.updateCheckInterval = TimeInterval(AppEnvironment.current.defaults.integer(forKey: Constants.Update.checkInterval))

        // Binding Events
        bind()

        // Services
        AppEnvironment.current.clipService.startMonitoring()
        AppEnvironment.current.dataCleanService.startMonitoring()
        AppEnvironment.current.excludeAppService.startMonitoring()
        AppEnvironment.current.hotKeyService.setupDefaultHotKeys()

        // Managers
        AppEnvironment.current.menuManager.setup()
    }

    func applicationWillTerminate(_ aNotification: Notification) {
        HotKeyCenter.shared.unregisterAll()
    }
}

// MARK: - Bind
fileprivate extension AppDelegate {
    fileprivate func bind() {
        // Login Item
        AppEnvironment.current.defaults.rx.observe(Bool.self, Constants.UserDefaults.loginItem, options: [.new])
            .filterNil()
            .subscribe(onNext: { [weak self] _ in
                self?.toggleLoginItemState()
            })
            .disposed(by: disposeBag)
        // Observe Screenshot
        AppEnvironment.current.defaults.rx.observe(Bool.self, Constants.Beta.observerScreenshot)
            .filterNil()
            .subscribe(onNext: { [weak self] enabled in
                self?.screenshotObserver.isEnabled = enabled
            })
            .disposed(by: disposeBag)
        // Observe Screenshot image
        screenshotObserver.rx.addedImage
            .subscribe(onNext: { image in
                AppEnvironment.current.clipService.create(with: image)
            })
            .disposed(by: disposeBag)
    }
}
