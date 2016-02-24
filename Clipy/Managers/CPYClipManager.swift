//
//  CPYClipManager.swift
//  Clipy
//
//  Created by 古林俊佑 on 2015/06/21.
//  Copyright (c) 2015年 Shunsuke Furubayashi. All rights reserved.
//

import Cocoa
import Realm
import PINCache

class CPYClipManager: NSObject {

    // MARK: - Properties
    static let sharedManager = CPYClipManager()
    
    private var storeTypes = [String: NSNumber]()
    private var cachedChangeCount: NSInteger = 0
    private var pasteboardObservingTimer: NSTimer?
    private let lock = NSRecursiveLock(name: "com.clipy-app.Clipy.ClipUpdatable")
    private let defaults = NSUserDefaults.standardUserDefaults()
    private let realm = RLMRealm.defaultRealm()
    
    // MARK: - Init
    override init() {
        super.init()
        initManager()
    }
    
    private func initManager() {
        storeTypes = defaults.objectForKey(kCPYPrefStoreTypesKey) as! [String: NSNumber]

        // Timer
        startPasteboardObservingTimer()
        
        defaults.addObserver(self, forKeyPath: kCPYPrefMaxHistorySizeKey, options: NSKeyValueObservingOptions.New, context: nil)
        defaults.addObserver(self, forKeyPath: kCPYPrefTimeIntervalKey, options: NSKeyValueObservingOptions.New, context: nil)
        defaults.addObserver(self, forKeyPath: kCPYPrefStoreTypesKey, options: NSKeyValueObservingOptions.New, context: nil)
    }
    
    deinit {
        defaults.removeObserver(self, forKeyPath: kCPYPrefMaxHistorySizeKey)
        defaults.removeObserver(self, forKeyPath: kCPYPrefTimeIntervalKey)
        defaults.removeObserver(self, forKeyPath: kCPYPrefStoreTypesKey)
        if pasteboardObservingTimer != nil && pasteboardObservingTimer!.valid {
            pasteboardObservingTimer?.invalidate()
        }
    }
    
    // MARK: - KVO
    override func observeValueForKeyPath(keyPath: String?, ofObject object: AnyObject?, change: [String : AnyObject]?, context: UnsafeMutablePointer<Void>) {
        if keyPath == kCPYPrefMaxHistorySizeKey {
            CPYHistoryManager.sharedManager.trimHistorySize()
        } else if keyPath == kCPYPrefTimeIntervalKey {
            startPasteboardObservingTimer()
        } else if keyPath == kCPYPrefStoreTypesKey {
            storeTypes = defaults.objectForKey(kCPYPrefStoreTypesKey) as! [String: NSNumber]
        }
    }
    
    // MARK: - Public Methods
    func loadClips() -> RLMResults {
        return CPYClip.allObjects()
    }
    
    func loadSortedClips() -> RLMResults {
        let ascending = !defaults.boolForKey(kCPYPrefReorderClipsAfterPasting)
        return CPYClip.allObjects().sortedResultsUsingProperty("updateTime", ascending: ascending)
    }
    
    func clearAll() {
        let results = loadClips()
        var imagePaths = [String]()
        
        for clipData in results {
            let clip = clipData as! CPYClip
            if !clip.thumbnailPath.isEmpty {
                imagePaths.append(clip.thumbnailPath)
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

        
        CPYHistoryManager.sharedManager.cleanHistory()
        
        NSNotificationCenter.defaultCenter().postNotificationName(kCPYChangeContentsNotification, object: nil)
    }
    
    func copyStringToPasteboard(aString: String) {
        let pboard = NSPasteboard.generalPasteboard()
        pboard.declareTypes([NSStringPboardType], owner: self)
        pboard.setString(aString, forType: NSStringPboardType)
    }
    
    func copyClipToPasteboard(clip: CPYClip) {
        if let loadData = NSKeyedUnarchiver.unarchiveObjectWithFile(clip.dataPath) as? CPYClipData {
            
            let types = loadData.types
            
            let pboard = NSPasteboard.generalPasteboard()
            pboard.declareTypes(types, owner: self)
            
            for pbType in types {
                if pbType == NSStringPboardType {
                    let pbString = loadData.stringValue
                    pboard.setString(pbString, forType: NSStringPboardType)
                } else if pbType == NSRTFDPboardType {
                    let rtfData = loadData.RTFData
                    pboard.setData(rtfData!, forType: NSRTFDPboardType)
                } else if pbType == NSRTFPboardType {
                    let rtfData = loadData.RTFData
                    pboard.setData(rtfData!, forType: NSRTFPboardType)
                } else if pbType == NSPDFPboardType {
                    let pdfData = loadData.PDF
                    if let pdfRep = NSPDFImageRep(data: pdfData!) {
                        pboard.setData(pdfRep.PDFRepresentation, forType: NSPDFPboardType)
                    }
                } else if pbType == NSFilenamesPboardType {
                    let fileNames = loadData.fileNames
                    pboard.setPropertyList(fileNames, forType: NSFilenamesPboardType)
                } else if pbType == NSURLPboardType {
                    let url = loadData.URLs
                    pboard.setPropertyList(url, forType: NSURLPboardType)
                } else if pbType == NSTIFFPboardType {
                    let image = loadData.image
                    if image != nil {
                        pboard.setData(image!.TIFFRepresentation!, forType: NSTIFFPboardType)
                    }
                }
            }
        }
    }
    
    func copyClipToPasteboardAtIndex(index: NSInteger) {
        let result = loadSortedClips()
        if let clip = result.objectAtIndex(UInt(index)) as? CPYClip where !clip.invalidated {
            copyClipToPasteboard(clip)
        }
    }
    
    // MARK: - Clip Methods
    func updateClips(sender: NSTimer) {
        lock.lock()
        
        let pasteBoard = NSPasteboard.generalPasteboard()
        if pasteBoard.changeCount == cachedChangeCount {
            lock.unlock()
            return
        }
        cachedChangeCount = pasteBoard.changeCount
        createClip()
        
        lock.unlock()
    }
    
    func createClip() {
        autoreleasepool { () -> () in
    
            if let clipData = makeClipDataFromPasteboard() {
                
                let isCopySameHistory = defaults.boolForKey(kCPYPrefCopySameHistroyKey)
                // Search same history
                if let _ = CPYClip(forPrimaryKey: String(clipData.hash)) where !isCopySameHistory { return }
                
                let isOverwriteHistory = defaults.boolForKey(kCPYPrefOverwriteSameHistroyKey)
                let hash: Int
                if isOverwriteHistory {
                    hash = clipData.hash
                } else {
                    hash = Int(arc4random() % 1000000)
                }
                
                // DB格納
                let unixTime = Int(floor(NSDate().timeIntervalSince1970))
                let path = (CPYUtilities.applicationSupportFolder() as NSString).stringByAppendingPathComponent("\(NSUUID().UUIDString).data")
                let title = clipData.stringValue
                
                let clip = CPYClip()
                clip.dataPath = path
                clip.title = title
                clip.dataHash = String(hash)
                clip.updateTime = unixTime
                clip.primaryType = clipData.primaryType ?? ""
                
                // Save thumbnail image
                if clipData.primaryType == NSTIFFPboardType {
                    if let image = clipData.image {
                        
                        let thumbnailWidth = defaults.integerForKey(kCPYPrefThumbnailWidthKey)
                        let thumbnailHeight = defaults.integerForKey(kCPYPrefThumbnailHeightKey)
                        
                        if let thumbnailImage = image.resizeImage(CGFloat(thumbnailWidth), CGFloat(thumbnailHeight)) {
                            PINCache.sharedCache().setObject(thumbnailImage, forKey: String(unixTime))
                            clip.thumbnailPath = String(unixTime)
                        }
                    }
                }
                
                if CPYUtilities.prepareSaveToPath(CPYUtilities.applicationSupportFolder()) {
                    let result = NSKeyedArchiver.archiveRootObject(clipData, toFile: path)
                    if result {
                        do {
                            try realm.transactionWithBlock({ () -> Void in
                                realm.addOrUpdateObject(clip)
                            })
                        } catch {}
                    }
                }
                
                
                CPYHistoryManager.sharedManager.trimHistorySize()
                
                NSNotificationCenter.defaultCenter().postNotificationName(kCPYChangeContentsNotification, object: nil)
            }
            
        }
    }
    
    // MARK: - Timer Methods
    func startPasteboardObservingTimer() {
        stopPasteboardObservingTimer()

        var timeInterval = defaults.floatForKey(kCPYPrefTimeIntervalKey)
        if timeInterval > 1.0 {
            timeInterval = 1.0
            defaults.setFloat(1.0, forKey: kCPYPrefTimeIntervalKey)
        }
 
        pasteboardObservingTimer = NSTimer(timeInterval: NSTimeInterval(timeInterval), target: self, selector: "updateClips:", userInfo: nil, repeats: true)
        NSRunLoop.currentRunLoop().addTimer(pasteboardObservingTimer!, forMode: NSRunLoopCommonModes)
    }
    
    func stopPasteboardObservingTimer() {
        if pasteboardObservingTimer != nil && pasteboardObservingTimer!.valid {
            pasteboardObservingTimer?.invalidate()
        }
    }
    
    // MARK: Private Methods
    private func makeClipDataFromPasteboard() -> CPYClipData? {
        
        let clipData = CPYClipData()
        
        let pboard = NSPasteboard.generalPasteboard()
        let types = makeTypesFromPasteboard()
        
        if types.isEmpty {
            return nil
        }
        
        if !storeTypes.values.contains(NSNumber(bool: true)) {
            return nil
        }
        
        clipData.types = types
        
        for pbType in types {
            if pbType == NSStringPboardType {
                if let pbString = pboard.stringForType(NSStringPboardType) {
                    clipData.stringValue = pbString
                }
            } else if pbType == NSRTFDPboardType {
                let rtfData = pboard.dataForType(NSRTFDPboardType)
                clipData.RTFData = rtfData
            } else if pbType == NSRTFPboardType && clipData.RTFData == nil {
                let rtfData = pboard.dataForType(NSRTFPboardType)
                clipData.RTFData = rtfData
            } else if pbType == NSPDFPboardType {
                let pdfData = pboard.dataForType(NSPDFPboardType)
                clipData.PDF = pdfData
            } else if pbType == NSFilenamesPboardType {
                if let fileNames = pboard.propertyListForType(NSFilenamesPboardType) as? [String] {
                    clipData.fileNames = fileNames
                }
            } else if pbType == NSURLPboardType {
                if let url = pboard.propertyListForType(NSURLPboardType) as? [String] {
                    clipData.URLs = url
                }
            } else if pbType == NSTIFFPboardType {
                if NSImage.canInitWithPasteboard(pboard) {
                    let image = NSImage(pasteboard: pboard)
                    clipData.image = image
                }
            }
        }
        
        return clipData
    }
    
    private func makeTypesFromPasteboard() -> [String] {
        var types = [String]()
        
        let pboard = NSPasteboard.generalPasteboard()
        if let pbTypes = pboard.types {
            for dataType in pbTypes {
                if !storeType(dataType) {
                    continue
                }
                
                if dataType == NSTIFFPboardType {
                    if types.contains(NSTIFFPboardType) {
                        continue
                    }
                    types.append(NSTIFFPboardType)
                } else {
                    types.append(dataType)
                }
            }
        }
        return types
    }
    
    private func storeType(type: String) -> Bool {
        let typeDict = CPYClipData.availableTypesDictinary()
        if let key = typeDict[type] {
            if let number = storeTypes[key] {
                return number.boolValue
            }
        }
        return false
    }
    
}
