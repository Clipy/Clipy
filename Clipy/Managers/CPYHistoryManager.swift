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
        self.startHistoryManageTimer()
    }
    
    // MARK: Public Methods
    internal func trimHistorySize() {
        dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), { () -> Void in
            let realm = RLMRealm.defaultRealm()
            let clips = CPYClipManager.sharedManager.loadSortedClips()
            
            let maxHistorySize = NSUserDefaults.standardUserDefaults().integerForKey(kCPYPrefMaxHistorySizeKey)
            if maxHistorySize < Int(clips.count) {
                if let lastClip = clips.objectAtIndex(UInt(maxHistorySize - 1)) as? CPYClip where !lastClip.invalidated {
                    
                    let lastUsedAt = lastClip.updateTime
                    let results = CPYClipManager.sharedManager.loadClips().objectsWithPredicate(NSPredicate(format: "updateTime < %d",lastUsedAt))
                    var paths = [String]()
                    var imagePaths = [String]()
                    for clipData in results {
                        if let clip = clipData as? CPYClip where !clip.invalidated {
                            paths.append(clip.dataPath)
                            if !clip.thumbnailPath.isEmpty {
                                imagePaths.append(clip.thumbnailPath)
                            }
                        }
                    }
                    
                    for path in paths {
                        CPYUtilities.deleteData(path)
                    }
                    for path in imagePaths {
                        PINCache.sharedCache().removeObjectForKey(path)
                    }
                    realm.transactionWithBlock({ () -> Void in
                        realm.deleteObjects(results)
                    })
                }
            }
        })
    }
    
    internal func cleanHistory() {
        dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), { () -> Void in
            let allClips = CPYClipManager.sharedManager.loadClips()
            
            let fileManager = NSFileManager.defaultManager()
            var error: NSError?
            if let dataPathList = fileManager.contentsOfDirectoryAtPath(CPYUtilities.applicationSupportFolder(), error: &error) as? [String] {
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
                        CPYUtilities.deleteData(CPYUtilities.applicationSupportFolder().stringByAppendingPathComponent(path))
                    }
                }
            }
        })

    }
    
    // MARK: - Private Methods
    private func startHistoryManageTimer() {
        // Clean clip data history every 12 hour
        self.historyManageTimer = NSTimer(timeInterval: 60 * 60 * 12, target: self, selector: "cleanHistory", userInfo: nil, repeats: true)
        NSRunLoop.currentRunLoop().addTimer(self.historyManageTimer, forMode: NSRunLoopCommonModes)
        self.historyManageTimer.fire()
    }
    
}
