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
import Combine
import Dependencies
import Foundation
import Sharing

final class ExcludeAppService {
    // MARK: - Properties
    fileprivate(set) var applications: [CPYAppInfo] = []
    fileprivate var frontApplication: NSRunningApplication?
    private var cancellables: Set<AnyCancellable> = []
    private let notificationCenter: NotificationCenter

    @Dependency(\.defaultAppStorage)
    private var appStorage

    // MARK: - Initialize
    init(notificationCenter: NotificationCenter = NSWorkspace.shared.notificationCenter) {
        self.notificationCenter = notificationCenter
    }
}

// MARK: - Monitor Applications
extension ExcludeAppService {
    func startMonitoring() {
        cancellables.removeAll()
        // Monitoring top active application
        self.applications = (appStorage.object(forKey: Constants.UserDefaults.excludeApplications) as? Data)
            .flatMap { NSKeyedUnarchiver.unarchiveObject(with: $0) as? [CPYAppInfo] }
            ?? []
        notificationCenter.publisher(for: NSWorkspace.didActivateApplicationNotification)
            .map { $0.userInfo?["NSWorkspaceApplicationKey"] as? NSRunningApplication }
            .prepend(NSWorkspace.shared.frontmostApplication)
            .sink { [weak self] in self?.frontApplication = $0 }
            .store(in: &cancellables)
    }
}

// MARK: - Exclude
extension ExcludeAppService {
    func frontProcessIsExcludedApplication() -> Bool {
        if applications.isEmpty { return false }
        guard let frontApplicationIdentifier = frontApplication?.bundleIdentifier else { return false }

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
        appStorage.set(data, forKey: Constants.UserDefaults.excludeApplications)
        appStorage.synchronize()
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

extension DependencyValues {
    var excludeAppService: ExcludeAppService {
        get { self[ExcludeAppServiceKey.self] }
        set { self[ExcludeAppServiceKey.self] = newValue }
    }

    private enum ExcludeAppServiceKey: DependencyKey {
        static let liveValue = ExcludeAppService()
    }
}
