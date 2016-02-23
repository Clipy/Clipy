//
//  CPYHistoryManager.swift
//  Clipy
//
//  Created by 古林俊佑 on 2015/09/10.
//  Copyright (c) 2015年 Shunsuke Furubayashi. All rights reserved.
//

import Cocoa

class CPYHistoryManager: NSObject {

    // MARK: - Properties
    static let sharedManager = CPYHistoryManager()
    private var historyManageTimer: NSTimer!
    
    // MARK: - Init
    override init() {
        super.init()
        startHistoryManageTimer()
    }
    
    // MARK: Public Methods
    func trimHistorySize() {
        dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), { () -> Void in
            let realm = RLMRealm.defaultRealm()
            let clips = CPYClip.allObjects().sortedResultsUsingProperty("updateTime", ascending: false)
            
            let maxHistorySize = NSUserDefaults.standardUserDefaults().integerForKey(kCPYPrefMaxHistorySizeKey)
            if maxHistorySize < Int(clips.count) {
                if let lastClip = clips.objectAtIndex(UInt(maxHistorySize - 1)) as? CPYClip where !lastClip.invalidated {
                    
                    let lastUsedAt = lastClip.updateTime
                    let results = CPYClipManager.sharedManager.loadClips().objectsWithPredicate(NSPredicate(format: "updateTime < %d",lastUsedAt))
                    var imagePaths = [String]()
                    for clipData in results {
                        if let clip = clipData as? CPYClip where !clip.invalidated {
                            if !clip.thumbnailPath.isEmpty {
                                imagePaths.append(clip.thumbnailPath)
                            }
                        }
                    }
    
                    for path in imagePaths {
                        PINCache.sharedCache().removeObjectForKey(path)
                    }
                    do {
                        try realm.transactionWithBlock({ () -> Void in
                            realm.deleteObjects(results)
                        })
                    } catch {}
                }
            }
            
        })
    }
    
    func cleanHistory() {
        dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), { () -> Void in
            let allClips = CPYClipManager.sharedManager.loadClips()
            
            let fileManager = NSFileManager.defaultManager()
            do {
                let dataPathList = try fileManager.contentsOfDirectoryAtPath(CPYUtilities.applicationSupportFolder())
                for path in dataPathList {
                    var isExist = false
                    for clipData in allClips {
                        if let clip = clipData as? CPYClip where !clip.invalidated {
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
        })
    }
    
    // MARK: - Private Methods
    private func startHistoryManageTimer() {
        // Clean clip data history every 30 minutes
        historyManageTimer = NSTimer(timeInterval: 60 * 30, target: self, selector: "cleanHistory", userInfo: nil, repeats: true)
        NSRunLoop.currentRunLoop().addTimer(historyManageTimer, forMode: NSRunLoopCommonModes)
        historyManageTimer.fire()
    }
    
}
