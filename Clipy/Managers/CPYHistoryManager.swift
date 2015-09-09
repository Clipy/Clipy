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
    
    // MARK: Public Methods
    internal func trimHistorySize() {
        dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), { () -> Void in
            let realm = RLMRealm.defaultRealm()
            let clips = CPYClipManager.sharedManager.loadSortedClips()
            
            let maxHistorySize = NSUserDefaults.standardUserDefaults().integerForKey(kCPYPrefMaxHistorySizeKey)
            if maxHistorySize < Int(clips.count) {
                if let lastClip = clips.objectAtIndex(UInt(maxHistorySize - 1)) as? CPYClip where !lastClip.invalidated {
                    
                    let lastUsedAt = lastClip.updateTime
                    if let results = CPYClipManager.sharedManager.loadClips().objectsWithPredicate(NSPredicate(format: "updateTime < %d",lastUsedAt)) {
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
            }
        })
    }
    
    internal func cleanHistory() {
        
    }
    
}
