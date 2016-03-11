//
//  HistoryManager.swift
//  Clipy
//
//  Created by 古林俊佑 on 2016/03/12.
//  Copyright (c) 2016年 Shunsuke Furubayashi. All rights reserved.
//

import Cocoa
import Realm
import PINCache

final class HistoryManager: NSObject {
    // MARK: - Properties
    static let sharedManager = HistoryManager()
    // Timer
    private var cleanHistotyTimer: NSTimer?
    // Other
    private let realm = RLMRealm.defaultRealm()
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
        let allClips = CPYClip.allObjects()
        let fileManager = NSFileManager.defaultManager()
        do {
            let dataPathList = try fileManager.contentsOfDirectoryAtPath(CPYUtilities.applicationSupportFolder())
            for path in dataPathList {
                var isExist = false
                for clipObject in allClips {
                    if let clip = clipObject as? CPYClip where !clip.invalidated {
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
        let clips = CPYClip.allObjects().sortedResultsUsingProperty("updateTime", ascending: false)
        let maxHistorySize = defaults.integerForKey(kCPYPrefMaxHistorySizeKey)
        
        if maxHistorySize < Int(clips.count) {
            if let lastClip = clips.objectAtIndex(UInt(maxHistorySize - 1)) as? CPYClip where !lastClip.invalidated {
                
                let lastUsedAt = lastClip.updateTime
                let results = CPYClip.allObjects().objectsWithPredicate(NSPredicate(format: "updateTime < %d",lastUsedAt))
                var imagePaths = [String]()
                for clipObject in results {
                    if let clip = clipObject as? CPYClip where !clip.invalidated {
                        if !clip.thumbnailPath.isEmpty {
                            imagePaths.append(clip.thumbnailPath)
                        }
                    }
                }
                
                for path in imagePaths {
                    PINCache.sharedCache().removeObjectForKey(path)
                }
                realm.transaction {
                    realm.deleteObjects(results)
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
        cleanHistotyTimer = NSTimer(timeInterval: 60 * 30, target: self, selector: "cleanHistory", userInfo: nil, repeats: true)
        NSRunLoop.currentRunLoop().addTimer(cleanHistotyTimer!, forMode: NSRunLoopCommonModes)
    }
    
    private func stopTimer() {
        if let timer = cleanHistotyTimer where timer.valid {
            cleanHistotyTimer?.invalidate()
            cleanHistotyTimer = nil
        }
    }
}
