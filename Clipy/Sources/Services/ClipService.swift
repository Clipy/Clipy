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

final class ClipService {

    // MARK: - Properties
    static let shared = ClipService()

    fileprivate var cachedChangeCount = Variable<Int>(0)
    fileprivate var storeTypes = [String: NSNumber]()
    fileprivate let defaults = UserDefaults.standard
    fileprivate let scheduler = SerialDispatchQueueScheduler(qos: .userInteractive)
    fileprivate let lock = NSRecursiveLock(name: "com.clipy-app.Clipy.ClipUpdatable")
    fileprivate let disposeBag = DisposeBag()

    // MARK: - Initialize
    init() {
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
        defaults.rx.observe([String: NSNumber].self, Constants.UserDefaults.storeTypes)
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
        DataCleanService.shared.cleanDatas()
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
        if ExcludeAppService.shared.frontProcessIsExcludedApplication() { return }
        // Special applications
        if ExcludeAppService.shared.copiedProcessIsExcludedApplications(pasteboard: pasteboard) { return }

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
        let isCopySameHistory = defaults.bool(forKey: Constants.UserDefaults.copySameHistory)
        if realm.object(ofType: CPYClip.self, forPrimaryKey: "\(data.hash)") != nil, !isCopySameHistory { return }

        // Don't save empty string history
        if data.isOnlyStringType && data.stringValue.isEmpty { return }

        // Overwrite same history
        let isOverwriteHistory = defaults.bool(forKey: Constants.UserDefaults.overwriteSameHistory)
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
