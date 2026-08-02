//
//  MonospacedDigitFormatter.swift
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

enum MonospacedDigitFormatter {
    /// Creates a numbered title whose digits use monospaced spacing.
    ///
    /// - Parameter font: The base font. Defaults to `NSFont.menuFont(ofSize: 0)`,
    ///   where passing `0` returns the default menu font.
    static func numberedTitle(
        _ title: String,
        listNumber: Int,
        showsNumber: Bool,
        font: NSFont = NSFont.menuFont(ofSize: 0)
    ) -> NSAttributedString {
        guard showsNumber else { return NSAttributedString(string: title) }

        let text = NSMutableAttributedString()
        text.append(monospacedDigits(listNumber, font: font))
        text.append(NSAttributedString(string: ". \(title)"))
        return text
    }

    /// Creates a numeric range title whose digits use monospaced spacing.
    ///
    /// - Parameter font: The base font. Defaults to `NSFont.menuFont(ofSize: 0)`,
    ///   where passing `0` returns the default menu font.
    static func rangeTitle(
        firstNumber: Int,
        lastNumber: Int,
        font: NSFont = NSFont.menuFont(ofSize: 0)
    ) -> NSAttributedString {
        let text = NSMutableAttributedString()
        text.append(monospacedDigits(firstNumber, font: font))
        text.append(NSAttributedString(string: " - "))
        text.append(monospacedDigits(lastNumber, font: font))
        return text
    }

    private static func monospacedDigits(_ number: Int, font: NSFont) -> NSAttributedString {
        let monospacedSettings: [NSFontDescriptor.FeatureKey: Any] = [
            .typeIdentifier: kNumberSpacingType,
            .selectorIdentifier: kMonospacedNumbersSelector
        ]
        let monospacedFont = NSFont(
            descriptor: font.fontDescriptor.addingAttributes([.featureSettings: [monospacedSettings]]),
            size: font.pointSize
        )
        return NSAttributedString(string: String(number), attributes: [.font: monospacedFont ?? font])
    }
}
