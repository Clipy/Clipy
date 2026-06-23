//
//  ClipyApp.swift
//
//  Clipy
//  GitHub: https://github.com/clipy
//  HP: https://clipy-app.com
//
//  Created by Econa77 on 2026/06/11.
//
//  Copyright © 2015-2026 Clipy Project.
//

import Dependencies
import SwiftUI

@main
struct ClipyApp: App {
    @NSApplicationDelegateAdaptor(AppDelegate.self) private var appDelegate

    private let migration = DatabaseMigration()

    init() {
        // If the SQLite database file does not exist yet, start the database and then migrate Realm data to SQLiteData.
        let sqliteDatabaseExists = (try? SQLiteDataDatabase.databaseURL().checkResourceIsReachable()) ?? false
        prepareDependencies { values in
            try! values.bootstrapDatabase()
            if !sqliteDatabaseExists {
                migration.migrateFromRealmToSQLiteData()
            }
        }
    }

    var body: some Scene {
        MenuBarExtra("", isInserted: .constant(false)) {
            EmptyView()
        }
    }
}
