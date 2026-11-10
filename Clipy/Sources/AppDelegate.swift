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
import Combine
import Dependencies
import Magnet
import Screeen
import ServiceManagement
import Settings
import Sharing

class AppDelegate: NSObject, NSMenuItemValidation {

    // MARK: - Properties
    private let screenshotObserver = ScreenShotObserver()
    private var cancellables: Set<AnyCancellable> = []
    @MainActor private lazy var keyboardTypingService = KeyboardTypingService()
    @MainActor private lazy var settingsWindowController: SettingsWindowController = {
        let controller = SettingsWindowController(
            panes: SettingsPane.allCases.map { $0.asPanelConvertible() },
            animated: false
        )
        controller.window?.delegate = self
        return controller
    }()

    @Dependency(\.context)
    var context
    @Dependency(\.mainQueue)
    private var mainQueue
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
    @Shared(.observesScreenshots)
    private var observesScreenshots

    fileprivate enum URLSchemeDestination: String {
        case settings
        case snippetEditor

        static let scheme = "clipy"
    }

    // MARK: - NSMenuItem Validation
    func validateMenuItem(_ menuItem: NSMenuItem) -> Bool {
        if menuItem.action == #selector(AppDelegate.typeClipboardAsKeyboardInput) {
            return !(NSPasteboard.general.string(forType: .string)?.isEmpty ?? true)
        }
        if menuItem.action == #selector(AppDelegate.clearAllHistory) {
            return pasteboardHistoryRepository.hasHistories()
        }
        return true
    }

    // MARK: - Menu Actions
    @MainActor @objc func showSettingsWindow() {
        openWindow(.settings)
    }

    @MainActor @objc func showSnippetEditorWindow() {
        openWindow(.snippetEditor)
    }

    @objc func terminate() {
        terminateApplication()
    }

    @objc func clearAllHistory() {
        clipService.clearAllHistory()
    }

    @MainActor @objc func typeClipboardAsKeyboardInput() {
        guard let text = NSPasteboard.general.string(forType: .string), !text.isEmpty,
              let application = NSWorkspace.shared.frontmostApplication else {
            NSSound.beep()
            return
        }
        guard Accessibility.isAccessibilityEnabled(isPrompt: false) else {
            Accessibility.showAccessibilityAuthenticationAlert()
            return
        }
        keyboardTypingService.type(text, into: application.processIdentifier)
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
        alert.informativeText = String(localized: "You can change this later in Settings.")
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

// MARK: - Settings Window
extension AppDelegate: NSWindowDelegate {
    func windowWillClose(_ notification: Notification) {
        guard let window = notification.object as? NSWindow, !window.makeFirstResponder(nil) else { return }
        window.endEditing(for: nil)
    }
}

// MARK: - NSApplication Delegate
extension AppDelegate: NSApplicationDelegate {
    @MainActor func application(_ application: NSApplication, open urls: [URL]) {
        guard let url = urls.first(where: { $0.scheme?.lowercased() == URLSchemeDestination.scheme }) else { return }
        guard let destination = URLSchemeDestination(rawValue: url.host() ?? "") else { return }
        open(destination)
    }

    func applicationDidFinishLaunching(_ aNotification: Notification) {
        guard !isTesting else { return }

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
        hotKeyService.setupDefaultHotKeys()

        // Managers
        menuManager.setup()
        // Screenshot
        screenshotObserver.delegate = self
    }

    func applicationWillTerminate(_ notification: Notification) {
        guard !isTesting else { return }
        clipService.applicationWillTerminate()
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
            .first(where: { $0 })
            .receive(on: mainQueue)
            .sink { [weak self] _ in
                self?.screenshotObserver.start()
            }
            .store(in: &cancellables)
    }
}

// MARK: - Windows
private extension AppDelegate {
    /// Opens the window through a URL scheme because forcing window activation
    /// in a status bar application can be unreliable.
    ///
    /// ref: https://stackoverflow.com/questions/79710850/macos-cocoa-app-doesnt-become-active-focussed-despite-using-nsapp-activate#comment140612492_79710850
    @MainActor private func openWindow(_ destination: URLSchemeDestination) {
        guard let url = URL(string: "\(URLSchemeDestination.scheme)://\(destination.rawValue)") else { return }
        let configuration = NSWorkspace.OpenConfiguration()
        configuration.activates = true
        configuration.allowsRunningApplicationSubstitution = false
        NSWorkspace.shared.open([url], withApplicationAt: Bundle.main.bundleURL, configuration: configuration)
    }

    @MainActor func open(_ destination: URLSchemeDestination) {
        switch destination {
        case .settings:
            settingsWindowController.show()
        case .snippetEditor:
            CPYSnippetsEditorWindowController.sharedController.showWindow(self)
        }
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
