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
    @Shared(.excludedApplications)
    private(set) var applications

    private var frontApplication: NSRunningApplication?
    private var cancellables: Set<AnyCancellable> = []
    private let notificationCenter: NotificationCenter

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
    func add(with applicationInformation: ApplicationInformation) {
        if applications.contains(applicationInformation) { return }
        $applications.withLock { $0.append(applicationInformation) }
    }

    func delete(with applicationInformation: ApplicationInformation) {
        $applications.withLock { $0.removeAll { $0 == applicationInformation } }
    }

    func delete(with index: Int) {
        delete(with: applications[index])
    }
}

// MARK: - Special Applications
extension ExcludeAppService {
    /// Applications that mark pasteboard data when the frontmost application cannot identify the copy source,
    /// such as menu bar applications that do not take focus.
    private enum Application: String {
        case onePassword = "com.agilebits.onepassword"

        // MARK: - Excluded
        func isExcluded(applications: [ApplicationInformation]) -> Bool {
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
