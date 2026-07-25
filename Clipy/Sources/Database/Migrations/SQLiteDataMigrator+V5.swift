//
//  SQLiteDataMigrator+V5.swift
//
//  Clipy
//  GitHub: https://github.com/clipy
//
//  Copyright © 2015-2026 Clipy Project.
//

import SQLiteData

extension DatabaseMigrator {
    mutating func registerMigrationV5() {
        // `deviceID` was added to the original schema after some users had already
        // applied that migration. Add it explicitly for those existing databases.
        registerMigration("Add deviceID to pasteboardHistories") { database in
            try #sql(
                """
                ALTER TABLE "pasteboardHistories"
                ADD COLUMN "deviceID" TEXT
                """
            )
            .execute(database)
        }
    }
}
