//
//  CPYHistoryManager.swift
//  Clipy
//
//  Created by 古林俊佑 on 2015/09/10.
//  Copyright (c) 2015年 Shunsuke Furubayashi. All rights reserved.
//

import Cocoa
import RealmSwift

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
            let realm = try! Realm()
            let clips = realm.objects(CPYClip).sorted("updateTime", ascending: false)
            
            let maxHistorySize = NSUserDefaults.standardUserDefaults().integerForKey(kCPYPrefMaxHistorySizeKey)
            if maxHistorySize < clips.count {
                let lastClip = clips[maxHistorySize - 1]
                if !lastClip.invalidated {
                    
                    let lastUsedAt = lastClip.updateTime
                    let results = CPYClipManager.sharedManager.loadClips().filter(NSPredicate(format: "updateTime < %d",lastUsedAt))
                    var imagePaths = [String]()
                    for clip in results {
                        if !clip.invalidated && !clip.thumbnailPath.isEmpty{
                            imagePaths.append(clip.thumbnailPath)
                        }
                    }
    
                    for path in imagePaths {
                        PINCache.sharedCache().removeObjectForKey(path)
                    }
                    do {
                        try realm.write {
                            realm.delete(results)
                        }
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
