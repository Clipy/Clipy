//
//  CPYClipManager.swift
//  Clipy
//
//  Created by 古林俊佑 on 2015/06/21.
//  Copyright (c) 2015年 Shunsuke Furubayashi. All rights reserved.
//

import Cocoa

class CPYClipManager: NSObject {

    // MARK: - Properties
    static let sharedManager = CPYClipManager()
    
    private var storeTypes = [String: NSNumber]()
    private var cachedChangeCount: NSInteger = 0
    private let copyLock = NSLock()
    // Timer
    private var pasteboardObservingTimer: NSTimer?
    
    // MARK: - Init
    override init() {
        super.init()
        self.initManager()
    }
    
    private func initManager() {
        let defaults = NSUserDefaults.standardUserDefaults()
        
        self.storeTypes = defaults.objectForKey(kCPYPrefStoreTypesKey) as! [String: NSNumber]

        // Timer
        self.startPasteboardObservingTimer()
        
        defaults.addObserver(self, forKeyPath: kCPYPrefMaxHistorySizeKey, options: NSKeyValueObservingOptions.New, context: nil)
        defaults.addObserver(self, forKeyPath: kCPYPrefTimeIntervalKey, options: NSKeyValueObservingOptions.New, context: nil)
        defaults.addObserver(self, forKeyPath: kCPYPrefStoreTypesKey, options: NSKeyValueObservingOptions.New, context: nil)
    }
    
    deinit {
        let defaults = NSUserDefaults.standardUserDefaults()
        defaults.removeObserver(self, forKeyPath: kCPYPrefMaxHistorySizeKey)
        defaults.removeObserver(self, forKeyPath: kCPYPrefTimeIntervalKey)
        defaults.removeObserver(self, forKeyPath: kCPYPrefStoreTypesKey)
        if self.pasteboardObservingTimer != nil && self.pasteboardObservingTimer!.valid {
            self.pasteboardObservingTimer?.invalidate()
        }
    }
    
    // MARK: - KVO
    override func observeValueForKeyPath(keyPath: String, ofObject object: AnyObject, change: [NSObject : AnyObject], context: UnsafeMutablePointer<Void>) {
        if keyPath == kCPYPrefMaxHistorySizeKey {
            self.trimHistorySize()
        } else if keyPath == kCPYPrefTimeIntervalKey {
            self.startPasteboardObservingTimer()
        } else if keyPath == kCPYPrefStoreTypesKey {
            let defaults = NSUserDefaults.standardUserDefaults()
            self.storeTypes = defaults.objectForKey(kCPYPrefStoreTypesKey) as! [String: NSNumber]
        }
    }
    
    // MARK: - Public Methods
    internal func loadClips() -> RLMResults? {
        return CPYClip.allObjects()
    }
    
    internal func loadSortedClips() -> RLMResults? {
        return CPYClip.allObjects().sortedResultsUsingProperty("updateTime", ascending: false)
    }
    
    internal func clearAll() {
        if let results = self.loadClips() {
            var paths = [String]()
            for clip in results {
                paths.append((clip as! CPYClip).dataPath)
            }
            for path in paths {
                CPYUtilities.deleteData(path)
            }
            let realm = RLMRealm.defaultRealm()
            realm.transactionWithBlock({ () -> Void in
                realm.deleteObjects(results)
            })
            
            NSNotificationCenter.defaultCenter().postNotificationName(kCPYChangeContentsNotification, object: nil)
        }
    }
    
    internal func clipAtIndex(index: NSInteger) -> CPYClip {
        return self.loadSortedClips()!.objectAtIndex(UInt(index)) as! CPYClip
    }
    
    internal func removeClipAtIndex(index: NSInteger) -> Bool {
        let clip = self.loadSortedClips()!.objectAtIndex(UInt(index)) as! CPYClip
        return self.removeClip(clip)
    }
    
    internal func removeClip(clip: CPYClip) -> Bool {
        let path = clip.dataPath
        CPYUtilities.deleteData(path)
        
        let realm = RLMRealm.defaultRealm()
        realm.transactionWithBlock({ () -> Void in
            realm.deleteObject(clip)
        })
        
        NSNotificationCenter.defaultCenter().postNotificationName(kCPYChangeContentsNotification, object: nil)
        
        return true
    }
    
    internal func copyStringToPasteboard(aString: String) {
        let pboard = NSPasteboard.generalPasteboard()
        pboard.declareTypes([NSStringPboardType], owner: self)
        pboard.setString(aString, forType: NSStringPboardType)
    }
    
    internal func copyClipToPasteboard(clip: CPYClip) {
        
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
    
    internal func copyClipToPasteboardAtIndex(index: NSInteger) {
        if let result = self.loadSortedClips() {
            if let clip = result.objectAtIndex(UInt(index)) as? CPYClip {
                self.copyClipToPasteboard(clip)
            }
        }
    }
    
    // MARK: - Clip Methods
    internal func updateClips(sender: NSTimer) {
        
        self.copyLock.lock()
        
        let pasteBoard = NSPasteboard.generalPasteboard()
        if pasteBoard.changeCount == self.cachedChangeCount {
            self.copyLock.unlock()
            return
        }
        self.cachedChangeCount = pasteBoard.changeCount
        
        if let clipData = self.makeClipDataFromPasteboard(pasteBoard) {
            
            let realm = RLMRealm.defaultRealm()
            let hash = clipData.hash
            let clips = self.loadClips()
            
            // DB格納
            let unixTime = Int(floor(NSDate().timeIntervalSince1970))
            let unixTimeString = String("\(unixTime)")
            let path = CPYUtilities.applicationSupportFolder().stringByAppendingPathComponent("\(NSUUID().UUIDString).data")
            let title = clipData.stringValue
            
            let clip = CPYClip()
            clip.dataPath = path
            clip.title = title
            clip.dataHash = String(hash)
            clip.updateTime = unixTime
            clip.primaryType = clipData.primaryType ?? ""
            
            if CPYUtilities.prepareSaveToPath(CPYUtilities.applicationSupportFolder()) {
                let result = NSKeyedArchiver.archiveRootObject(clipData, toFile: path)
                if result {
                    realm.transactionWithBlock({ () -> Void in
                        realm.addOrUpdateObject(clip)
                    })
                }
            }
            
            self.trimHistorySize()
            
            NSNotificationCenter.defaultCenter().postNotificationName(kCPYChangeContentsNotification, object: nil)
        }
        
        self.copyLock.unlock()
    }
    
    // MARK: - Timer Methods
    private func startPasteboardObservingTimer() {
        self.stopPasteboardObservingTimer()
        
        let defaults = NSUserDefaults.standardUserDefaults()
        var timeInterval = defaults.floatForKey(kCPYPrefTimeIntervalKey)
        if timeInterval > 1.0 {
            timeInterval = 1.0
            defaults.setFloat(1.0, forKey: kCPYPrefTimeIntervalKey)
        }
        
        self.pasteboardObservingTimer = NSTimer.scheduledTimerWithTimeInterval(NSTimeInterval(timeInterval), target: self, selector: "updateClips:", userInfo: nil, repeats: true)
    }
    
    internal func stopPasteboardObservingTimer() {
        if self.pasteboardObservingTimer != nil && self.pasteboardObservingTimer!.valid {
            self.pasteboardObservingTimer?.invalidate()
        }
    }
    
    // MARK: Private Methods
    private func makeClipDataFromPasteboard(pboard: NSPasteboard) -> CPYClipData? {
        
        let clipData = CPYClipData()
        
        let types = self.makeTypesFromPasteboard(pboard)
        
        if types.isEmpty {
            return nil
        }
        
        if !contains(self.storeTypes.values, NSNumber(bool: true)) {
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
    
    private func makeTypesFromPasteboard(pboard: NSPasteboard) -> [String] {
        var types = [String]()
        
        let pbTypes = pboard.types
        
        for dataType in pbTypes as! [String] {
            if !self.storeType(dataType) {
                continue
            }
            
            if dataType == NSTIFFPboardType {
                if contains(types, NSTIFFPboardType) {
                    continue
                }
                types.append(NSTIFFPboardType)
            } else {
                types.append(dataType)
            }
        }
        return types
    }
    
    private func storeType(type: String) -> Bool {
        let typeDict = CPYClipData.availableTypesDictinary()
        if let key = typeDict[type] {
            if let number = self.storeTypes[key] {
                return number.boolValue
            }
        }
        return false
    }
    
    private func trimHistorySize() {
        
        let realm = RLMRealm.defaultRealm()
        if let clips = self.loadSortedClips() {
            
            let maxHistorySize = NSUserDefaults.standardUserDefaults().integerForKey(kCPYPrefMaxHistorySizeKey)
            if maxHistorySize < Int(clips.count) {
                
                let lastClip = clips.objectAtIndex(UInt(maxHistorySize - 1)) as! CPYClip
                let lastUsedAt = lastClip.updateTime
                if let results = self.loadClips()?.objectsWithPredicate(NSPredicate(format: "updateTime < %d",lastUsedAt)) {
                    var paths = [String]()
                    for clip in results {
                        paths.append((clip as! CPYClip).dataPath)
                    }
                    for path in paths {
                        CPYUtilities.deleteData(path)
                    }
                    realm.transactionWithBlock({ () -> Void in
                        realm.deleteObjects(results)
                    })
                }
            }
            
        }
    }
    
}
