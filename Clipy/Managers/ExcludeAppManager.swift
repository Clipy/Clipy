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
    let defaults = UserDefaults.standard
    var applications = [CPYAppInfo]()

    // MARK: - Initialize
    init() {
        guard let data = defaults.object(forKey: Constants.UserDefaults.excludeApplications) as? Data else { return }
        guard let applications = NSKeyedUnarchiver.unarchiveObject(with: data) as? [CPYAppInfo] else { return }
        self.applications = applications
    }

}

// MARK: - Exclude
extension ExcludeAppManager {
    func frontProcessIsExcludeApplication() -> Bool {
        // No Application
        if applications.isEmpty { return false }

        // Front Process Application
        guard let application = NSWorkspace.shared().frontmostApplication else { return false }
        guard let identifier = application.bundleIdentifier else { return false }

        for app in applications where app.identifier == identifier {
            return true
        }
        return false
    }
}

// MARK: - Add or Delete
extension ExcludeAppManager {
    func addExcludeApp(_ appInfo: CPYAppInfo) {
        if let _ = applications.filter({ $0 == appInfo }).first { return }
        applications.append(appInfo)
        saveApplications()
    }

    func deleteExcludeApp(_ appInfo: CPYAppInfo) {
        applications = applications.filter { $0 != appInfo }
        saveApplications()
    }

    func deleteExcludeApp(_ index: Int) {
        deleteExcludeApp(applications[index])
    }

    fileprivate func saveApplications() {
        let data = NSKeyedArchiver.archivedData(withRootObject: applications)
        defaults.set(data, forKey: Constants.UserDefaults.excludeApplications)
        defaults.synchronize()
    }
}
