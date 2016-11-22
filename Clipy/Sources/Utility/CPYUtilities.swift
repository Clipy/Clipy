//
//  CPYUtilities.swift
//  Clipy
//
//  Created by 古林俊佑 on 2015/06/21.
//  Copyright (c) 2015年 Shunsuke Furubayashi. All rights reserved.
//

import Cocoa
import RealmSwift
import Fabric
import Crashlytics

final class CPYUtilities {

    static func initSDKs() {
        // Fabric
        UserDefaults.standard.register(defaults: ["NSApplicationCrashOnExceptions": true])
        if UserDefaults.standard.bool(forKey: Constants.UserDefaults.collectCrashReport) {
            Fabric.with([Answers.self, Crashlytics.self])
            CPYUtilities.sendCustomLog(with: "applicationDidFinishLaunching")
        }
    }

    static func registerUserDefaultKeys() {
        var defaultValues = [String: Any]()

        defaultValues.updateValue(HotKeyService.defaultKeyCombos, forKey: Constants.UserDefaults.hotKeys)
        /* General */
        defaultValues.updateValue(NSNumber(value: false), forKey: Constants.UserDefaults.loginItem)
        defaultValues.updateValue(NSNumber(value: false), forKey: Constants.UserDefaults.suppressAlertForLoginItem)
        defaultValues.updateValue(NSNumber(value: 30), forKey: Constants.UserDefaults.maxHistorySize)
        defaultValues.updateValue(NSNumber(value: 1), forKey: Constants.UserDefaults.showStatusItem)
        defaultValues.updateValue(AppDelegate.storeTypesDictinary(), forKey: Constants.UserDefaults.storeTypes)
        defaultValues.updateValue(NSNumber(value: true), forKey: Constants.UserDefaults.inputPasteCommand)
        defaultValues.updateValue(NSNumber(value: true), forKey: Constants.UserDefaults.reorderClipsAfterPasting)
        defaultValues.updateValue(NSNumber(value: true), forKey: Constants.UserDefaults.collectCrashReport)

        /* Menu */
        defaultValues.updateValue(NSNumber(value: 16), forKey: Constants.UserDefaults.menuIconSize)
        defaultValues.updateValue(NSNumber(value: 20), forKey: Constants.UserDefaults.maxMenuItemTitleLength)
        defaultValues.updateValue(NSNumber(value: 0), forKey: Constants.UserDefaults.numberOfItemsPlaceInline)
        defaultValues.updateValue(NSNumber(value: 10), forKey: Constants.UserDefaults.numberOfItemsPlaceInsideFolder)
        defaultValues.updateValue(NSNumber(value: false), forKey: Constants.UserDefaults.menuItemsTitleStartWithZero)
        defaultValues.updateValue(NSNumber(value: true), forKey: Constants.UserDefaults.showAlertBeforeClearHistory)
        defaultValues.updateValue(NSNumber(value: true), forKey: Constants.UserDefaults.addClearHistoryMenuItem)
        defaultValues.updateValue(NSNumber(value: true), forKey: Constants.UserDefaults.showIconInTheMenu)
        defaultValues.updateValue(NSNumber(value: true), forKey: Constants.UserDefaults.menuItemsAreMarkedWithNumbers)
        defaultValues.updateValue(NSNumber(value: false), forKey: Constants.UserDefaults.addNumericKeyEquivalents)
        defaultValues.updateValue(NSNumber(value: true), forKey: Constants.UserDefaults.showToolTipOnMenuItem)
        defaultValues.updateValue(NSNumber(value: true), forKey: Constants.UserDefaults.showImageInTheMenu)
        defaultValues.updateValue(NSNumber(value: 200), forKey: Constants.UserDefaults.maxLengthOfToolTip)
        defaultValues.updateValue(NSNumber(value: 100), forKey: Constants.UserDefaults.thumbnailWidth)
        defaultValues.updateValue(NSNumber(value: 32), forKey: Constants.UserDefaults.thumbnailHeight)
        defaultValues.updateValue(NSNumber(value: true), forKey: Constants.UserDefaults.overwriteSameHistory)
        defaultValues.updateValue(NSNumber(value: true), forKey: Constants.UserDefaults.copySameHistory)
        defaultValues.updateValue(NSNumber(value: true), forKey: Constants.UserDefaults.showColorPreviewInTheMenu)

        /* Updates */
        defaultValues.updateValue(NSNumber(value: true), forKey: Constants.Update.enableAutomaticCheck)
        defaultValues.updateValue(NSNumber(value: 86400), forKey: Constants.Update.checkInterval)

        /* Beta */
        defaultValues.updateValue(NSNumber(value: true), forKey: Constants.Beta.pastePlainText)
        defaultValues.updateValue(NSNumber(value: 0), forKey: Constants.Beta.pastePlainTextModifier)
        defaultValues.updateValue(NSNumber(value: false), forKey: Constants.Beta.observerScreenshot)

        UserDefaults.standard.register(defaults: defaultValues)
        UserDefaults.standard.synchronize()
    }

    static func applicationSupportFolder() -> String {
        let paths = NSSearchPathForDirectoriesInDomains(.applicationSupportDirectory, .userDomainMask, true)
        var basePath: String!
        if paths.count > 0 {
            basePath = paths.first
        } else {
            basePath = NSTemporaryDirectory()
        }

        return (basePath as NSString).appendingPathComponent(Constants.Application.name)
    }

    static func prepareSaveToPath(_ path: String) -> Bool {
        let fileManager = FileManager.default
        var isDir: ObjCBool = false

        if (fileManager.fileExists(atPath: path, isDirectory: &isDir) && isDir.boolValue) == false {
            do {
                try fileManager.createDirectory(atPath: path, withIntermediateDirectories: true, attributes: nil)
            } catch {
                return false
            }
        }
        return true
    }

    static func deleteData(at path: String) {
        autoreleasepool {
            let fileManager = FileManager.default
            if fileManager.fileExists(atPath: path) {
                try? fileManager.removeItem(atPath: path)
            }
        }
    }

    static func sendCustomLog(with name: String) {
        if UserDefaults.standard.bool(forKey: Constants.UserDefaults.collectCrashReport) {
            Answers.logCustomEvent(withName: name, customAttributes: nil)
        }
    }
}
