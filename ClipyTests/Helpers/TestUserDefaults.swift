//
//  TestUserDefaults.swift
//  Clipy
//
//  Created by 古林俊佑 on 2017/08/13.
//  Copyright © 2017年 Shunsuke Furubayashi. All rights reserved.
//

import Foundation
@testable import Clipy

final class TestUserDefaults: NSObject, KeyValueStorable {

    // MARK: - Properties
    var store = [String: Any]()

    // MARK: KeyValue Storable
    @nonobjc func set(_ value: Any?, forKey defaultName: String) {
        store[defaultName] = value
    }

    @nonobjc func set(_ value: Int, forKey defaultName: String) {
        store[defaultName] = value
    }

    @nonobjc func set(_ value: Float, forKey defaultName: String) {
        store[defaultName] = value
    }

    @nonobjc func set(_ value: Double, forKey defaultName: String) {
        store[defaultName] = value
    }

    @nonobjc func set(_ value: Bool, forKey defaultName: String) {
        store[defaultName] = value
    }

    func removeObject(forKey defaultName: String) {
        store.removeValue(forKey: defaultName)
    }

    func object(forKey defaultName: String) -> Any? {
        return store[defaultName]
    }

    func string(forKey defaultName: String) -> String? {
        return object(forKey: defaultName) as? String
    }

    func array(forKey defaultName: String) -> [Any]? {
        return object(forKey: defaultName) as? [Any]
    }

    func dictionary(forKey defaultName: String) -> [String : Any]? {
        return object(forKey: defaultName) as? [String: Any]
    }

    func data(forKey defaultName: String) -> Data? {
        return object(forKey: defaultName) as? Data
    }

    func stringArray(forKey defaultName: String) -> [String]? {
        return object(forKey: defaultName) as? [String]
    }

    func integer(forKey defaultName: String) -> Int {
        return object(forKey: defaultName) as? Int ?? 0
    }

    func float(forKey defaultName: String) -> Float {
        return object(forKey: defaultName) as? Float ?? 0
    }

    func double(forKey defaultName: String) -> Double {
        return object(forKey: defaultName) as? Double ?? 0
    }

    func bool(forKey defaultName: String) -> Bool {
        return object(forKey: defaultName) as? Bool ?? false
    }

    func register(defaults registrationDictionary: [String : Any]) {
        registrationDictionary.forEach { key, value in
            store[key] = value
        }
    }

    @discardableResult
    func synchronize() -> Bool {
        return true
    }

    var object: NSObject { return self }

}
