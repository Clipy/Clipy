//
//  AppDelegate.swift
//
//  Clipy
//  GitHub: https://github.com/clipy
//  HP: https://clipy-app.com
//
//  Created by Econa77 on 2015/06/21.
//
//  Copyright © 2015-2018 Clipy Project.
//

import Cocoa
import Dependencies
import LoginServiceKit
import Magnet
import RealmSwift
import RxCocoa
import RxSwift
import Screeen
import Sparkle

@NSApplicationMain
class AppDelegate: NSObject, NSMenuItemValidation {

    // MARK: - Properties
    private(set) var updaterController: SPUStandardUpdaterController?
    private let screenshotObserver = ScreenShotObserver()
    private let disposeBag = DisposeBag()
    private let historyPruningScheduler = SerialDispatchQueueScheduler(qos: .utility)

    @Dependency(\.context)
    var context
    @Dependency(\.pasteboardHistoryRepository)
    private var pasteboardHistoryRepository
    @Dependency(\.snippetRepository)
    private var snippetRepository

    // MARK: - Init
    override func awakeFromNib() {
        super.awakeFromNib()
        // Migrate Realm
        Realm.migration()
        prepareDependencies { values in
            try! values.bootstrapDatabase()
        }
    }

    // MARK: - NSMenuItem Validation
    func validateMenuItem(_ menuItem: NSMenuItem) -> Bool {
        if menuItem.action == #selector(AppDelegate.clearAllHistory) {
            return pasteboardHistoryRepository.hasHistories()
        }
        return true
    }

    // MARK: - Menu Actions
    @objc func showPreferenceWindow() {
        NSApp.activate(ignoringOtherApps: true)
        CPYPreferencesWindowController.sharedController.showWindow(self)
    }

    @objc func showSnippetEditorWindow() {
        NSApp.activate(ignoringOtherApps: true)
        CPYSnippetsEditorWindowController.sharedController.showWindow(self)
    }

    @objc func terminate() {
        terminateApplication()
    }

    @objc func clearAllHistory() {
        let isShowAlert = AppEnvironment.current.defaults.bool(forKey: Constants.UserDefaults.showAlertBeforeClearHistory)
        if isShowAlert {
            let alert = NSAlert()
            alert.messageText = String(localized: "Clear History")
            alert.informativeText = String(localized: "Are you sure you want to clear your clipboard history?")
            alert.addButton(withTitle: String(localized: "Clear History"))
            alert.addButton(withTitle: String(localized: "Cancel"))
            alert.showsSuppressionButton = true

            NSApp.activate(ignoringOtherApps: true)

            let result = alert.runModal()
            if result != NSApplication.ModalResponse.alertFirstButtonReturn { return }

            if alert.suppressionButton?.state == NSControl.StateValue.on {
                AppEnvironment.current.defaults.set(false, forKey: Constants.UserDefaults.showAlertBeforeClearHistory)
            }
            AppEnvironment.current.defaults.synchronize()
        }

        AppEnvironment.current.clipService.clearAll()
    }

    @objc func selectClipMenuItem(_ sender: NSMenuItem) {
        CPYUtilities.sendCustomLog(with: "selectClipMenuItem")
        guard let id = sender.representedObject as? PasteboardHistory.ID, let content = pasteboardHistoryRepository.fetchContent(id: id) else {
            NSSound.beep()
            return
        }

        AppEnvironment.current.pasteService.paste(id: id, content: content)
    }

    @objc func selectSnippetMenuItem(_ sender: AnyObject) {
        guard let id = sender.representedObject as? Snippet.ID, let snippet = snippetRepository.fetchSnippet(id: id) else {
            NSSound.beep()
            return
        }
        AppEnvironment.current.pasteService.copyToPasteboard(with: snippet.content)
        AppEnvironment.current.pasteService.paste()
    }

    func terminateApplication() {
        NSApplication.shared.terminate(nil)
    }

    // MARK: - Login Item Methods
    private func promptToAddLoginItems() {
        let alert = NSAlert()
        alert.messageText = String(localized: "Launch Clipy on system startup?")
        alert.informativeText = String(localized: "You can change this setting in the Preferences if you want")
        alert.addButton(withTitle: String(localized: "Launch on system startup"))
        alert.addButton(withTitle: String(localized: "Don't Launch"))
        alert.showsSuppressionButton = true
        NSApp.activate(ignoringOtherApps: true)

        //  Launch on system startup
        if alert.runModal() == NSApplication.ModalResponse.alertFirstButtonReturn {
            AppEnvironment.current.defaults.set(true, forKey: Constants.UserDefaults.loginItem)
            AppEnvironment.current.defaults.synchronize()
            reflectLoginItemState()
        }
        // Do not show this message again
        if alert.suppressionButton?.state == NSControl.StateValue.on {
            AppEnvironment.current.defaults.set(true, forKey: Constants.UserDefaults.suppressAlertForLoginItem)
            AppEnvironment.current.defaults.synchronize()
        }
    }

    private func toggleAddingToLoginItems(_ isEnable: Bool) {
        if isEnable {
            LoginServiceKit.addLoginItems()
        } else {
            LoginServiceKit.removeLoginItems()
        }
    }

    private func reflectLoginItemState() {
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

        guard context != .test else { return }

        // SDKs
        CPYUtilities.initSDKs()
        // Check Accessibility Permission
        AppEnvironment.current.accessibilityService.isAccessibilityEnabled(isPrompt: true)

        // Show Login Item
        if !AppEnvironment.current.defaults.bool(forKey: Constants.UserDefaults.loginItem) && !AppEnvironment.current.defaults.bool(forKey: Constants.UserDefaults.suppressAlertForLoginItem) {
            promptToAddLoginItems()
        }

        // Sparkle
        self.updaterController = SPUStandardUpdaterController(
            startingUpdater: AppEnvironment.current.defaults.bool(forKey: Constants.Update.enableAutomaticCheck),
            updaterDelegate: nil,
            userDriverDelegate: nil
        )
        updaterController?.updater.updateCheckInterval = TimeInterval(AppEnvironment.current.defaults.integer(forKey: Constants.Update.checkInterval))
        updaterController?.updater.clearFeedURLFromUserDefaults()

        // Binding Events
        bind()

        // Services
        AppEnvironment.current.clipService.startMonitoring()
        AppEnvironment.current.excludeAppService.startMonitoring()
        AppEnvironment.current.hotKeyService.setupDefaultHotKeys()

        // Managers
        AppEnvironment.current.menuManager.setup()
        // Screenshot
        screenshotObserver.delegate = self

        // Clean histories every 30 minutes
        Observable<Int>.interval(.seconds(60 * 30), scheduler: historyPruningScheduler)
            .subscribe(onNext: { [weak self] _ in
                let maxHistorySize = AppEnvironment.current.defaults.integer(forKey: Constants.UserDefaults.maxHistorySize)
                self?.pasteboardHistoryRepository.deleteOverflowingHistories(maxHistorySize: maxHistorySize)
            })
            .disposed(by: disposeBag)
    }

}

// MARK: - Bind
private extension AppDelegate {
    func bind() {
        // Login Item
        AppEnvironment.current.defaults.rx.observe(Bool.self, Constants.UserDefaults.loginItem, retainSelf: false)
            .compactMap { $0 }
            .subscribe(onNext: { [weak self] _ in
                self?.reflectLoginItemState()
            })
            .disposed(by: disposeBag)
        // Observe Screenshot
        let observerScreenshot = AppEnvironment.current.defaults.rx.observe(Bool.self, Constants.Beta.observerScreenshot, retainSelf: false)
            .compactMap { $0 }
            .share(replay: 1)
        observerScreenshot
            .subscribe(onNext: { [weak self] enabled in
                self?.screenshotObserver.isEnabled = enabled
            })
            .disposed(by: disposeBag)
        observerScreenshot
            .filter { $0 }
            .take(1)
            .subscribe(onNext: { [weak self] _ in
                self?.screenshotObserver.start()
            })
            .disposed(by: disposeBag)
    }
}

// MARK: - ScreenShotObserver Delegate
extension AppDelegate: ScreenShotObserverDelegate {
    func screenShotObserver(_ observer: ScreenShotObserver, addedItem item: NSMetadataItem) {
        guard let path = item.value(forAttribute: NSMetadataItemPathKey) as? String else { return }
        guard let image = NSImage(contentsOfFile: path) else { return }
        AppEnvironment.current.clipService.create(with: image)
    }
}
