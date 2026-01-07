//
//  Realm+Migration.swift
//
//  Clipy
//  GitHub: https://github.com/clipy
//  HP: https://clipy-app.com
//
//  Created by Econa77 on 2016/10/16.
//
//  Copyright © 2015-2018 Clipy Project.
//

import Foundation
import RealmSwift

extension Realm {
    static func migration() {
        let config = Realm.Configuration(schemaVersion: 7, migrationBlock: { migration, oldSchemaVersion in
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
        })
        Realm.Configuration.defaultConfiguration = config
        do {
            _ = try Realm()
        } catch let error as NSError {
            if error.domain == "io.realm" && (error.code == 16 || error.code == 8) {
                if let url = config.fileURL {
                    let fm = FileManager.default
                    let timestamp = Int(Date().timeIntervalSince1970)
                    let backupURL = url.deletingPathExtension().appendingPathExtension("bak-\(timestamp)")
                    _ = try? fm.moveItem(at: url, to: backupURL)
                    let lockURL = url.deletingPathExtension().appendingPathExtension("lock")
                    _ = try? fm.removeItem(at: lockURL)
                    let managementURL = url.deletingLastPathComponent().appendingPathComponent("default.realm.management")
                    _ = try? fm.removeItem(at: managementURL)
                }
                do {
                    _ = try Realm()
                } catch {
                    var inMemory = config
                    inMemory.inMemoryIdentifier = "dev"
                    Realm.Configuration.defaultConfiguration = inMemory
                    _ = try! Realm()
                }
            } else {
                fatalError("Realm init failed: \(error)")
            }
        }
    }
}
