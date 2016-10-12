//
//  ExcludeAppManager.swift
//  Clipy
//
//  Created by 古林俊佑 on 2016/08/09.
//  Copyright © 2016年 Shunsuke Furubayashi. All rights reserved.
//

import Foundation
import Cocoa

final class ExcludeAppManager {

    // MARK: - Properties
    static let sharedManager = ExcludeAppManager()
    let defaults = NSUserDefaults.standardUserDefaults()
    var applications = [CPYAppInfo]()

    // MARK: - Initialize
    init() {
        guard let data = defaults.objectForKey(Constants.UserDefaults.excludeApplications) as? NSData else { return }
        guard let applications = NSKeyedUnarchiver.unarchiveObjectWithData(data) as? [CPYAppInfo] else { return }
        self.applications = applications
    }

}

// MARK: - Exclude
extension ExcludeAppManager {
    func frontProcessIsExcludeApplication() -> Bool {
        // No Application
        if applications.isEmpty { return false }

        // Front Process Application
        guard let application = NSWorkspace.sharedWorkspace().frontmostApplication else { return false }
        guard let identifier = application.bundleIdentifier else { return false }

        for app in applications where app.identifier == identifier {
            return true
        }
        return false
    }
}

// MARK: - Add or Delete
extension ExcludeAppManager {
    func addExcludeApp(appInfo: CPYAppInfo) {
        if let _ = applications.filter({ $0 == appInfo }).first { return }
        applications.append(appInfo)
        saveApplications()
    }

    func deleteExcludeApp(appInfo: CPYAppInfo) {
        applications = applications.filter { $0 != appInfo }
        saveApplications()
    }

    func deleteExcludeApp(index: Int) {
        deleteExcludeApp(applications[index])
    }

    private func saveApplications() {
        let data = NSKeyedArchiver.archivedDataWithRootObject(applications)
        defaults.setObject(data, forKey: Constants.UserDefaults.excludeApplications)
        defaults.synchronize()
    }
}
