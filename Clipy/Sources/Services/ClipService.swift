//
//  ClipService.swift
//  Clipy
//
//  Created by 古林俊佑 on 2016/11/17.
//  Copyright © 2016年 Shunsuke Furubayashi. All rights reserved.
//

import Foundation
import Cocoa
import RealmSwift
import PINCache
import RxSwift
import RxOptional
import AEXML

final class ClipService {

    // MARK: - Properties
    fileprivate var cachedChangeCount = Variable<Int>(0)
    fileprivate var storeTypes = [String: NSNumber]()
    fileprivate let scheduler = SerialDispatchQueueScheduler(qos: .userInteractive)
    fileprivate let lock = NSRecursiveLock(name: "com.clipy-app.Clipy.ClipUpdatable")
    fileprivate var disposeBag = DisposeBag()

    // MARK: - Clips
    func startMonitoring() {
        disposeBag = DisposeBag()
        // Pasteboard observe timer
        Observable<Int>.interval(0.75, scheduler: scheduler)
            .map { _ in NSPasteboard.general().changeCount }
            .withLatestFrom(cachedChangeCount.asObservable()) { ($0, $1) }
            .filter { $0 != $1 }
            .subscribe(onNext: { [weak self] (changeCount, _) in
                self?.cachedChangeCount.value = changeCount
                self?.create()
            })
            .disposed(by: disposeBag)
        // Store types
        AppEnvironment.current.defaults.rx
            .observe([String: NSNumber].self, Constants.UserDefaults.storeTypes)
            .filterNil()
            .asDriver(onErrorJustReturn: [:])
            .drive(onNext: { [weak self] in
                self?.storeTypes = $0
            })
            .disposed(by: disposeBag)
    }

    func clearAll() {
        let realm = try! Realm()
        let clips = realm.objects(CPYClip.self)

        // Delete saved images
        clips
            .filter { !$0.thumbnailPath.isEmpty }
            .map { $0.thumbnailPath }
            .forEach { PINCache.shared().removeObject(forKey: $0) }
        // Delete Realm
        realm.transaction { realm.delete(clips) }
        // Delete writed datas
        AppEnvironment.current.dataCleanService.cleanDatas()
    }

}

// MARK: - Create Clip
extension ClipService {
    fileprivate func create() {
        lock.lock(); defer { lock.unlock() }

        // Store types
        if !storeTypes.values.contains(NSNumber(value: true)) { return }
        // Pasteboard types
        let pasteboard = NSPasteboard.general()
        let types = self.types(with: pasteboard)
        if types.isEmpty { return }

        // Excluded application
        guard !AppEnvironment.current.excludeAppService.frontProcessIsExcludedApplication() else { return }
        // Special applications
        guard !AppEnvironment.current.excludeAppService.copiedProcessIsExcludedApplications(pasteboard: pasteboard) else { return }

        // Create data
        let data = CPYClipData(pasteboard: pasteboard, types: types)
        save(with: data)
    }

    func create(with image: NSImage) {
        lock.lock(); defer { lock.unlock() }

        // Create only image data
        let data = CPYClipData(image: image)
        save(with: data)
    }

    fileprivate func save(with data: CPYClipData) {
        let realm = try! Realm()
        // Copy already copied history
        let isCopySameHistory = AppEnvironment.current.defaults.bool(forKey: Constants.UserDefaults.copySameHistory)
        if realm.object(ofType: CPYClip.self, forPrimaryKey: "\(data.hash)") != nil, !isCopySameHistory { return }

        // Don't save empty string history
        if data.isOnlyStringType && data.stringValue.isEmpty { return }

        // Overwrite same history
        let isOverwriteHistory = AppEnvironment.current.defaults.bool(forKey: Constants.UserDefaults.overwriteSameHistory)
        let savedHash = (isOverwriteHistory) ? data.hash : Int(arc4random() % 1000000)

        // Saved time and path
        let unixTime = Int(Date().timeIntervalSince1970)
        let savedPath = CPYUtilities.applicationSupportFolder() + "/\(NSUUID().uuidString).data"
        // Create Realm object
        let clip = CPYClip()
        clip.dataPath = savedPath
        clip.title = data.stringValue[0...10000]
        clip.dataHash = "\(savedHash)"
        clip.updateTime = unixTime
        clip.primaryType = data.primaryType ?? ""

        DispatchQueue.main.async {
            // Save thumbnail image
            if let thumbnailImage = data.thumbnailImage {
                PINCache.shared().setObject(thumbnailImage, forKey: "\(unixTime)")
                clip.thumbnailPath = "\(unixTime)"
            }
            if let colorCodeImage = data.colorCodeImage {
                PINCache.shared().setObject(colorCodeImage, forKey: "\(unixTime)")
                clip.thumbnailPath = "\(unixTime)"
                clip.isColorCode = true
            }
            // Save Realm and .data file
            let dispatchRealm = try! Realm()
            if CPYUtilities.prepareSaveToPath(CPYUtilities.applicationSupportFolder()) {
                if NSKeyedArchiver.archiveRootObject(data, toFile: savedPath) {
                    dispatchRealm.transaction {
                        dispatchRealm.add(clip, update: true)
                    }
                }
            }
        }
    }

    private func types(with pasteboard: NSPasteboard) -> [String] {
        let types = pasteboard.types?.filter { canSave(with: $0) } ?? []
        return NSOrderedSet(array: types).array as? [String] ?? []
    }

    private func canSave(with type: String) -> Bool {
        let dictionary = CPYClipData.availableTypesDictinary
        guard let value = dictionary[type] else { return false }
        guard let number = storeTypes[value] else { return false }
        return number.boolValue
    }
}

// MARK: - Import / Expor
extension ClipService {
    func exportClipboard() -> AEXMLDocument {
        let realm = try! Realm()
        let defaults = UserDefaults.standard
        let maxHistory = defaults.integer(forKey: Constants.UserDefaults.maxHistorySize)
        let ascending = !defaults.bool(forKey: Constants.UserDefaults.reorderClipsAfterPasting)
        let clips = realm.objects(CPYClip.self).sorted(byKeyPath: #keyPath(CPYClip.updateTime), ascending: ascending)

        let xmlDocument = AEXMLDocument()
        let rootElement = xmlDocument.addChild(name: Constants.HistoryXml.rootElement)

        for (index, clip) in clips.enumerated() {
            if maxHistory <= index { break }

            guard !clip.isInvalidated else { continue }
            guard let clipData = NSKeyedUnarchiver.unarchiveObject(withFile: clip.dataPath) as? CPYClipData else { continue }
            guard !clipData.stringValue.isEmpty else { continue }

            let historyElement = rootElement.addChild(name: Constants.HistoryXml.historyElement)
            historyElement.addChild(name: Constants.HistoryXml.contentElement, value: clipData.stringValue)
        }

        return xmlDocument
    }

    func importClipboard(with xml: AEXMLDocument) {
        xml[Constants.HistoryXml.rootElement]
            .children
            .forEach { historyElement in
                if let content = historyElement[Constants.HistoryXml.contentElement].value {
                    let data = CPYClipData(string: content)
                    save(with: data)
                }
            }
    }
}
