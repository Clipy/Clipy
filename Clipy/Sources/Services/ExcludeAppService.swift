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
    /**
     *  Responding to applications that have special handling for protection of passwords etc.
     *  e.g 1Password on GoogleChrome browser extension
     *
     *  ref: http://nspasteboard.org/
     */
    private enum Application: String {
        case onePassword = "com.agilebits.onepassword"

        // MARK: - Properties
        private var macApplicationIdentifiers: [String] {
            switch self {
            case .onePassword:
                return ["com.agilebits.onepassword-osx", // for 1Password 6
                        "com.agilebits.onepassword7"] // for 1Password 7
            }
        }

        // MARK: - Excluded
        func isExcluded(applications: [CPYAppInfo]) -> Bool {
            return !applications.filter { macApplicationIdentifiers.contains($0.identifier) }.isEmpty
        }

    }

    func copiedProcessIsExcludedApplications(pasteboard: NSPasteboard) -> Bool {
        guard let types = pasteboard.types else { return false }
        guard let application = types.compactMap({ Application(rawValue: $0.rawValue) }).first else { return false }
        return application.isExcluded(applications: applications)
    }
}
