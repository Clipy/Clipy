//
//  KeyValueStoable.swift
//  Clipy
//
//  Created by 古林俊佑 on 2017/08/13.
//  Copyright © 2017年 Shunsuke Furubayashi. All rights reserved.
//

import Foundation
import RxSwift

protocol KeyValueStorable: class {
    func set(_ value: Any?, forKey defaultName: String)
    func set(_ value: Int, forKey defaultName: String)
    func set(_ value: Float, forKey defaultName: String)
    func set(_ value: Double, forKey defaultName: String)
    func set(_ value: Bool, forKey defaultName: String)
    func removeObject(forKey defaultName: String)

    func object(forKey defaultName: String) -> Any?
    func string(forKey defaultName: String) -> String?
    func array(forKey defaultName: String) -> [Any]?
    func dictionary(forKey defaultName: String) -> [String: Any]?
    func data(forKey defaultName: String) -> Data?
    func stringArray(forKey defaultName: String) -> [String]?
    func integer(forKey defaultName: String) -> Int
    func float(forKey defaultName: String) -> Float
    func double(forKey defaultName: String) -> Double
    func bool(forKey defaultName: String) -> Bool

    func register(defaults registrationDictionary: [String : Any])

    @discardableResult
    func synchronize() -> Bool

    var object: NSObject { get }
}

extension KeyValueStorable {
    var rx: Reactive<NSObject> { // swiftlint:disable:this identifier_name
        return object.rx
    }
}

extension UserDefaults: KeyValueStorable {
    var object: NSObject { return self }
}
