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
    fileprivate var cleanHistotyTimer: Timer?
    // Other
    fileprivate let realm = try! Realm()
    fileprivate let defaults = UserDefaults.standard

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
        let fileManager = FileManager.default
        do {
            let dataPathList = try fileManager.contentsOfDirectory(atPath: CPYUtilities.applicationSupportFolder())
            for path in dataPathList {
                var isExist = false
                for clip in allClips {
                    if !clip.isInvalidated {
                        if let clipPath = clip.dataPath.components(separatedBy: "/").last, clipPath == path {
                            isExist = true
                            break
                        }
                    }
                }
                if !isExist {
                    CPYUtilities.deleteData((CPYUtilities.applicationSupportFolder() as NSString).appendingPathComponent(path))
                }
            }
        } catch { }
    }

    fileprivate func trimHistory() {
        let clips = realm.objects(CPYClip.self).sorted(byProperty: "updateTime", ascending: false)
        let maxHistorySize = defaults.integer(forKey: Constants.UserDefaults.maxHistorySize)

        if maxHistorySize < Int(clips.count) {
            let lastClip = clips[maxHistorySize - 1]
            if !lastClip.isInvalidated {

                let lastUsedAt = lastClip.updateTime
                let results = realm.objects(CPYClip.self).filter("updateTime < %d", lastUsedAt)
                var imagePaths = [String]()
                for clip in results {
                    if !clip.isInvalidated && !clip.thumbnailPath.isEmpty {
                        imagePaths.append(clip.thumbnailPath)
                    }
                }

                imagePaths.forEach { PINCache.shared().removeObject(forKey: $0) }
                realm.transaction {
                    realm.delete(results)
                }
            }
        }

    }
}

// MARK: - Timer
private extension HistoryManager {
    func startTimer() {
        stopTimer()
        // Clean clip data history every 30 minutes
        cleanHistotyTimer = Timer(timeInterval: 60 * 30,
                                    target: self,
                                    selector: #selector(HistoryManager.cleanHistory),
                                    userInfo: nil,
                                    repeats: true)
        RunLoop.current.add(cleanHistotyTimer!, forMode: RunLoopMode.commonModes)
    }

    func stopTimer() {
        if let timer = cleanHistotyTimer, timer.isValid {
            cleanHistotyTimer?.invalidate()
            cleanHistotyTimer = nil
        }
    }
}
