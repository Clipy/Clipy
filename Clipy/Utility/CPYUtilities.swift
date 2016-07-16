//
//  CPYUtilities.swift
//  Clipy
//
//  Created by 古林俊佑 on 2015/06/21.
//  Copyright (c) 2015年 Shunsuke Furubayashi. All rights reserved.
//

import Cocoa
import Realm
import Fabric
import Crashlytics

final class CPYUtilities {

    static func initSDKs() {
        // Fabric
        NSUserDefaults.standardUserDefaults().registerDefaults(["NSApplicationCrashOnExceptions": true])
        Fabric.with([Answers.self, Crashlytics.self])
        Answers.logCustomEventWithName("applicationDidFinishLaunching", customAttributes: nil)
    }

    static func registerUserDefaultKeys() {
        var defaultValues = [String: AnyObject]()

        defaultValues.updateValue(HotKeyManager.defaultHotKeyCombos(), forKey: Constants.UserDefaults.hotKeys)
        /* General */
        defaultValues.updateValue(NSNumber(bool: false), forKey: Constants.UserDefaults.loginItem)
        defaultValues.updateValue(NSNumber(bool: false), forKey: Constants.UserDefaults.suppressAlertForLoginItem)
        defaultValues.updateValue(NSNumber(integer: 30), forKey: Constants.UserDefaults.maxHistorySize)
        defaultValues.updateValue(NSNumber(integer: 1), forKey: Constants.UserDefaults.showStatusItem)
        defaultValues.updateValue(NSNumber(float: 0.75), forKey: Constants.UserDefaults.timeInterval)
        defaultValues.updateValue(AppDelegate.storeTypesDictinary(), forKey: Constants.UserDefaults.storeTypes)
        defaultValues.updateValue(NSNumber(bool: true), forKey: Constants.UserDefaults.inputPasteCommand)
        defaultValues.updateValue(NSNumber(bool: true), forKey: Constants.UserDefaults.reorderClipsAfterPasting)

        /* Menu */
        defaultValues.updateValue(NSNumber(integer: 16), forKey: Constants.UserDefaults.menuIconSize)
        defaultValues.updateValue(NSNumber(integer: 20), forKey: Constants.UserDefaults.maxMenuItemTitleLength)
        defaultValues.updateValue(NSNumber(integer: 0), forKey: Constants.UserDefaults.numberOfItemsPlaceInline)
        defaultValues.updateValue(NSNumber(integer: 10), forKey: Constants.UserDefaults.numberOfItemsPlaceInsideFolder)
        defaultValues.updateValue(NSNumber(bool: false), forKey: Constants.UserDefaults.menuItemsTitleStartWithZero)
        defaultValues.updateValue(NSNumber(bool: true), forKey: Constants.UserDefaults.showAlertBeforeClearHistory)
        defaultValues.updateValue(NSNumber(bool: true), forKey: Constants.UserDefaults.addClearHistoryMenuItem)
        defaultValues.updateValue(NSNumber(bool: true), forKey: Constants.UserDefaults.showIconInTheMenu)
        defaultValues.updateValue(NSNumber(bool: true), forKey: Constants.UserDefaults.menuItemsAreMarkedWithNumbers)
        defaultValues.updateValue(NSNumber(bool: false), forKey: Constants.UserDefaults.addNumericKeyEquivalents)
        defaultValues.updateValue(NSNumber(bool: true), forKey: Constants.UserDefaults.showToolTipOnMenuItem)
        defaultValues.updateValue(NSNumber(bool: true), forKey: Constants.UserDefaults.showImageInTheMenu)
        defaultValues.updateValue(NSNumber(integer: 200), forKey: Constants.UserDefaults.maxLengthOfToolTip)
        defaultValues.updateValue(NSNumber(integer: 100), forKey: Constants.UserDefaults.thumbnailWidth)
        defaultValues.updateValue(NSNumber(integer: 32), forKey: Constants.UserDefaults.thumbnailHeight)
        defaultValues.updateValue(NSNumber(bool: true), forKey: Constants.UserDefaults.overwriteSameHistory)
        defaultValues.updateValue(NSNumber(bool: true), forKey: Constants.UserDefaults.copySameHistory)

        /* Updates */
        defaultValues.updateValue(NSNumber(bool: true), forKey: Constants.Update.enableAutomaticCheck)
        defaultValues.updateValue(NSNumber(int: 86400), forKey: Constants.Update.checkInterval)

        /* Beta */
        defaultValues.updateValue(NSNumber(bool: true), forKey: Constants.Beta.pastePlainText)
        defaultValues.updateValue(NSNumber(integer: 0), forKey: Constants.Beta.pastePlainTextModifier)
        defaultValues.updateValue(NSNumber(bool: false), forKey: Constants.Beta.observerScreenshot)

        NSUserDefaults.standardUserDefaults().registerDefaults(defaultValues)
        NSUserDefaults.standardUserDefaults().synchronize()
    }

    static func migrationRealm() {
        let config = RLMRealmConfiguration.defaultConfiguration()
        config.schemaVersion = 3
        config.migrationBlock = { (migrate, oldSchemaVersion) in
            if oldSchemaVersion <= 2 {
                // Add identifier in CPYSnippet
                migrate.enumerateObjects(CPYSnippet.className()) { (_, newObject) in
                    newObject!["identifier"] = NSUUID().UUIDString
                }
            }
        }
        RLMRealmConfiguration.setDefaultConfiguration(config)
        RLMRealm.defaultRealm()
    }

    static func applicationSupportFolder() -> String {
        let paths = NSSearchPathForDirectoriesInDomains(.ApplicationSupportDirectory, .UserDomainMask, true)
        var basePath: String!
        if paths.count > 0 {
            basePath = paths.first
        } else {
            basePath = NSTemporaryDirectory()
        }

        return (basePath as NSString).stringByAppendingPathComponent(Constants.Application.name)
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
