//
//  SQLiteDataMigratorTests.swift
//
//  Clipy
//  GitHub: https://github.com/clipy
//  HP: https://clipy-app.com
//
//  Created by Shunsuke Furubayashi on 2026/05/26.
//
//  Copyright © 2015-2026 Clipy Project.
//

import SQLiteData
import Testing
@testable import Clipy

@MainActor
@Suite
struct SQLiteDataMigratorTests {
    @Test
    func migrationIdentifiers() throws {
        let database = try DatabaseQueue()
        var migrator = DatabaseMigrator()
        migrator.registerMigration()
        try migrator.migrate(database)

        try database.read { database in
            let identifiers = try migrationIdentifiers(database)
            #expect(
                identifiers == [
                    "Create initial tables",
                    "Create search indexes",
                    "Add createdAt to pasteboardHistories"
                ]
            )
        }
    }
}

extension SQLiteDataMigratorTests {
    func migrationIdentifiers(_ database: Database) throws -> [String] {
        try #sql(
            """
            SELECT "identifier"
            FROM "grdb_migrations"
            ORDER BY "rowid"
            """,
            as: String.self
        )
        .fetchAll(database)
    }

    func tableNames(_ database: Database) throws -> [String] {
        try #sql(
            """
            SELECT "name"
            FROM "sqlite_master"
            WHERE "type" = 'table'
            ORDER BY "name"
            """,
            as: String.self
        )
        .fetchAll(database)
    }

    func indexes(_ database: Database) throws -> [String] {
        try #sql(
            """
            SELECT "name"
            FROM "sqlite_master"
            WHERE "type" = 'index'
            AND "name" GLOB 'index_*'
            ORDER BY "name"
            """,
            as: String.self
        )
        .fetchAll(database)
    }

    func triggers(_ database: Database) throws -> [String] {
        try #sql(
            """
            SELECT "name"
            FROM "sqlite_master"
            WHERE "type" = 'trigger'
            ORDER BY "name"
            """,
            as: String.self
        )
        .fetchAll(database)
    }

    func columnNames(of tableName: String, database: Database) throws -> [String] {
        try #sql(
            """
            SELECT "name"
            FROM pragma_table_info('\(raw: tableName)')
            ORDER BY "name"
            """,
            as: String.self
        )
        .fetchAll(database)
    }
}
