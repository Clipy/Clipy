//
//  ClipService.swift
//
//  Clipy
//  GitHub: https://github.com/clipy
//  HP: https://clipy-app.com
//
//  Created by Econa77 on 2016/11/17.
//
//  Copyright © 2015-2018 Clipy Project.
//

import Cocoa
import Clocks
import Dependencies
import Foundation
import Sharing
import SQLiteData

final class ClipService {

    // MARK: - Properties
    fileprivate var cachedChangeCount = 0
    fileprivate let lock = NSRecursiveLock(name: "com.clipy-app.Clipy.ClipUpdatable")
    fileprivate var monitoringTask: Task<Void, Never>?

    @Dependency(\.excludeAppService)
    private var excludeAppService
    @Dependency(\.pasteboardHistoryRepository)
    private var pasteboardHistoryRepository
    @Dependency(\.textRecognizer)
    private var textRecognizer
    @Dependency(\.defaultAppStorage)
    private var appStorage
    @Dependency(\.defaultDatabase)
    private var database

    @Shared(.pasteboardTypeSettings)
    private var pasteboardTypeSettings

    // MARK: - Clips
    func startMonitoring() {
        monitoringTask?.cancel()
        // Pasteboard observe timer
        monitoringTask = Task { @MainActor [weak self] in
            @Dependency(\.continuousClock) var continuousClock

            for await _ in continuousClock.timer(interval: .milliseconds(500)) {
                let changeCount = NSPasteboard.general.changeCount
                guard changeCount != self?.cachedChangeCount else { continue }
                self?.cachedChangeCount = changeCount
                self?.create()
            }
        }
    }

    @objc func clearAllHistory() {
        let isShowAlert = appStorage.bool(forKey: Constants.UserDefaults.showAlertBeforeClearHistory)
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
                appStorage.set(false, forKey: Constants.UserDefaults.showAlertBeforeClearHistory)
            }
            appStorage.synchronize()
        }

        pasteboardHistoryRepository.deleteAll()
        Task(priority: .utility) { [database] in
            await withErrorReporting {
                try await database.vacuum()
            }
        }
        // Clear legacy Realm-backed history caches used through v1.2.1.
        try? FileManager.default.removeLegacyHistoryCacheDirectory()
    }

    func delete(id: PasteboardHistory.ID) {
        pasteboardHistoryRepository.deleteHistory(id: id)
    }

    func incrementChangeCount() {
        cachedChangeCount += 1
    }

    deinit {
        monitoringTask?.cancel()
    }
}

// MARK: - Create Clip
extension ClipService {
    fileprivate func create() {
        lock.lock(); defer { lock.unlock() }

        let pasteboard = NSPasteboard.general
        // Prefer the root pasteboard types because they are comprehensive and can include root-only
        // fallback types such as .deprecatedFilenames and .tiff. Fall back to item types when needed,
        // then let PasteboardAvailableType filter the storeable types.
        let types = PasteboardAvailableType.availableTypes(
            from: pasteboard.types ?? pasteboard.pasteboardItems?.flatMap(\.types) ?? [],
            storeAvailableTypes: pasteboardTypeSettings.enabledTypes,
            ignoresConcealedType: appStorage.bool(forKey: Constants.UserDefaults.ignoreConcealedPasteboardType)
        )
        guard !types.isEmpty else { return }

        // Excluded application
        guard !excludeAppService.frontProcessIsExcludedApplication() else { return }
        // Special applications
        guard !excludeAppService.copiedProcessIsExcludedApplications(pasteboard: pasteboard) else { return }

        guard let content = PasteboardContent(pasteboard: pasteboard, types: types) else { return }
        save(content)
    }

    func create(with image: NSImage) {
        lock.lock(); defer { lock.unlock() }

        guard let content = PasteboardContent(image: image) else { return }
        save(content)
    }

    private func save(_ content: PasteboardContent) {
        // Copy already copied history
        let isCopySameHistory = appStorage.bool(forKey: Constants.UserDefaults.copySameHistory)
        let historyID = PasteboardHistory.ID(rawValue: content.hash)
        if pasteboardHistoryRepository.fetchHistory(id: historyID) != nil, !isCopySameHistory { return }

        // Don't save empty or whitespace-only text history
        if content.isBlankText { return }

        // Overwrite same history
        let isOverwriteHistory = appStorage.bool(forKey: Constants.UserDefaults.overwriteSameHistory)
        let savedHash = (isOverwriteHistory) ? content.hash : UUID().uuidString

        let unixTime = Int(Date().timeIntervalSince1970)
        let id = PasteboardHistory.ID(rawValue: savedHash)
        pasteboardHistoryRepository.save(id: id, content: content, updateAt: unixTime)
        textRecognizer.recognizeTextIfNeeded(id: id)
    }
}

extension DependencyValues {
    var clipService: ClipService {
        get { self[ClipServiceKey.self] }
        set { self[ClipServiceKey.self] = newValue }
    }

    private enum ClipServiceKey: DependencyKey {
        static let liveValue = ClipService()
    }
}
