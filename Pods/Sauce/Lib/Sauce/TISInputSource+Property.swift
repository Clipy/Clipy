//  TISInputSource+Property.swift
//
//  Sauce
//  GitHub: https://github.com/clipy
//  HP: https://clipy-app.com
//
//  Created by Econa77 on 2018/07/30.
//
//  Copyright © 2018 Clipy Project.
//

import Foundation
import Carbon

extension TISInputSource {
    func value<T>(forProperty propertyKey: CFString, type: T.Type) -> T? {
        guard let value = TISGetInputSourceProperty(self, propertyKey) else { return nil }
        return Unmanaged<AnyObject>.fromOpaque(value).takeUnretainedValue() as? T
    }
}
