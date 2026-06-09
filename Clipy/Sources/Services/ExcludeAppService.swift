//
//  ExcludeAppService.swift
//
//  Clipy
//  GitHub: https://github.com/clipy
//  HP: https://clipy-app.com
//
//  Created by Econa77 on 2017/02/10.
//
//  Copyright © 2015-2018 Clipy Project.
//

import AppKit
import Foundation
import RxSwift
import RxCocoa

final class ExcludeAppService {

    // MARK: - Properties
    fileprivate(set) var applications = [CPYAppInfo]()
    fileprivate var frontApplication = BehaviorRelay<NSRunningApplication?>(value: nil)
    fileprivate var disposeBag = DisposeBag()

    // MARK: - Initialize
    init(applications: [CPYAppInfo]) {
        self.applications = applications
    }

}

// MARK: - Monitor Applications
extension ExcludeAppService {
    func startMonitoring() {
        disposeBag = DisposeBag()
        // Monitoring top active application
        NSWorkspace.shared.notificationCenter.rx.notification(NSWorkspace.didActivateApplicationNotification)
            .map { $0.userInfo?["NSWorkspaceApplicationKey"] as? NSRunningApplication }
            .startWith(NSWorkspace.shared.frontmostApplication)
            .bind(to: frontApplication)
            .disposed(by: disposeBag)
    }
}

// MARK: - Exclude
extension ExcludeAppService {
    func frontProcessIsExcludedApplication() -> Bool {
        if applications.isEmpty { return false }
        guard let frontApplicationIdentifier = frontApplication.value?.bundleIdentifier else { return false }

        for app in applications where app.identifier == frontApplicationIdentifier {
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
        AppEnvironment.current.defaults.set(data, forKey: Constants.UserDefaults.excludeApplications)
        AppEnvironment.current.defaults.synchronize()
    }
}

// MARK: - Special Applications
extension ExcludeAppService {
    /// Applications that mark pasteboard data when the frontmost application cannot identify the copy source,
    /// such as menu bar applications that do not take focus.
    private enum Application: String {
        case onePassword = "com.agilebits.onepassword"

        // MARK: - Excluded
        func isExcluded(applications: [CPYAppInfo]) -> Bool {
            return applications.contains { $0.identifier.hasPrefix(rawValue) }
        }

    }

    func copiedProcessIsExcludedApplications(pasteboard: NSPasteboard) -> Bool {
        guard let types = pasteboard.types else { return false }
        guard let application = types.compactMap({ Application(rawValue: $0.rawValue) }).first else { return false }
        return application.isExcluded(applications: applications)
    }
}
