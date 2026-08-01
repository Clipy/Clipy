//
//  AppMigration.swift
//
//  Clipy
//  GitHub: https://github.com/clipy
//  HP: https://clipy-app.com
//
//  Created by Shunsuke Furubayashi on 2026/08/01.
//
//  Copyright © 2015-2026 Clipy Project.
//

protocol AppMigration {
    var id: AppMigrationID { get }

    func run() throws
}

/// A unique identifier used to record a completed migration.
///
/// Never remove an identifier, even if the corresponding migration is removed. Reusing the same identifier
/// prevents a new migration from running for users who completed the previous migration.
enum AppMigrationID: String {
    case userDefaultsCodable = "appMigrationUserDefaultsCodable"
}
