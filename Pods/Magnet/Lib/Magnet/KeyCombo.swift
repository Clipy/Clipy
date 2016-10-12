//
//  KeyCombo.swift
//  Magnet
//
//  Created by 古林俊佑 on 2016/03/09.
//  Copyright © 2016年 Shunsuke Furubayashi. All rights reserved.
//

import Cocoa
import Carbon

public final class KeyCombo: NSObject, NSCopying, NSCoding {

    // MARK: - Properties
    public let keyCode: Int
    public let modifiers: Int
    public let doubledModifiers: Bool
    public var characters: String {
        if doubledModifiers { return "" }
        return KeyCodeTransformer.sharedTransformer.transformValue(keyCode, carbonModifiers: modifiers)
    }

    // MARK: - Initialize
    public init?(keyCode: Int, carbonModifiers: Int) {
        if keyCode < 0 || carbonModifiers < 0 { return nil }

        if KeyTransformer.containsFunctionKey(keyCode) {
            self.modifiers = Int(UInt(carbonModifiers) | NSEventModifierFlags.FunctionKeyMask.rawValue)
        } else {
            self.modifiers = carbonModifiers
        }
        self.keyCode = keyCode
        self.doubledModifiers = false
    }

    public init?(keyCode: Int, cocoaModifiers: NSEventModifierFlags) {
        if keyCode < 0 || !KeyTransformer.supportedCocoaFlags(cocoaModifiers) { return nil }

        if KeyTransformer.containsFunctionKey(keyCode) {
            self.modifiers = Int(UInt(KeyTransformer.cocoaToCarbonFlags(cocoaModifiers)) | NSEventModifierFlags.FunctionKeyMask.rawValue)
        } else {
            self.modifiers = KeyTransformer.cocoaToCarbonFlags(cocoaModifiers)
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

    public init?(doubledCocoaModifiers modifiers: NSEventModifierFlags) {
        if !KeyTransformer.singleCocoaFlags(modifiers) { return nil }

        self.keyCode = 0
        self.modifiers = KeyTransformer.cocoaToCarbonFlags(modifiers)
        self.doubledModifiers = true
    }

    public func copyWithZone(zone: NSZone) -> AnyObject {
        if doubledModifiers {
            return KeyCombo(doubledCarbonModifiers: modifiers)!
        } else {
            return KeyCombo(keyCode: keyCode, carbonModifiers: modifiers)!
        }
    }

    public init?(coder aDecoder: NSCoder) {
        self.keyCode = aDecoder.decodeIntegerForKey("keyCode")
        self.modifiers = aDecoder.decodeIntegerForKey("modifiers")
        self.doubledModifiers = aDecoder.decodeBoolForKey("doubledModifiers")
    }

    public func encodeWithCoder(aCoder: NSCoder) {
        aCoder.encodeInteger(keyCode, forKey: "keyCode")
        aCoder.encodeInteger(modifiers, forKey: "modifiers")
        aCoder.encodeBool(doubledModifiers, forKey: "doubledModifiers")
    }

    // MARK: - Equatable
    public override func isEqual(object: AnyObject?) -> Bool {
        guard let keyCombo = object as? KeyCombo else { return false }
        return keyCode == keyCombo.keyCode &&
                modifiers == keyCombo.modifiers &&
                doubledModifiers == keyCombo.doubledModifiers
    }
}
