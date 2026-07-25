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
import RxCocoa
import RxSwift

final class ClipService {

    // MARK: - Properties
    fileprivate var cachedChangeCount = 0
    fileprivate var storeTypes = [String: NSNumber]()
    fileprivate let lock = NSRecursiveLock(name: "com.clipy-app.Clipy.ClipUpdatable")
    fileprivate var monitoringTask: Task<Void, Never>?
    fileprivate var disposeBag = DisposeBag()

    @Dependency(\.excludeAppService)
    private var excludeAppService
    @Dependency(\.pasteboardHistoryRepository)
    private var pasteboardHistoryRepository
    @Dependency(\.textRecognizer)
    private var textRecognizer
    @Dependency(\.defaultAppStorage)
    private var appStorage

    // MARK: - Clips
    func startMonitoring() {
        monitoringTask?.cancel()
        disposeBag = DisposeBag()
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
        // Store types
        storeTypes = appStorage.object(forKey: Constants.UserDefaults.storeTypes) as? [String: NSNumber] ?? [:]
        appStorage.rx
            .observe([String: NSNumber].self, Constants.UserDefaults.storeTypes)
            .compactMap { $0 }
            .asDriver(onErrorDriveWith: .empty())
            .drive(onNext: { [weak self] in
                self?.storeTypes = $0
            })
            .disposed(by: disposeBag)
    }

    func clearAll() {
        pasteboardHistoryRepository.deleteAll()
        // Clear legacy Realm-backed history caches used through v1.2.1.
        try? FileManager.default.removeItem(atPath: CPYUtilities.applicationSupportFolder())
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
            storeAvailableTypes: storeTypes.filter { $0.value.boolValue }.compactMap { PasteboardAvailableType(rawValue: $0.key) },
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
