//
//  CPYUtilities.swift
//  Clipy
//
//  Created by 古林俊佑 on 2015/06/21.
//  Copyright (c) 2015年 Shunsuke Furubayashi. All rights reserved.
//

import Cocoa

class CPYUtilities: NSObject {

    static func registerUserDefaultKeys() {
        var defaultValues = [String: AnyObject]()
        
        defaultValues.updateValue(CPYHotKeyManager.defaultHotKeyCombos(), forKey: kCPYPrefHotKeysKey)
        /* General */
        defaultValues.updateValue(NSNumber(bool: false), forKey: kCPYPrefLoginItemKey)
        defaultValues.updateValue(NSNumber(bool: false), forKey: kCPYPrefSuppressAlertForLoginItemKey)
        defaultValues.updateValue(NSNumber(integer: 30), forKey: kCPYPrefMaxHistorySizeKey)
        defaultValues.updateValue(NSNumber(integer: 1), forKey: kCPYPrefShowStatusItemKey)
        defaultValues.updateValue(NSNumber(float: 0.75), forKey: kCPYPrefTimeIntervalKey)
        defaultValues.updateValue(AppDelegate.storeTypesDictinary(), forKey: kCPYPrefStoreTypesKey)
        defaultValues.updateValue(NSNumber(bool: true), forKey: kCPYPrefInputPasteCommandKey)
        defaultValues.updateValue(NSNumber(bool: true), forKey: kCPYPrefReorderClipsAfterPasting)
        
        /* Menu */
        defaultValues.updateValue(NSNumber(integer: 16), forKey: kCPYPrefMenuIconSizeKey)
        defaultValues.updateValue(NSNumber(integer: 20), forKey: kCPYPrefMaxMenuItemTitleLengthKey)
        defaultValues.updateValue(NSNumber(integer: 0), forKey: kCPYPrefNumberOfItemsPlaceInlineKey)
        defaultValues.updateValue(NSNumber(integer: 10), forKey: kCPYPrefNumberOfItemsPlaceInsideFolderKey)
        defaultValues.updateValue(NSNumber(bool: false), forKey: kCPYPrefMenuItemsTitleStartWithZeroKey)
        defaultValues.updateValue(NSNumber(bool: true), forKey: kCPYPrefShowAlertBeforeClearHistoryKey)
        defaultValues.updateValue(NSNumber(bool: true), forKey: kCPYPrefAddClearHistoryMenuItemKey)
        defaultValues.updateValue(NSNumber(bool: true), forKey: kCPYPrefShowIconInTheMenuKey)
        defaultValues.updateValue(NSNumber(bool: true), forKey: kCPYPrefMenuItemsAreMarkedWithNumbersKey)
        defaultValues.updateValue(NSNumber(bool: false), forKey: kCPYPrefAddNumericKeyEquivalentsKey)
        defaultValues.updateValue(NSNumber(bool: true), forKey: kCPYPrefShowToolTipOnMenuItemKey)
        defaultValues.updateValue(NSNumber(bool: true), forKey: kCPYPrefShowImageInTheMenuKey)
        defaultValues.updateValue(NSNumber(integer: 200), forKey: kCPYPrefMaxLengthOfToolTipKey)
        defaultValues.updateValue(NSNumber(integer: 100), forKey: kCPYPrefThumbnailWidthKey)
        defaultValues.updateValue(NSNumber(integer: 32), forKey: kCPYPrefThumbnailHeightKey)
        defaultValues.updateValue(NSNumber(bool: true), forKey: kCPYPrefOverwriteSameHistroy)
        
        /* Updates */
        defaultValues.updateValue(NSNumber(bool: true), forKey: kCPYEnableAutomaticCheckKey)
        defaultValues.updateValue(NSNumber(int: 86400), forKey: kCPYUpdateCheckIntervalKey)

        NSUserDefaults.standardUserDefaults().registerDefaults(defaultValues)
        NSUserDefaults.standardUserDefaults().synchronize()
    }
    
    static func paste() -> Bool {
        if !NSUserDefaults.standardUserDefaults().boolForKey(kCPYPrefInputPasteCommandKey) {
            return false
        }
    
        dispatch_async(dispatch_get_main_queue(), { () -> Void in
            
            let keyVDown = CGEventCreateKeyboardEvent(nil, CGKeyCode(9), true)
            CGEventSetFlags(keyVDown, CGEventFlags.MaskCommand)
            CGEventPost(CGEventTapLocation.CGHIDEventTap, keyVDown)
            
            let keyVUp = CGEventCreateKeyboardEvent(nil, CGKeyCode(9), false)
            CGEventSetFlags(keyVUp, CGEventFlags.MaskCommand)
            CGEventPost(CGEventTapLocation.CGHIDEventTap, keyVUp)
        })
        
        return true
    }
    
    static func applicationSupportFolder() -> String {
        let paths = NSSearchPathForDirectoriesInDomains(NSSearchPathDirectory.ApplicationSupportDirectory, NSSearchPathDomainMask.UserDomainMask, true)
        var basePath: String!
        if paths.count > 0 {
            basePath = paths.first
        } else {
            basePath = NSTemporaryDirectory()
        }

        return (basePath as NSString).stringByAppendingPathComponent(kApplicationName)
    }
    
    static func prepareSaveToPath(path: String) -> Bool {
        let fileManager = NSFileManager.defaultManager()
        var isDir: ObjCBool = false
        
        if (fileManager.fileExistsAtPath(path, isDirectory: &isDir) && isDir) == false {
            do {
                try fileManager.createDirectoryAtPath(path, withIntermediateDirectories: true, attributes: nil)
            } catch {
                return false
            }
        }
        return true
    }
    
    static func deleteData(path: String) {
        autoreleasepool { () -> () in
            let fileManager = NSFileManager.defaultManager()
            var isDir: ObjCBool = false
            
            if fileManager.fileExistsAtPath(path, isDirectory: &isDir) {
                do {
                    try fileManager.removeItemAtPath(path)
                } catch { }
            }
        }
    }
}
