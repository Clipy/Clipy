//
//  ExcludeAppService.swift
//  Clipy
//
//  Created by 古林俊佑 on 2017/02/10.
//  Copyright © 2017年 Shunsuke Furubayashi. All rights reserved.
//

import Foundation
import RxSwift

final class ExcludeAppService {

    // MARK: - Properties
    static let shared = ExcludeAppService()

    fileprivate(set) var applications = [CPYAppInfo]()
    fileprivate var frontApplication = Variable<NSRunningApplication?>(nil)
    fileprivate let defaults = UserDefaults.standard
    fileprivate var notificationDisposeBag = DisposeBag()

    // MARK: - Initialize
    init() {
        guard let data = defaults.object(forKey: Constants.UserDefaults.excludeApplications) as? Data else { return }
        guard let applications = NSKeyedUnarchiver.unarchiveObject(with: data) as? [CPYAppInfo] else { return }
        self.applications = applications
    }

}

// MARK: - Monitor Applications
extension ExcludeAppService {
    func startAppMonitoring() {
        notificationDisposeBag = DisposeBag()
        // Monitoring top active application
        NSWorkspace.shared().notificationCenter.rx.notification(.NSWorkspaceDidActivateApplication)
            .map { $0.userInfo?["NSWorkspaceApplicationKey"] as? NSRunningApplication }
            .bindTo(frontApplication)
            .addDisposableTo(notificationDisposeBag)
    }
}

// MARK: - Exclude
extension ExcludeAppService {
    func frontProcessIsExcludedApplication() -> Bool {
        if applications.isEmpty { return false }
        guard let frontApplication = frontApplication.value else { return false }

        for app in applications where app.identifier == frontApplication.bundleIdentifier {
            return true
        }
        return false
    }
}

// MARK: - Add or Delete
extension ExcludeAppService {
    func add(with appInfo: CPYAppInfo) {
        if applications.contains(appInfo) { return }
        applications.append(appInfo)
        save()
    }

    func delete(with appInfo: CPYAppInfo) {
        applications = applications.filter { $0 != appInfo }
        save()
    }

    func delete(with index: Int) {
        delete(with: applications[index])
    }

    private func save() {
        let data = applications.archive()
        defaults.set(data, forKey: Constants.UserDefaults.excludeApplications)
        defaults.synchronize()
    }
}
