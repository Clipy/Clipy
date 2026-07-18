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
import Clocks
import Dependencies
import Magnet
import RxCocoa
import RxSwift
import Screeen
import ServiceManagement
import Sharing
import Sparkle

class AppDelegate: NSObject, NSMenuItemValidation {

    // MARK: - Properties
    private(set) var updaterController: SPUStandardUpdaterController?
    private let screenshotObserver = ScreenShotObserver(searchDirectoryPaths: AppDelegate.screenshotSearchDirectoryPaths())
    private let disposeBag = DisposeBag()

    @Dependency(\.context)
    var context
    @Dependency(\.continuousClock)
    private var continuousClock
    @Dependency(\.defaultAppStorage)
    var appStorage
    @Dependency(\.pasteboardHistoryRepository)
    private var pasteboardHistoryRepository
    @Dependency(\.snippetRepository)
    private var snippetRepository
    @Dependency(\.firebase)
    private var firebase

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

    @objc func showHistorySearchWindow() {
        HistorySearchWindowController.shared.showSearchWindow()
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
        }
        // Do not show this message again
        if alert.suppressionButton?.state == NSControl.StateValue.on {
            AppEnvironment.current.defaults.set(true, forKey: Constants.UserDefaults.suppressAlertForLoginItem)
            AppEnvironment.current.defaults.synchronize()
        }
    }
}

// MARK: - NSApplication Delegate
extension AppDelegate: NSApplicationDelegate {

    func applicationDidFinishLaunching(_ aNotification: Notification) {
        // Environments
        AppEnvironment.replaceCurrent(environment: AppEnvironment.fromStorage())
        // UserDefaults
        CPYUtilities.registerUserDefaultKeys(AppEnvironment.current.defaults)

        guard context != .test else { return }

        // SDKs
        firebase.configure()
        // Check Accessibility Permission
        Accessibility.isAccessibilityEnabled(isPrompt: true)

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

        // Periodically trim excess history using the current size limit and sort preference.
        Task(priority: .utility) { [weak self] in
            guard let self else { return }
            for await _ in continuousClock.timer(interval: .seconds(60)) {
                let maxHistorySize = appStorage.integer(forKey: Constants.UserDefaults.maxHistorySize)
                let reorderClipsAfterPasting = appStorage.bool(forKey: Constants.UserDefaults.reorderClipsAfterPasting)
                pasteboardHistoryRepository.deleteOverflowingHistories(
                    sortsByCreatedAt: !reorderClipsAfterPasting,
                    maxHistorySize: maxHistorySize
                )
            }
        }
    }

}

// MARK: - Bind
private extension AppDelegate {
    func bind() {
        // Login Item
        AppEnvironment.current.defaults.rx.observe(Bool.self, Constants.UserDefaults.loginItem, retainSelf: false)
            .compactMap { $0 }
            .subscribe(onNext: { isEnabled in
                if isEnabled {
                    guard SMAppService.mainApp.status != .enabled else { return }
                    try? SMAppService.mainApp.register()
                } else {
                    guard SMAppService.mainApp.status != .notRegistered else { return }
                    try? SMAppService.mainApp.unregister()
                }
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

// MARK: - Screenshot Search Paths
private extension AppDelegate {
    /// Directories the screenshot observer watches via Spotlight.
    ///
    /// Screeen's default scope is the Desktop only, so screenshots saved to a
    /// custom location (configured via `defaults write com.apple.screencapture
    /// location <path>`) are never detected. Read that configured destination
    /// and watch it in addition to the Desktop.
    static func screenshotSearchDirectoryPaths() -> [String] {
        var paths: [String] = []
        if let location = CFPreferencesCopyAppValue("location" as CFString,
                                                    "com.apple.screencapture" as CFString) as? String {
            paths.append((location as NSString).expandingTildeInPath)
        }
        if let desktop = NSSearchPathForDirectoriesInDomains(.desktopDirectory, .userDomainMask, true).first {
            paths.append(desktop)
        }
        // Deduplicate while preserving order. When empty, Screeen leaves the
        // query scope unset and searches all indexed locations as a fallback.
        var seen = Set<String>()
        return paths.filter { seen.insert($0).inserted }
    }
}
