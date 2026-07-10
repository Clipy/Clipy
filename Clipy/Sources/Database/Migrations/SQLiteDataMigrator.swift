//
//  SQLiteDataMigrator.swift
//
//  Clipy
//  GitHub: https://github.com/clipy
//  HP: https://clipy-app.com
//
//  Created by Shunsuke Furubayashi on 2026/05/22.
//
//  Copyright © 2015-2026 Clipy Project.
//

import SQLiteData

extension DatabaseMigrator {
    mutating func registerMigration() {
        registerMigrationV1()
        registerMigrationV2()
        registerMigrationV3()
        registerMigrationV4()
    }
}
