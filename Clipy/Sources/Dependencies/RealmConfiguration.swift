//
//  RealmConfiguration.swift
//
//  Clipy
//  GitHub: https://github.com/clipy
//  HP: https://clipy-app.com
//
//  Created by Shunsuke Furubayashi on 2026/05/22.
//
//  Copyright © 2015-2026 Clipy Project.
//

import AppKit
import Dependencies
import RealmSwift

extension DependencyValues {
    var realmConfiguration: Realm.Configuration {
        get { self[RealmConfigurationKey.self] }
        set { self[RealmConfigurationKey.self] = newValue }
    }

    private enum RealmConfigurationKey: DependencyKey {
        static let liveValue = Realm.Configuration(
            schemaVersion: 7,
            migrationBlock: { migration, oldSchemaVersion in
                if oldSchemaVersion <= 2 {
                    // Add identifier in CPYSnippet
                    migration.enumerateObjects(ofType: CPYSnippet.className()) { _, newObject in
                        newObject!["identifier"] = NSUUID().uuidString
                    }
                }
                if oldSchemaVersion <= 4 {
                    // Add identifier in CPYFolder
                    migration.enumerateObjects(ofType: CPYFolder.className()) { _, newObject in
                        newObject!["identifier"] = NSUUID().uuidString
                    }
                }
                if oldSchemaVersion <= 5 {
                    // Update RealmObjc to RealmSwift
                    migration.enumerateObjects(ofType: CPYClip.className(), { oldObject, newObject in
                        newObject!["dataPath"] = oldObject!["dataPath"]
                        newObject!["title"] = oldObject!["title"]
                        newObject!["dataHash"] = oldObject!["dataHash"]
                        newObject!["primaryType"] = oldObject!["primaryType"]
                        newObject!["updateTime"] = oldObject!["updateTime"]
                        newObject!["thumbnailPath"] = oldObject!["thumbnailPath"]
                    })
                    migration.enumerateObjects(ofType: CPYSnippet.className(), { oldObject, newObject in
                        newObject!["index"] = oldObject!["index"]
                        newObject!["enable"] = oldObject!["enable"]
                        newObject!["title"] = oldObject!["title"]
                        newObject!["content"] = oldObject!["content"]
                        if oldSchemaVersion >= 3 {
                            newObject!["identifier"] = oldObject!["identifier"]
                        }
                    })
                    migration.enumerateObjects(ofType: CPYFolder.className(), { oldObject, newObject in
                        newObject!["index"] = oldObject!["index"]
                        newObject!["enable"] = oldObject!["enable"]
                        newObject!["title"] = oldObject!["title"]
                        if oldSchemaVersion >= 5 {
                            newObject!["identifier"] = oldObject!["identifier"]
                        }
                    })
                }
            }
        )

        static let previewValue = Realm.Configuration(inMemoryIdentifier: UUID().uuidString)
        static let testValue = Realm.Configuration(inMemoryIdentifier: UUID().uuidString)
    }
}
