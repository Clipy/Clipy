//
//  HistoryManager.swift
//  Clipy
//
//  Created by 古林俊佑 on 2016/03/12.
//  Copyright (c) 2016年 Shunsuke Furubayashi. All rights reserved.
//

import Cocoa
import RealmSwift
import PINCache

final class HistoryManager: NSObject {
    // MARK: - Properties
    static let sharedManager = HistoryManager()
    // Timer
    private var cleanHistotyTimer: NSTimer?
    // Other
    private let realm = try! Realm()
    private let defaults = NSUserDefaults.standardUserDefaults()

    // MARK: - Initialize
    deinit {
        stopTimer()
    }

    func setup() {
        startTimer()
    }
}

// MARK: - Clean
extension HistoryManager {
    func cleanHistory() {
        trimHistory()
        cleanDatas()
    }

    func cleanDatas() {
        let allClips = realm.objects(CPYClip.self)
        let fileManager = NSFileManager.defaultManager()
        do {
            let dataPathList = try fileManager.contentsOfDirectoryAtPath(CPYUtilities.applicationSupportFolder())
            for path in dataPathList {
                var isExist = false
                for clip in allClips {
                    if !clip.invalidated {
                        if let clipPath = clip.dataPath.componentsSeparatedByString("/").last where clipPath == path {
                            isExist = true
                            break
                        }
                    }
                }
                if !isExist {
                    CPYUtilities.deleteData((CPYUtilities.applicationSupportFolder() as NSString).stringByAppendingPathComponent(path))
                }
            }
        } catch { }
    }

    private func trimHistory() {
        let clips = realm.objects(CPYClip.self).sorted("updateTime", ascending: false)
        let maxHistorySize = defaults.integerForKey(Constants.UserDefaults.maxHistorySize)

        if maxHistorySize < Int(clips.count) {
            let lastClip = clips[maxHistorySize - 1]
            if !lastClip.invalidated {

                let lastUsedAt = lastClip.updateTime
                let results = realm.objects(CPYClip.self).filter("updateTime < %d", lastUsedAt)
                var imagePaths = [String]()
                for clip in results {
                    if !clip.invalidated && !clip.thumbnailPath.isEmpty {
                        imagePaths.append(clip.thumbnailPath)
                    }
                }

                imagePaths.forEach { PINCache.sharedCache().removeObjectForKey($0) }
                realm.transaction {
                    realm.delete(results)
                }
            }
        }

    }
}

// MARK: - Timer
private extension HistoryManager {
    private func startTimer() {
        stopTimer()
        // Clean clip data history every 30 minutes
        cleanHistotyTimer = NSTimer(timeInterval: 60 * 30,
                                    target: self,
                                    selector: #selector(HistoryManager.cleanHistory),
                                    userInfo: nil,
                                    repeats: true)
        NSRunLoop.currentRunLoop().addTimer(cleanHistotyTimer!, forMode: NSRunLoopCommonModes)
    }

    private func stopTimer() {
        if let timer = cleanHistotyTimer where timer.valid {
            cleanHistotyTimer?.invalidate()
            cleanHistotyTimer = nil
        }
    }
}
