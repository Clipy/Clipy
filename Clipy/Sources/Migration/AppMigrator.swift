//
//  AppMigrator.swift
//
//  Clipy
//  GitHub: https://github.com/clipy
//  HP: https://clipy-app.com
//
//  Created by Shunsuke Furubayashi on 2026/08/01.
//
//  Copyright © 2015-2026 Clipy Project.
//

import Dependencies
import Sharing

final class AppMigrator {
    // MARK: - Properties
    private let migrations: [any AppMigration]

    @Dependency(\.defaultAppStorage)
    private var appStorage

    // MARK: - Initialize
    init(migrations: [any AppMigration] = []) {
        self.migrations = migrations
    }

    // MARK: - Migration
    func run() {
        for migration in migrations {
            guard !appStorage.bool(forKey: migration.id.rawValue) else { continue }
            do {
                try migration.run()
                appStorage.set(true, forKey: migration.id.rawValue)
            } catch {
                reportIssue(error, "App migration failed: \(migration.id.rawValue)")
            }
        }
    }
}
