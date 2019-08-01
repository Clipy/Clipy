//
//  KeyCombo.swift
//  Magnet
//
//  Created by 古林俊佑 on 2016/03/09.
//  Copyright © 2016年 Shunsuke Furubayashi. All rights reserved.
//

import Cocoa
import Carbon

public final class KeyCombo: NSObject, NSCopying, NSCoding, Codable {

    // MARK: - Properties
    public let keyCode: Int
    public let modifiers: Int
    public let doubledModifiers: Bool
    public var characters: String {
        if doubledModifiers { return "" }
        return KeyCodeTransformer.shared.transformValue(keyCode, carbonModifiers: modifiers)
    }

    // MARK: - Initialize
    public init?(keyCode: Int, carbonModifiers: Int) {
        if keyCode < 0 || carbonModifiers < 0 { return nil }

        if KeyTransformer.containsFunctionKey(keyCode) {
            self.modifiers = Int(UInt(carbonModifiers) | NSEvent.ModifierFlags.function.rawValue)
        } else {
            self.modifiers = carbonModifiers
        }
        self.keyCode = keyCode
        self.doubledModifiers = false
    }

    public init?(keyCode: Int, cocoaModifiers: NSEvent.ModifierFlags) {
        if keyCode < 0 || !KeyTransformer.supportedCocoaFlags(cocoaModifiers) { return nil }

        if KeyTransformer.containsFunctionKey(keyCode) {
            self.modifiers = Int(UInt(KeyTransformer.carbonFlags(from: cocoaModifiers)) | NSEvent.ModifierFlags.function.rawValue)
        } else {
            self.modifiers = KeyTransformer.carbonFlags(from: cocoaModifiers)
        }
        self.keyCode = keyCode
        self.doubledModifiers = false
    }

    public init?(doubledCarbonModifiers modifiers: Int) {
        if !KeyTransformer.singleCarbonFlags(modifiers) { return nil }

        self.keyCode = 0
        self.modifiers = modifiers
        self.doubledModifiers = true
    }

    public init?(doubledCocoaModifiers modifiers: NSEvent.ModifierFlags) {
        if !KeyTransformer.singleCocoaFlags(modifiers) { return nil }

        self.keyCode = 0
        self.modifiers = KeyTransformer.carbonFlags(from: modifiers)
        self.doubledModifiers = true
    }

    public func copy(with zone: NSZone?) -> Any {
        if doubledModifiers {
            return KeyCombo(doubledCarbonModifiers: modifiers)!
        } else {
            return KeyCombo(keyCode: keyCode, carbonModifiers: modifiers)!
        }
    }

    public init?(coder aDecoder: NSCoder) {
        self.keyCode = aDecoder.decodeInteger(forKey: "keyCode")
        self.modifiers = aDecoder.decodeInteger(forKey: "modifiers")
        self.doubledModifiers = aDecoder.decodeBool(forKey: "doubledModifiers")
    }

    public func encode(with aCoder: NSCoder) {
        aCoder.encode(keyCode, forKey: "keyCode")
        aCoder.encode(modifiers, forKey: "modifiers")
        aCoder.encode(doubledModifiers, forKey: "doubledModifiers")
    }

    // MARK: - Equatable
    public override func isEqual(_ object: Any?) -> Bool {
        guard let keyCombo = object as? KeyCombo else { return false }
        return keyCode == keyCombo.keyCode &&
                modifiers == keyCombo.modifiers &&
                doubledModifiers == keyCombo.doubledModifiers
    }
}
