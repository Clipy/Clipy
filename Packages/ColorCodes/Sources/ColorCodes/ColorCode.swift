//
//  ColorCode.swift
//
//  ColorCodes
//  GitHub: https://github.com/clipy
//  HP: https://clipy-app.com
//
//  Created by Shunsuke Furubayashi on 2026/07/05.
//
//  Copyright © 2015-2026 Clipy Project.
//

import AppKit

/// A color parsed from a color code string, such as `#ff8942`, `rgb(255 137 66)`,
/// or `oklch(70% 0.15 50)`.
///
/// Supported styles:
/// - Hex: `#f84`, `#f848`, `#ff8942`, `#ff894280`
/// - CSS color functions (CSS Color Module Level 4):
///   `rgb()` / `rgba()`, `hsl()` / `hsla()`, `hwb()`,
///   `lab()`, `lch()`, `oklab()`, `oklch()`, `color()`
/// - CSS named colors (opt-in): `tomato`, `rebeccapurple`, ...
/// - Hex without the leading `#` (opt-in): `f84`, `ff8942`, ...
public struct ColorCode: Equatable {
    /// Opt-in styles that are too ambiguous to detect by default.
    public struct ParseOptions: OptionSet, Sendable {
        public let rawValue: Int

        public init(rawValue: Int) {
            self.rawValue = rawValue
        }

        /// CSS named colors such as `tomato`. Everyday words
        /// (`orange`, `salmon`, `tan`, ...) are also detected as colors.
        public static let cssKeyword = ParseOptions(rawValue: 1 << 0)
        /// Hex codes without the leading `#`, such as `ff8942`. Hex-only words
        /// (`beef`, `facade`, ...) and plain numbers (`123456`) are also
        /// detected as colors.
        public static let bareHex = ParseOptions(rawValue: 1 << 1)
    }

    public enum CodeType: Equatable, CaseIterable, Sendable {
        case shortHex           // #rgb
        case shortHexWithAlpha  // #rgba
        case hex                // #rrggbb
        case hexWithAlpha       // #rrggbbaa
        case cssRGB             // rgb(255, 137, 66) / rgb(100% 54% 26% / 50%)
        case cssHSL             // hsl(23, 100%, 63%) / hsl(23deg 100% 63%)
        case cssHWB             // hwb(23 26% 0%)
        case cssLab             // lab(69% 41 63)
        case cssLCh             // lch(69% 75 57)
        case cssOKLab           // oklab(0.75 0.09 0.12)
        case cssOKLCh           // oklch(0.75 0.15 55)
        case cssColorFunction   // color(display-p3 1 0.54 0.26)
        case cssKeyword         // rebeccapurple
    }

    // MARK: - Properties
    public let type: CodeType
    public let color: NSColor

    // MARK: - Initialize
    init(type: CodeType, color: NSColor) {
        self.type = type
        self.color = color
    }

    /// Parses a color code. The whole string must be a single color code,
    /// aside from surrounding whitespace.
    ///
    /// - Parameters:
    ///   - string: The string to parse.
    ///   - options: Additional opt-in styles. Empty by default because the
    ///     styles behind each option would misdetect everyday strings.
    public init?(_ string: String, options: ParseOptions = []) {
        let code = string.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !code.isEmpty else { return nil }

        if code.hasPrefix("#") {
            guard let colorCode = Self.parseHex(code.dropFirst()) else { return nil }
            self = colorCode
        } else if let colorCode = CSSColorFunctionParser.parse(code) {
            self = colorCode
        } else if options.contains(.cssKeyword), let color = CSSNamedColor.color(for: code) {
            self.init(type: .cssKeyword, color: color)
        } else if options.contains(.bareHex), let colorCode = Self.parseHex(code[...]) {
            self = colorCode
        } else {
            return nil
        }
    }
}

// MARK: - Hex
private extension ColorCode {
    static func parseHex(_ digits: Substring) -> ColorCode? {
        let nibbles = digits.compactMap { character -> Int? in
            guard character.isASCII else { return nil }
            return character.hexDigitValue
        }
        guard nibbles.count == digits.count else { return nil }

        let components: [CGFloat]
        let type: CodeType
        switch nibbles.count {
        case 3, 4:
            // Each digit doubles: #f84 == #ff8844
            components = nibbles.map { CGFloat($0 * 17) / 255 }
            type = nibbles.count == 3 ? .shortHex : .shortHexWithAlpha
        case 6, 8:
            components = stride(from: 0, to: nibbles.count, by: 2).map {
                CGFloat(nibbles[$0] * 16 + nibbles[$0 + 1]) / 255
            }
            type = nibbles.count == 6 ? .hex : .hexWithAlpha
        default:
            return nil
        }

        let color = NSColor(srgbRed: components[0],
                            green: components[1],
                            blue: components[2],
                            alpha: components.count == 4 ? components[3] : 1)
        return ColorCode(type: type, color: color)
    }
}
