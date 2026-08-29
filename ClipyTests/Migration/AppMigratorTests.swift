//
//  AppMigratorTests.swift
//
//  ClipyTests
//  GitHub: https://github.com/clipy
//  HP: https://clipy-app.com
//
//  Created by Shunsuke Furubayashi on 2026/08/01.
//
//  Copyright © 2015-2026 Clipy Project.
//

import Dependencies
import DependenciesTestSupport
import Testing
@testable import Clipy

@Suite(.dependencies)
struct AppMigratorTests {
    @Dependency(\.defaultAppStorage)
    private var appStorage

    @Test
    func runsMigrationOnlyOnce() {
        var runCount = 0
        let migrator = AppMigrator(
            migrations: [TestMigration { runCount += 1 }]
        )

        migrator.run()

        #expect(runCount == 1)
        #expect(appStorage.bool(forKey: AppMigrationID.userDefaultsCodable.rawValue))

        migrator.run()

        #expect(runCount == 1)
    }

    @Test
    func retriesFailedMigration() {
        var runCount = 0
        let migrator = AppMigrator(
            migrations: [
                TestMigration {
                    runCount += 1
                    throw TestError()
                }
            ]
        )

        withKnownIssue {
            migrator.run()
        }
        #expect(runCount == 1)
        #expect(!appStorage.bool(forKey: AppMigrationID.userDefaultsCodable.rawValue))

        withKnownIssue {
            migrator.run()
        }
        #expect(runCount == 2)
    }
}

private struct TestMigration: AppMigration {
    let id: AppMigrationID = .userDefaultsCodable
    let operation: () throws -> Void

    init(operation: @escaping () throws -> Void) {
        self.operation = operation
    }

    func run() throws {
        try operation()
    }
}

private struct TestError: Error {}
