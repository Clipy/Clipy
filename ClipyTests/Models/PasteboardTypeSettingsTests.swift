//
//  PasteboardTypeSettingsTests.swift
//
//  ClipyTests
//  GitHub: https://github.com/clipy
//  HP: https://clipy-app.com
//
//  Created by Shunsuke Furubayashi on 2026/08/09.
//
//  Copyright © 2015-2026 Clipy Project.
//

import Foundation
import Testing
@testable import Clipy

struct PasteboardTypeSettingsTests {
    @Test
    func enablesTypesWithoutStoredValues() {
        let settings = PasteboardTypeSettings()

        #expect(PasteboardAvailableType.allCases.allSatisfy { settings[$0] })
        #expect(settings.enabledTypes == PasteboardAvailableType.allCases)
    }

    @Test
    func updatesAType() {
        var settings = PasteboardTypeSettings()

        settings[.pdf] = false

        #expect(!settings[.pdf])
        #expect(settings[.string])
        #expect(!settings.enabledTypes.contains(.pdf))
    }

    @Test
    func codableRoundTripPreservesValuesAndDefaultsMissingTypesToEnabled() throws {
        let settings = PasteboardTypeSettings(
            values: [PasteboardAvailableType.pdf.rawValue: false]
        )

        let data = try JSONEncoder().encode(settings)
        let decodedSettings = try JSONDecoder().decode(PasteboardTypeSettings.self, from: data)

        #expect(decodedSettings == settings)
        #expect(!decodedSettings[.pdf])
        #expect(decodedSettings[.string])
    }
}
