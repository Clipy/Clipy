//
//  PasteboardTypeSettings.swift
//
//  Clipy
//  GitHub: https://github.com/clipy
//  HP: https://clipy-app.com
//
//  Created by Shunsuke Furubayashi on 2026/08/09.
//
//  Copyright © 2015-2026 Clipy Project.
//

import Foundation

struct PasteboardTypeSettings: Codable, Equatable, Sendable {
    private var values: [String: Bool]

    init(values: [String: Bool] = [:]) {
        self.values = values
    }

    subscript(type: PasteboardAvailableType) -> Bool {
        get { values[type.rawValue] ?? true }
        set { values[type.rawValue] = newValue }
    }

    var enabledTypes: [PasteboardAvailableType] {
        PasteboardAvailableType.allCases.filter { self[$0] }
    }
}
