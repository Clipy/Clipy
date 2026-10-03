//
//  SnippetSyncService.swift
//
//  Clipy
//  GitHub: https://github.com/clipy
//  HP: https://clipy-app.com
//
//  Created by Shunsuke Furubayashi on 2026/09/28.
//
//  Copyright © 2015-2026 Clipy Project.
//

import AppKit
import Combine
import Dependencies
import Foundation
import Sharing

/// Coordinates the opt-in "sync snippets through a folder" feature: watches the local database
/// and the shared sync file for changes, merges them through `SnippetSyncMerger`, and applies the
/// result to both sides. Disabled by default; only snippets and snippet folders are affected,
/// never clipboard history.
final class SnippetSyncService {
    @Dependency(\.snippetRepository)
    private var snippetRepository
    @Dependency(\.mainQueue)
    private var mainQueue

    private let fileStore: SnippetSyncFileStore

    private var filePresenter: SnippetSyncFilePresenter?
    private var configurationCancellable: AnyCancellable?
    private var localObservationCancellable: AnyCancellable?
    private var syncTriggerCancellable: AnyCancellable?
    private var activationCancellable: AnyCancellable?
    private var retryCancellable: AnyCancellable?
    private var retryAttempt = 0
    private let syncTrigger = PassthroughSubject<Void, Never>()

    private var currentFolderURL: URL?
    private var previousLocalFolders: [SnippetFolder]?
    private var previousLocalSnippets: [Snippet]?

    @Shared(.isSnippetSyncEnabled)
    private var isSnippetSyncEnabled
    @Shared(.snippetSyncFolderPath)
    private var snippetSyncFolderPath

    init(fileStore: SnippetSyncFileStore = SnippetSyncFileStore()) {
        self.fileStore = fileStore
    }

    func synchronizeForTesting(at folderURL: URL) {
        if currentFolderURL != folderURL {
            cancelRetry()
            currentFolderURL = folderURL
        }
        syncNow()
    }

    func disableForTesting() {
        reconfigure(isEnabled: false, folderPath: nil)
    }

    func start() {
        syncTriggerCancellable = syncTrigger
            .debounce(for: .seconds(1), scheduler: mainQueue)
            .sink { [weak self] in
                self?.syncNow()
            }

        activationCancellable = NotificationCenter.default.publisher(for: NSApplication.didBecomeActiveNotification)
            .receive(on: mainQueue)
            .sink { [weak self] _ in
                self?.syncTrigger.send(())
            }

        configurationCancellable = Publishers.CombineLatest(
            $isSnippetSyncEnabled.changes(includingInitialValue: true),
            $snippetSyncFolderPath.changes(includingInitialValue: true)
        )
        .receive(on: mainQueue)
        .sink { [weak self] isEnabled, folderPath in
            self?.reconfigure(isEnabled: isEnabled, folderPath: folderPath)
        }
    }

    deinit {
        filePresenter?.stop()
    }
}

private extension SnippetSyncService {
    func reconfigure(isEnabled: Bool, folderPath: String?) {
        // Reconfiguration invalidates a pending retry, even if the old folder is still downloading.
        cancelRetry()
        stopWatching()

        guard isEnabled, let folderPath, !folderPath.isEmpty else {
            currentFolderURL = nil
            return
        }

        let folderURL = URL(fileURLWithPath: folderPath, isDirectory: true)
        currentFolderURL = folderURL
        // Reset the baseline: we don't know yet whether the last state we wrote for this folder
        // (if any) still reflects reality, so the next merge treats this as a first enable and
        // only adds records rather than risk inventing deletions.
        previousLocalFolders = nil
        previousLocalSnippets = nil

        localObservationCancellable = snippetRepository.observeFolderDetails()
            .dropFirst()
            .receive(on: mainQueue)
            .sink { [weak self] _ in
                self?.syncTrigger.send(())
            }

        filePresenter = SnippetSyncFilePresenter(url: syncFileURL(in: folderURL)) { [weak self] in
            self?.syncTrigger.send(())
        }

        syncTrigger.send(())
    }

    func stopWatching() {
        localObservationCancellable = nil
        filePresenter?.stop()
        filePresenter = nil
    }

    func syncNow() {
        guard let folderURL = currentFolderURL else { return }
        synchronize(at: folderURL)
    }

    func cancelRetry() {
        retryCancellable = nil
        retryAttempt = 0
    }

    func scheduleRetry() {
        // A file presenter notification is not guaranteed when a cloud download completes.
        // Keep one cancellable retry outstanding, with a capped delay while the folder stays active.
        guard retryCancellable == nil else { return }
        let delays = [2, 5, 15, 30]
        let delay = delays[min(retryAttempt, delays.count - 1)]
        retryAttempt += 1
        retryCancellable = Just(())
            .delay(for: .seconds(delay), scheduler: mainQueue)
            .sink { [weak self] in
                self?.retryCancellable = nil
                self?.syncNow()
            }
    }

    func synchronize(at folderURL: URL) {
        let url = syncFileURL(in: folderURL)

        let details = snippetRepository.fetchFolderDetails()
        let localFolders = details.map(\.folder)
        let localSnippets = details.flatMap(\.snippets)

        let remoteRead = fileStore.read(at: url)
        let plan = SnippetSyncMerger.plan(
            localFolders: localFolders,
            localSnippets: localSnippets,
            previousLocal: SnippetSyncBaseline(folders: previousLocalFolders, snippets: previousLocalSnippets),
            remote: remoteRead,
            now: Int(Date().timeIntervalSince1970 * 1_000)
        )

        guard let fileToWrite = plan.fileToWrite else {
            // Keep the baseline and local data untouched until the shared file can be read.
            scheduleRetry()
            return
        }

        if plan.hasLocalChanges {
            snippetRepository.applySyncChanges(
                folderUpserts: plan.folderUpserts,
                folderDeletions: plan.folderDeletions,
                snippetUpserts: plan.snippetUpserts,
                snippetDeletions: plan.snippetDeletions
            )
        }

        let needsWrite: Bool
        switch remoteRead {
        case .notFound:
            needsWrite = true
        case let .success(remoteFile):
            needsWrite = remoteFile != fileToWrite
        case .unreadable:
            return
        }
        if needsWrite && !fileStore.write(fileToWrite, to: url) {
            // Don't advance the baseline when a coordinated write fails: the next observation
            // must retry instead of treating an unwritten change as synchronized.
            return
        }

        previousLocalFolders = fileToWrite.folders.map(SnippetFolder.init)
        previousLocalSnippets = fileToWrite.snippets.map(Snippet.init)
        cancelRetry()
    }

    func syncFileURL(in folderURL: URL) -> URL {
        folderURL.appendingPathComponent(SnippetSyncFileStore.fileName)
    }
}

extension DependencyValues {
    var snippetSyncService: SnippetSyncService {
        get { self[SnippetSyncServiceKey.self] }
        set { self[SnippetSyncServiceKey.self] = newValue }
    }

    private enum SnippetSyncServiceKey: DependencyKey {
        static let liveValue = SnippetSyncService()
    }
}
