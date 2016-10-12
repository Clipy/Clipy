//
//  ClipManager.swift
//  Clipy
//
//  Created by 古林俊佑 on 2016/03/12.
//  Copyright (c) 2016年 Shunsuke Furubayashi. All rights reserved.
//

import Cocoa
import Realm
import PINCache
import RxCocoa
import RxSwift
import NSObject_Rx
import RxOptional

final class ClipManager: NSObject {
    // MARK: - Properties
    static let sharedManager = ClipManager()
    // Clip Observer
    private var storeTypes = [String: NSNumber]()
    private var cachedChangeCount = 0
    private var pasteboardObservingTimer: NSTimer?
    private let lock = NSRecursiveLock(name: "com.clipy-app.Clipy.ClipUpdatable")
    // Other
    private let defaults = NSUserDefaults.standardUserDefaults()
    private let realm = RLMRealm.defaultRealm()
    private let pasteboard = NSPasteboard.generalPasteboard()
    // Realm Result
    private var clipResults: RLMResults

    // MARK: - Initialize
    override init() {
        clipResults = CPYClip.allObjects().sortedResultsUsingProperty("updateTime", ascending: !defaults.boolForKey(Constants.UserDefaults.reorderClipsAfterPasting))
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

        for clipObject in clipResults {
            if let clip = clipObject as? CPYClip where !clip.thumbnailPath.isEmpty {
                imagePaths.append(clip.thumbnailPath)
            }
        }

        imagePaths.forEach { PINCache.sharedCache().removeObjectForKey($0) }
        realm.transaction { realm.deleteObjects(clipResults) }
        HistoryManager.sharedManager.cleanDatas()
    }
}

// MARK: - Binding
private extension ClipManager {
    private func bind() {
        // Store Type
        defaults.rx_observe([String: NSNumber].self, Constants.UserDefaults.storeTypes)
            .filterNil()
            .subscribeNext { [unowned self] types in
                self.storeTypes = types
            }.addDisposableTo(rx_disposeBag)
        // Observe Interval
        defaults.rx_observe(Float.self, Constants.UserDefaults.timeInterval, options: [.New])
            .filterNil()
            .subscribeNext { [unowned self] _ in
                self.startTimer()
            }.addDisposableTo(rx_disposeBag)
        // Sort clips
        defaults.rx_observe(Bool.self, Constants.UserDefaults.reorderClipsAfterPasting, options: [.New])
            .filterNil()
            .subscribeNext { [unowned self] enabled in
                self.clipResults = CPYClip.allObjects().sortedResultsUsingProperty("updateTime", ascending: !enabled)
            }.addDisposableTo(rx_disposeBag)
    }
}

// MARK: - Observe Timer
extension ClipManager {
    func startTimer() {
        stopTimer()

        var timeInterval = defaults.floatForKey(Constants.UserDefaults.timeInterval)
        if timeInterval > 1.0 {
            timeInterval = 1.0
            defaults.setFloat(1.0, forKey: Constants.UserDefaults.timeInterval)
        }

        pasteboardObservingTimer = NSTimer(timeInterval: NSTimeInterval(timeInterval),
                                           target: self,
                                           selector: #selector(ClipManager.updateClips),
                                           userInfo: nil,
                                           repeats: true)
        NSRunLoop.currentRunLoop().addTimer(pasteboardObservingTimer!, forMode: NSRunLoopCommonModes)
    }

    func stopTimer() {
        if let timer = pasteboardObservingTimer where timer.valid {
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
    private func createClip() {
        if ExcludeAppManager.sharedManager.frontProcessIsExcludeApplication() { return }

        let types = clipTypes(pasteboard)
        if types.isEmpty { return }
        if !storeTypes.values.contains(NSNumber(bool: true)) { return }

        let data = CPYClipData(pasteboard: pasteboard, types: types)
        saveClipData(data)
    }

    func createclip(image: NSImage) {
        let data = CPYClipData(image: image)
        saveClipData(data)
    }

    private func saveClipData(data: CPYClipData) {
        let isCopySameHistory = defaults.boolForKey(Constants.UserDefaults.copySameHistory)
        // Search same history
        if let _ = CPYClip(forPrimaryKey: "\(data.hash)") where !isCopySameHistory { return }
        // Dont't save empty stirng object
        if data.isOnlyStringType && data.stringValue.isEmpty { return }

        let isOverwriteHistory = defaults.boolForKey(Constants.UserDefaults.overwriteSameHistory)
        let hash = (isOverwriteHistory) ? data.hash : Int(arc4random() % 1000000)

        // Save DB
        let unixTime = Int(floor(NSDate().timeIntervalSince1970))
        let path = (CPYUtilities.applicationSupportFolder() as NSString).stringByAppendingPathComponent("\(NSUUID().UUIDString).data")
        let title = data.stringValue

        let clip = CPYClip()
        clip.dataPath = path
        // Trim Save Title
        clip.title = title[0...10000]
        clip.dataHash = "\(hash)"
        clip.updateTime = unixTime
        clip.primaryType = data.primaryType ?? ""

        // Save thumbnail image
        if let image = data.image where data.primaryType == NSTIFFPboardType {
            let thumbnailWidth = defaults.integerForKey(Constants.UserDefaults.thumbnailWidth)
            let thumbnailHeight = defaults.integerForKey(Constants.UserDefaults.thumbnailHeight)

            if let thumbnailImage = image.resizeImage(CGFloat(thumbnailWidth), CGFloat(thumbnailHeight)) {
                PINCache.sharedCache().setObject(thumbnailImage, forKey: String(unixTime))
                clip.thumbnailPath = String(unixTime)
            }
        }

        if CPYUtilities.prepareSaveToPath(CPYUtilities.applicationSupportFolder()) {
            if NSKeyedArchiver.archiveRootObject(data, toFile: path) {
                realm.transaction {
                    realm.addOrUpdateObject(clip)
                }
            }
        }
    }

    private func clipTypes(pasteboard: NSPasteboard) -> [String] {
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

    private func isClipType(type: String) -> Bool {
        let typeDict = CPYClipData.availableTypesDictinary
        if let key = typeDict[type] {
            if let number = storeTypes[key] {
                return number.boolValue
            }
        }
        return false
    }
}
