//
//  ClipManager.swift
//  Clipy
//
//  Created by 古林俊佑 on 2016/03/12.
//  Copyright (c) 2016年 Shunsuke Furubayashi. All rights reserved.
//

import Cocoa
import RealmSwift
import PINCache
import RxCocoa
import RxSwift
import RxOptional

final class ClipManager: NSObject {
    // MARK: - Properties
    static let sharedManager = ClipManager()
    // Clip Observer
    fileprivate var storeTypes = [String: NSNumber]()
    fileprivate var cachedChangeCount = 0
    fileprivate var pasteboardObservingTimer: Timer?
    fileprivate let lock = NSRecursiveLock(name: "com.clipy-app.Clipy.ClipUpdatable")
    // Other
    fileprivate let disposeBag = DisposeBag()
    fileprivate let defaults = UserDefaults.standard
    fileprivate let realm = try! Realm()
    fileprivate let pasteboard = NSPasteboard.general()
    // Realm Result
    fileprivate var clipResults: Results<CPYClip>

    // MARK: - Initialize
    override init() {
        clipResults = realm.objects(CPYClip.self).sorted(byProperty: "updateTime", ascending: !defaults.bool(forKey: Constants.UserDefaults.reorderClipsAfterPasting))
        super.init()
        startTimer()
    }

    deinit {
        stopTimer()
    }

    func setup() {
        bind()
    }
}

// MARK: - Clear Clips
extension ClipManager {
    func clearAll() {
        var imagePaths = [String]()

        clipResults.forEach { clip in
            if clip.thumbnailPath.isEmpty { return }
            imagePaths.append(clip.thumbnailPath)
        }

        imagePaths.forEach { PINCache.shared().removeObject(forKey: $0) }
        realm.transaction { realm.delete(clipResults) }
        HistoryManager.sharedManager.cleanDatas()
    }
}

// MARK: - Binding
fileprivate extension ClipManager {
    fileprivate func bind() {
        // Store Type
        defaults.rx.observe([String: NSNumber].self, Constants.UserDefaults.storeTypes)
            .filterNil()
            .subscribe(onNext: { [unowned self] types in
                self.storeTypes = types
            }).addDisposableTo(disposeBag)
        // Observe Interval
        defaults.rx.observe(Float.self, Constants.UserDefaults.timeInterval, options: [.new])
            .filterNil()
            .subscribe(onNext: { [unowned self] _ in
                self.startTimer()
            }).addDisposableTo(disposeBag)
        // Sort clips
        defaults.rx.observe(Bool.self, Constants.UserDefaults.reorderClipsAfterPasting, options: [.new])
            .filterNil()
            .subscribe(onNext: { [unowned self] enabled in
                self.clipResults = self.realm.objects(CPYClip.self).sorted(byProperty: "updateTime", ascending: !enabled)
            }).addDisposableTo(disposeBag)
    }
}

// MARK: - Observe Timer
extension ClipManager {
    func startTimer() {
        stopTimer()

        var timeInterval = defaults.float(forKey: Constants.UserDefaults.timeInterval)
        if timeInterval > 1.0 {
            timeInterval = 1.0
            defaults.set(1.0, forKey: Constants.UserDefaults.timeInterval)
        }

        pasteboardObservingTimer = Timer(timeInterval: TimeInterval(timeInterval),
                                           target: self,
                                           selector: #selector(ClipManager.updateClips),
                                           userInfo: nil,
                                           repeats: true)
        RunLoop.current.add(pasteboardObservingTimer!, forMode: RunLoopMode.commonModes)
    }

    func stopTimer() {
        if let timer = pasteboardObservingTimer, timer.isValid {
            timer.invalidate()
            pasteboardObservingTimer = nil
        }
    }

    func updateClips() {
        lock.lock()
        if pasteboard.changeCount != cachedChangeCount {
            cachedChangeCount = pasteboard.changeCount
            createClip()
        }
        lock.unlock()
    }
}

// MARK: - Create Clips
extension ClipManager {
    fileprivate func createClip() {
        if ExcludeAppManager.sharedManager.frontProcessIsExcludeApplication() { return }

        let types = clipTypes(pasteboard)
        if types.isEmpty { return }
        if !storeTypes.values.contains(NSNumber(value: true)) { return }

        let data = CPYClipData(pasteboard: pasteboard, types: types)
        saveClipData(data)
    }

    func createclip(_ image: NSImage) {
        let data = CPYClipData(image: image)
        saveClipData(data)
    }

    fileprivate func saveClipData(_ data: CPYClipData) {
        let isCopySameHistory = defaults.bool(forKey: Constants.UserDefaults.copySameHistory)
        // Search same history
        if let _ = realm.object(ofType: CPYClip.self, forPrimaryKey: "\(data.hash)"), !isCopySameHistory { return }
        // Dont't save empty stirng object
        if data.isOnlyStringType && data.stringValue.isEmpty { return }

        let isOverwriteHistory = defaults.bool(forKey: Constants.UserDefaults.overwriteSameHistory)
        let hash = (isOverwriteHistory) ? data.hash : Int(arc4random() % 1000000)

        // Save DB
        let unixTime = Int(floor(Date().timeIntervalSince1970))
        let path = (CPYUtilities.applicationSupportFolder() as NSString).appendingPathComponent("\(UUID().uuidString).data")
        let title = data.stringValue

        let clip = CPYClip()
        clip.dataPath = path
        // Trim Save Title
        clip.title = title[0...10000]
        clip.dataHash = "\(hash)"
        clip.updateTime = unixTime
        clip.primaryType = data.primaryType ?? ""

        // Save thumbnail image
        if let image = data.image, data.primaryType == NSTIFFPboardType {
            let thumbnailWidth = defaults.integer(forKey: Constants.UserDefaults.thumbnailWidth)
            let thumbnailHeight = defaults.integer(forKey: Constants.UserDefaults.thumbnailHeight)

            if let thumbnailImage = image.resizeImage(CGFloat(thumbnailWidth), CGFloat(thumbnailHeight)) {
                PINCache.shared().setObject(thumbnailImage, forKey: String(unixTime))
                clip.thumbnailPath = String(unixTime)
            }
        }

        if CPYUtilities.prepareSaveToPath(CPYUtilities.applicationSupportFolder()) {
            if NSKeyedArchiver.archiveRootObject(data, toFile: path) {
                realm.transaction {
                    realm.add(clip, update: true)
                }
            }
        }
    }

    fileprivate func clipTypes(_ pasteboard: NSPasteboard) -> [String] {
        var types = [String]()
        if let pbTypes = pasteboard.types {
            for dataType in pbTypes {
                if !isClipType(dataType) { continue }
                if dataType == NSTIFFPboardType && types.contains(NSTIFFPboardType) { continue }
                types.append(dataType)
            }
        }
        return types
    }

    fileprivate func isClipType(_ type: String) -> Bool {
        let typeDict = CPYClipData.availableTypesDictinary
        if let key = typeDict[type] {
            if let number = storeTypes[key] {
                return number.boolValue
            }
        }
        return false
    }
}
