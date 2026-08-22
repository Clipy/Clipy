//
//  DeviceIdentifier.swift
//
//  Clipy
//  GitHub: https://github.com/clipy
//  HP: https://clipy-app.com
//
//  Created by Shunsuke Furubayashi on 2026/08/22.
//
//  Copyright © 2015-2026 Clipy Project.
//

import Dependencies
import Foundation
import IOKit

extension DependencyValues {
    var deviceIdentifier: String? {
        get { self[DeviceIdentifierKey.self] }
        set { self[DeviceIdentifierKey.self] = newValue }
    }

    private enum DeviceIdentifierKey: DependencyKey {
        static let liveValue: String? = {
            // ref: https://gist.github.com/vadimpiven/3373bb2592d59560b5d698ba1e2ed7e4
            let platformExpert = IOServiceGetMatchingService(
                kIOMainPortDefault,
                IOServiceMatching("IOPlatformExpertDevice")
            )
            guard platformExpert != 0 else { return nil }
            defer { IOObjectRelease(platformExpert) }

            guard let property = IORegistryEntryCreateCFProperty(
                platformExpert,
                kIOPlatformUUIDKey as CFString,
                kCFAllocatorDefault,
                0
            ) else {
                return nil
            }

            return property.takeRetainedValue() as? String
        }()

        static let previewValue: String? = UUID().uuidString
        static let testValue: String? = UUID().uuidString
    }
}
