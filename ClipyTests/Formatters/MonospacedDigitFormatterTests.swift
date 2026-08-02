//
//  MonospacedDigitFormatterTests.swift
//
//  Clipy
//  GitHub: https://github.com/clipy
//  HP: https://clipy-app.com
//
//  Created by Shunsuke Furubayashi on 2026/08/02.
//
//  Copyright © 2015-2026 Clipy Project.
//

import AppKit
import CoreText
import Testing
@testable import Clipy

@MainActor
@Suite
struct MonospacedDigitFormatterTests {
    @Test
    func numberedTitleUsesMonospacedDigitsOnlyForTheNumber() throws {
        let baseFont = NSFont.systemFont(ofSize: 17, weight: .bold)
        let title = MonospacedDigitFormatter.numberedTitle(
            "History",
            listNumber: 12,
            showsNumber: true,
            font: baseFont
        )

        #expect(title.string == "12. History")
        try expectMonospacedDigits(at: 0, in: title, basedOn: baseFont)
        try expectMonospacedDigits(at: 1, in: title, basedOn: baseFont)
        #expect(font(at: 2, in: title) == nil)
    }

    @Test
    func rangeTitleUsesMonospacedDigitsOnlyForBothNumbers() throws {
        let baseFont = NSFont.menuFont(ofSize: 0)
        let title = MonospacedDigitFormatter.rangeTitle(firstNumber: 1, lastNumber: 10)

        #expect(title.string == "1 - 10")
        try expectMonospacedDigits(at: 0, in: title, basedOn: baseFont)
        #expect(font(at: 1, in: title) == nil)
        #expect(font(at: 2, in: title) == nil)
        #expect(font(at: 3, in: title) == nil)
        try expectMonospacedDigits(at: 4, in: title, basedOn: baseFont)
        try expectMonospacedDigits(at: 5, in: title, basedOn: baseFont)
    }

    @Test
    func unnumberedTitleKeepsTheDefaultFont() {
        let title = MonospacedDigitFormatter.numberedTitle(
            "History",
            listNumber: 1,
            showsNumber: false
        )

        #expect(title.string == "History")
        #expect(font(at: 0, in: title) == nil)
    }

    private func expectMonospacedDigits(
        at index: Int,
        in title: NSAttributedString,
        basedOn baseFont: NSFont
    ) throws {
        let font = font(at: index, in: title)
        #expect(font?.familyName == baseFont.familyName)
        #expect(font?.pointSize == baseFont.pointSize)
        #expect(font?.fontDescriptor.symbolicTraits == baseFont.fontDescriptor.symbolicTraits)

        let monospacedSettings: [NSFontDescriptor.FeatureKey: Int] = [
            .typeIdentifier: kNumberSpacingType,
            .selectorIdentifier: kMonospacedNumbersSelector
        ]
        let featureSettings = try #require(font?.fontDescriptor.fontAttributes[.featureSettings] as? [[NSFontDescriptor.FeatureKey: Int]])
        #expect(featureSettings == [monospacedSettings])
    }

    private func font(at index: Int, in title: NSAttributedString) -> NSFont? {
        title.attribute(.font, at: index, effectiveRange: nil) as? NSFont
    }
}
