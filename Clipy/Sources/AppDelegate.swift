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
import Combine
import Dependencies
import Magnet
import Screeen
import ServiceManagement
import Sharing

class AppDelegate: NSObject, NSMenuItemValidation {

    // MARK: - Properties
    private let screenshotObserver = ScreenShotObserver()
    private var cancellables: Set<AnyCancellable> = []

    @Dependency(\.context)
    var context
    @Dependency(\.mainQueue)
    private var mainQueue
    @Dependency(\.excludeAppService)
    private var excludeAppService
    @Dependency(\.pasteboardHistoryRepository)
    private var pasteboardHistoryRepository
    @Dependency(\.snippetRepository)
    private var snippetRepository
    @Dependency(\.firebase)
    private var firebase
    @Dependency(\.clipService)
    private var clipService
    @Dependency(\.hotKeyService)
    private var hotKeyService
    @Dependency(\.pasteService)
    private var pasteService
    @Dependency(\.menuManager)
    private var menuManager
    @Dependency(\.sparkle)
    private var sparkle

    @Shared(.isLaunchAtLogin)
    private var isLaunchAtLogin
    @Shared(.suppressesLoginItemAlert)
    private var suppressesLoginItemAlert
    @Shared(.pastesAutomatically)
    private var pastesAutomatically
    @Shared(.maximumHistoryCount)
    private var maximumHistoryCount
    @Shared(.reordersClipsAfterPasting)
    private var reordersClipsAfterPasting
    @Shared(.observesScreenshots)
    private var observesScreenshots

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
        clipService.clearAllHistory()
    }

    @objc func selectClipMenuItem(_ sender: NSMenuItem) {
        guard let id = sender.representedObject as? PasteboardHistory.ID, let content = pasteboardHistoryRepository.fetchContent(id: id) else {
            NSSound.beep()
            return
        }

        pasteService.paste(id: id, content: content)
    }

    @objc func selectSnippetMenuItem(_ sender: AnyObject) {
        guard let id = sender.representedObject as? Snippet.ID, let snippet = snippetRepository.fetchSnippet(id: id) else {
            NSSound.beep()
            return
        }
        pasteService.copyToPasteboard(with: snippet.content)
        pasteService.paste()
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
            $isLaunchAtLogin.withLock { $0 = true }
        }
        // Do not show this message again
        if alert.suppressionButton?.state == NSControl.StateValue.on {
            $suppressesLoginItemAlert.withLock { $0 = true }
        }
    }
}

// MARK: - NSApplication Delegate
extension AppDelegate: NSApplicationDelegate {

    func applicationDidFinishLaunching(_ aNotification: Notification) {
        guard !isTesting else { return }

        AppStorageValues.register()
        AppMigrator().run()

        // SDKs
        firebase.configure()
        // Check Accessibility Permission
        if pastesAutomatically {
            Accessibility.isAccessibilityEnabled(isPrompt: true)
        }

        // Show Login Item
        if !isLaunchAtLogin && !suppressesLoginItemAlert {
            promptToAddLoginItems()
        }

        // Sparkle
        sparkle.configure()

        // Binding Events
        bind()

        // Services
        clipService.startMonitoring()
        excludeAppService.startMonitoring()
        hotKeyService.setupDefaultHotKeys()

        // Managers
        menuManager.setup()
        // Screenshot
        screenshotObserver.delegate = self

        // Periodically trim excess history using the current size limit and sort preference.
        Task(priority: .utility) { [weak self] in
            @Dependency(\.continuousClock) var continuousClock

            for await _ in continuousClock.timer(interval: .seconds(60)) {
                guard let self else { return }
                pasteboardHistoryRepository.deleteOverflowingHistories(
                    sortsByCreatedAt: !reordersClipsAfterPasting,
                    maxHistorySize: maximumHistoryCount
                )
            }
        }
    }

}

// MARK: - Bind
private extension AppDelegate {
    func bind() {
        // Accessibility Permission
        $pastesAutomatically.changes()
            .filter { $0 }
            .receive(on: mainQueue)
            .sink { _ in
                Accessibility.isAccessibilityEnabled(isPrompt: true)
            }
            .store(in: &cancellables)
        // Login Item
        $isLaunchAtLogin.changes(includingInitialValue: true)
            .receive(on: mainQueue)
            .sink { isEnabled in
                if isEnabled {
                    guard SMAppService.mainApp.status != .enabled else { return }
                    try? SMAppService.mainApp.register()
                } else {
                    guard SMAppService.mainApp.status != .notRegistered else { return }
                    try? SMAppService.mainApp.unregister()
                }
            }
            .store(in: &cancellables)
        // Observe Screenshot
        $observesScreenshots.changes(includingInitialValue: true)
            .receive(on: mainQueue)
            .sink { [weak self] enabled in
                self?.screenshotObserver.isEnabled = enabled
            }
            .store(in: &cancellables)
        $observesScreenshots.changes(includingInitialValue: true)
            .filter { $0 }
            .first()
            .receive(on: mainQueue)
            .sink { [weak self] _ in
                self?.screenshotObserver.start()
            }
            .store(in: &cancellables)
    }
}

// MARK: - ScreenShotObserver Delegate
extension AppDelegate: ScreenShotObserverDelegate {
    func screenShotObserver(_ observer: ScreenShotObserver, addedItem item: NSMetadataItem) {
        guard let path = item.value(forAttribute: NSMetadataItemPathKey) as? String else { return }
        guard let image = NSImage(contentsOfFile: path) else { return }
        clipService.create(with: image)
    }
}
