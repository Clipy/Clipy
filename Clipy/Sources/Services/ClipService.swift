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
import Combine
import Dependencies
import Foundation
import Sharing
import SQLiteData

final class ClipService {

    // MARK: - Properties
    private var cachedChangeCount = 0
    private let lock = NSRecursiveLock(name: "com.clipy-app.Clipy.ClipUpdatable")
    private var pasteboardMonitoringTask: Task<Void, Never>?
    private var historyTrimmingTask: Task<Void, Never>?
    private var historyCleanupTask: Task<Void, Never>?
    private var cancellables: Set<AnyCancellable> = []

    @Dependency(\.pasteboardHistoryRepository)
    private var pasteboardHistoryRepository
    @Dependency(\.textRecognizer)
    private var textRecognizer
    @Dependency(\.defaultDatabase)
    private var database
    @Dependency(\.mainQueue)
    private var mainQueue

    @Shared(.showsClearHistoryAlert)
    private var showsClearHistoryAlert
    @Shared(.excludedApplications)
    private var excludedApplications
    @Shared(.ignoresConcealedPasteboardTypes)
    private var ignoresConcealedPasteboardTypes
    @Shared(.allowsDuplicateHistory)
    private var allowsDuplicateHistory
    @Shared(.overwritesDuplicateHistory)
    private var overwritesDuplicateHistory
    @Shared(.pasteboardTypeSettings)
    private var pasteboardTypeSettings
    @Shared(.maximumHistoryCount)
    private var maximumHistoryCount
    @Shared(.reordersClipsAfterPasting)
    private var reordersClipsAfterPasting
    @Shared(.clearsHistoryOnQuit)
    private var clearsHistoryOnQuit
    @Shared(.clearsHistoryPeriodically)
    private var clearsHistoryPeriodically
    @Shared(.historyClearInterval)
    private var historyClearInterval

    // MARK: - Clips
    func startMonitoring() {
        pasteboardMonitoringTask = Task { @MainActor [weak self] in
            @Dependency(\.continuousClock) var continuousClock

            for await _ in continuousClock.timer(interval: .milliseconds(500)) {
                let changeCount = NSPasteboard.general.changeCount
                guard changeCount != self?.cachedChangeCount else { continue }
                self?.cachedChangeCount = changeCount
                self?.create()
            }
        }

        historyTrimmingTask = Task(priority: .utility) { @MainActor [weak self] in
            @Dependency(\.continuousClock) var continuousClock

            for await _ in continuousClock.timer(interval: .seconds(60)) {
                guard let self else { return }
                pasteboardHistoryRepository.deleteOverflowingHistories(
                    sortsByCreatedAt: !reordersClipsAfterPasting,
                    maxHistorySize: maximumHistoryCount
                )
            }
        }

        $clearsHistoryPeriodically.changes(includingInitialValue: true)
            .combineLatest($historyClearInterval.changes(includingInitialValue: true))
            .receive(on: mainQueue)
            .sink { [weak self] isEnabled, interval in
                self?.scheduleHistoryCleanup(interval, isEnabled: isEnabled)
            }
            .store(in: &cancellables)
    }

    func applicationWillTerminate() {
        guard clearsHistoryOnQuit else { return }
        pasteboardHistoryRepository.deleteAll()
    }

    @objc func clearAllHistory() {
        if showsClearHistoryAlert {
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
                $showsClearHistoryAlert.withLock { $0 = false }
            }
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
        pasteboardMonitoringTask?.cancel()
        historyTrimmingTask?.cancel()
        historyCleanupTask?.cancel()
    }
}

// MARK: - Automatic History Cleanup
extension ClipService {
    private func scheduleHistoryCleanup(_ interval: HistoryClearInterval, isEnabled: Bool) {
        historyCleanupTask?.cancel()
        historyCleanupTask = nil
        guard isEnabled else { return }

        historyCleanupTask = Task { @MainActor [weak self] in
            @Dependency(\.continuousClock) var continuousClock

            await withErrorReporting { [weak self] in
                let duration = Duration.seconds(interval.rawValue)
                let clock = AnyClock(continuousClock)
                var nextDeadline = clock.now.advanced(by: duration)

                while !Task.isCancelled {
                    try await clock.sleep(until: nextDeadline, tolerance: nil)
                    guard !Task.isCancelled, let self, clearsHistoryPeriodically, historyClearInterval == interval else { return }
                    pasteboardHistoryRepository.deleteAll()
                    nextDeadline = clock.now.advanced(by: duration)
                }
            }
        }
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
        let pasteboardTypes = pasteboard.types ?? pasteboard.pasteboardItems?.flatMap(\.types) ?? []
        let types = PasteboardAvailableType.availableTypes(
            from: pasteboardTypes,
            storeAvailableTypes: pasteboardTypeSettings.enabledTypes,
            ignoresConcealedType: ignoresConcealedPasteboardTypes
        )
        guard !types.isEmpty else { return }

        guard !isExcludedApplication(
            frontmostApplicationIdentifier: NSWorkspace.shared.frontmostApplication?.bundleIdentifier,
            pasteboardTypes: pasteboardTypes
        ) else { return }

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
        let historyID = PasteboardHistory.ID(rawValue: content.hash)
        if pasteboardHistoryRepository.fetchHistory(id: historyID) != nil, !allowsDuplicateHistory { return }

        // Don't save empty or whitespace-only text history
        if content.isBlankText { return }

        // Overwrite same history
        let savedHash = overwritesDuplicateHistory ? content.hash : UUID().uuidString

        let unixTime = Int(Date().timeIntervalSince1970)
        let id = PasteboardHistory.ID(rawValue: savedHash)
        pasteboardHistoryRepository.save(id: id, content: content, updateAt: unixTime)
        textRecognizer.recognizeTextIfNeeded(id: id)
    }
}

// MARK: - Excluded Applications
extension ClipService {
    private func isExcludedApplication(
        frontmostApplicationIdentifier: String?,
        pasteboardTypes: [NSPasteboard.PasteboardType]
    ) -> Bool {
        excludedApplications.contains { application in
            if application.identifier == frontmostApplicationIdentifier {
                return true
            }

            // Extensions and menu bar apps can mark copied data without taking focus.
            // The marker can be a prefix of the installed application's bundle identifier.
            return pasteboardTypes.contains { application.identifier.hasPrefix($0.rawValue) }
        }
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
