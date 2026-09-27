//
//  CSSColorFunctionParser.swift
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

/// Parses CSS color function syntax (`rgb()`, `hsl()`, `hwb()`, `lab()`, `lch()`,
/// `oklab()`, `oklch()`, `color()`) as defined by CSS Color Module Level 4.
///
/// Both the legacy comma syntax (`rgb(255, 137, 66)`, `rgb()` / `hsl()` only) and
/// the modern space syntax (`rgb(255 137 66 / 50%)`) are accepted.
/// Out-of-range values are clamped following the specification, and colors
/// outside the target gamut are clipped into it.
enum CSSColorFunctionParser {

    static func parse(_ code: String) -> ColorCode? {
        guard let function = Function(code) else { return nil }
        switch function.name {
        case "rgb", "rgba": return rgb(function)
        case "hsl", "hsla": return hsl(function)
        case "hwb": return hwb(function)
        case "lab": return lab(function)
        case "lch": return lch(function)
        case "oklab": return oklab(function)
        case "oklch": return oklch(function)
        case "color": return profiledColor(function)
        default: return nil
        }
    }
}

// MARK: - Functions
private extension CSSColorFunctionParser {
    static func rgb(_ function: Function) -> ColorCode? {
        guard let components = function.resolvedParameters(numberScale: 1 / 255, percentageScale: 1 / 100),
              let alpha = function.resolvedAlpha() else { return nil }
        return srgbColor(components, alpha: alpha).map { ColorCode(type: .cssRGB, color: $0) }
    }

    static func hsl(_ function: Function) -> ColorCode? {
        guard function.parameters.count == 3,
              let hue = Value(function.parameters[0])?.hueDegrees,
              let saturation = Value(function.parameters[1])?.resolved(numberScale: 1 / 100, percentageScale: 1 / 100),
              let lightness = Value(function.parameters[2])?.resolved(numberScale: 1 / 100, percentageScale: 1 / 100),
              let alpha = function.resolvedAlpha() else { return nil }
        let rgb = CSSColorConversion.srgbFromHSL(hue: normalizedHue(hue),
                                                 saturation: saturation.clamped(to: 0...1),
                                                 lightness: lightness.clamped(to: 0...1))
        return srgbColor(rgb, alpha: alpha).map { ColorCode(type: .cssHSL, color: $0) }
    }

    static func hwb(_ function: Function) -> ColorCode? {
        guard !function.usesCommas,
              function.parameters.count == 3,
              let hue = Value(function.parameters[0])?.hueDegrees,
              let whiteness = Value(function.parameters[1])?.resolved(numberScale: 1 / 100, percentageScale: 1 / 100),
              let blackness = Value(function.parameters[2])?.resolved(numberScale: 1 / 100, percentageScale: 1 / 100),
              let alpha = function.resolvedAlpha() else { return nil }
        let rgb = CSSColorConversion.srgbFromHWB(hue: normalizedHue(hue),
                                                 whiteness: whiteness.clamped(to: 0...1),
                                                 blackness: blackness.clamped(to: 0...1))
        return srgbColor(rgb, alpha: alpha).map { ColorCode(type: .cssHWB, color: $0) }
    }

    static func lab(_ function: Function) -> ColorCode? {
        guard !function.usesCommas,
              function.parameters.count == 3,
              let lightness = Value(function.parameters[0])?.resolved(numberScale: 1, percentageScale: 1),
              let aAxis = Value(function.parameters[1])?.resolved(numberScale: 1, percentageScale: 1.25),
              let bAxis = Value(function.parameters[2])?.resolved(numberScale: 1, percentageScale: 1.25),
              let alpha = function.resolvedAlpha() else { return nil }
        let rgb = CSSColorConversion.srgbFromLab(lightness: lightness.clamped(to: 0...100), aAxis: aAxis, bAxis: bAxis)
        return srgbColor(rgb, alpha: alpha).map { ColorCode(type: .cssLab, color: $0) }
    }

    static func lch(_ function: Function) -> ColorCode? {
        guard !function.usesCommas,
              function.parameters.count == 3,
              let lightness = Value(function.parameters[0])?.resolved(numberScale: 1, percentageScale: 1),
              let chroma = Value(function.parameters[1])?.resolved(numberScale: 1, percentageScale: 1.5),
              let hue = Value(function.parameters[2])?.hueDegrees,
              let alpha = function.resolvedAlpha() else { return nil }
        let rgb = CSSColorConversion.srgbFromLCh(lightness: lightness.clamped(to: 0...100),
                                                 chroma: max(chroma, 0),
                                                 hue: normalizedHue(hue))
        return srgbColor(rgb, alpha: alpha).map { ColorCode(type: .cssLCh, color: $0) }
    }

    static func oklab(_ function: Function) -> ColorCode? {
        guard !function.usesCommas,
              function.parameters.count == 3,
              let lightness = Value(function.parameters[0])?.resolved(numberScale: 1, percentageScale: 1 / 100),
              let aAxis = Value(function.parameters[1])?.resolved(numberScale: 1, percentageScale: 0.004),
              let bAxis = Value(function.parameters[2])?.resolved(numberScale: 1, percentageScale: 0.004),
              let alpha = function.resolvedAlpha() else { return nil }
        let rgb = CSSColorConversion.srgbFromOKLab(lightness: lightness.clamped(to: 0...1), aAxis: aAxis, bAxis: bAxis)
        return srgbColor(rgb, alpha: alpha).map { ColorCode(type: .cssOKLab, color: $0) }
    }

    static func oklch(_ function: Function) -> ColorCode? {
        guard !function.usesCommas,
              function.parameters.count == 3,
              let lightness = Value(function.parameters[0])?.resolved(numberScale: 1, percentageScale: 1 / 100),
              let chroma = Value(function.parameters[1])?.resolved(numberScale: 1, percentageScale: 0.004),
              let hue = Value(function.parameters[2])?.hueDegrees,
              let alpha = function.resolvedAlpha() else { return nil }
        let rgb = CSSColorConversion.srgbFromOKLCh(lightness: lightness.clamped(to: 0...1),
                                                   chroma: max(chroma, 0),
                                                   hue: normalizedHue(hue))
        return srgbColor(rgb, alpha: alpha).map { ColorCode(type: .cssOKLCh, color: $0) }
    }

    static func profiledColor(_ function: Function) -> ColorCode? {
        guard !function.usesCommas, function.parameters.count == 4 else { return nil }
        let components = function.parameters.dropFirst().compactMap {
            Value($0)?.resolved(numberScale: 1, percentageScale: 1 / 100)
        }
        guard components.count == 3, let alpha = function.resolvedAlpha() else { return nil }
        let channels = SIMD3(components[0], components[1], components[2])

        let color: NSColor?
        switch function.parameters[0].lowercased() {
        case "srgb":
            color = srgbColor(channels, alpha: alpha)
        case "srgb-linear":
            color = srgbColor(CSSColorConversion.srgbFromLinearSRGB(channels), alpha: alpha)
        case "display-p3":
            color = NSColor(displayP3Red: CGFloat(channels.x.clamped(to: 0...1)),
                            green: CGFloat(channels.y.clamped(to: 0...1)),
                            blue: CGFloat(channels.z.clamped(to: 0...1)),
                            alpha: CGFloat(alpha))
        case "a98-rgb":
            let components: [CGFloat] = [
                CGFloat(channels.x.clamped(to: 0...1)),
                CGFloat(channels.y.clamped(to: 0...1)),
                CGFloat(channels.z.clamped(to: 0...1)),
                CGFloat(alpha)
            ]
            color = NSColor(colorSpace: .adobeRGB1998, components: components, count: components.count)
        case "prophoto-rgb":
            color = srgbColor(CSSColorConversion.srgbFromProPhoto(channels), alpha: alpha)
        case "rec2020":
            color = srgbColor(CSSColorConversion.srgbFromRec2020(channels), alpha: alpha)
        case "xyz", "xyz-d65":
            color = srgbColor(CSSColorConversion.srgbFromXYZD65(channels), alpha: alpha)
        case "xyz-d50":
            color = srgbColor(CSSColorConversion.srgbFromXYZD50(channels), alpha: alpha)
        default:
            color = nil
        }

        guard let color else { return nil }
        return ColorCode(type: .cssColorFunction, color: color)
    }
}

// MARK: - Helpers
private extension CSSColorFunctionParser {
    static func srgbColor(_ rgb: SIMD3<Double>, alpha: Double) -> NSColor? {
        // Finite input can still overflow during matrix multiplication or exponentiation.
        // Clamping does not remove NaN; reject it before handing components to AppKit.
        guard rgb.x.isFinite, rgb.y.isFinite, rgb.z.isFinite, alpha.isFinite else { return nil }
        return NSColor(srgbRed: CGFloat(rgb.x.clamped(to: 0...1)),
                       green: CGFloat(rgb.y.clamped(to: 0...1)),
                       blue: CGFloat(rgb.z.clamped(to: 0...1)),
                       alpha: CGFloat(alpha))
    }

    static func normalizedHue(_ degrees: Double) -> Double {
        let remainder = degrees.truncatingRemainder(dividingBy: 360)
        return remainder < 0 ? remainder + 360 : remainder
    }
}

// MARK: - Function Syntax
private extension CSSColorFunctionParser {
    struct Function {
        let name: String
        let parameters: [String]
        let alphaParameter: String?
        let usesCommas: Bool

        init?(_ code: String) {
            guard code.hasSuffix(")"), let parenthesisIndex = code.firstIndex(of: "(") else { return nil }
            let name = code[..<parenthesisIndex].lowercased()
            guard !name.isEmpty, name.allSatisfy({ $0.isASCII && $0.isLetter }) else { return nil }

            let contents = code[code.index(after: parenthesisIndex)..<code.index(before: code.endIndex)]
            if contents.contains(",") {
                // Legacy comma syntax: rgb(255, 137, 66) / rgba(255, 137, 66, 0.5)
                let parts = contents.components(separatedBy: ",").map { $0.trimmingCharacters(in: .whitespaces) }
                guard (3...4).contains(parts.count),
                      parts.allSatisfy({ !$0.isEmpty && !$0.contains(where: \.isWhitespace) }) else { return nil }
                self.parameters = Array(parts.prefix(3))
                self.alphaParameter = parts.count == 4 ? parts[3] : nil
                self.usesCommas = true
            } else {
                // Modern space syntax: rgb(255 137 66 / 50%)
                let parts = contents.components(separatedBy: "/")
                guard (1...2).contains(parts.count) else { return nil }
                let parameters = parts[0].split(whereSeparator: \.isWhitespace).map(String.init)
                guard !parameters.isEmpty else { return nil }
                if parts.count == 2 {
                    let alphaTokens = parts[1].split(whereSeparator: \.isWhitespace)
                    guard alphaTokens.count == 1 else { return nil }
                    self.alphaParameter = String(alphaTokens[0])
                } else {
                    self.alphaParameter = nil
                }
                self.parameters = parameters
                self.usesCommas = false
            }
            self.name = name
        }

        /// Resolves all parameters as number-or-percentage components.
        func resolvedParameters(numberScale: Double, percentageScale: Double) -> SIMD3<Double>? {
            guard parameters.count == 3 else { return nil }
            let components = parameters.compactMap {
                Value($0)?.resolved(numberScale: numberScale, percentageScale: percentageScale)
            }
            guard components.count == 3 else { return nil }
            return SIMD3(components[0], components[1], components[2])
        }

        /// Resolves the alpha parameter, defaulting to fully opaque.
        func resolvedAlpha() -> Double? {
            guard let alphaParameter else { return 1 }
            guard let alpha = Value(alphaParameter)?.resolved(numberScale: 1, percentageScale: 1 / 100) else { return nil }
            return alpha.clamped(to: 0...1)
        }
    }
}

// MARK: - Component Values
private extension CSSColorFunctionParser {
    /// A single component of a CSS color function: `255`, `63%`, `120deg`, or `none`.
    enum Value {
        case number(Double)
        case percentage(Double)
        case angle(degrees: Double)
        case none

        init?(_ token: String) {
            let token = token.lowercased()
            if token == "none" {
                self = .none
                return
            }
            if token.hasSuffix("%"), let number = Self.number(from: token.dropLast()) {
                self = .percentage(number)
                return
            }
            // "grad" must precede "rad" because it also matches as a suffix.
            let angleUnits: [(unit: String, degrees: Double)] = [
                ("grad", 0.9), ("rad", 180 / Double.pi), ("deg", 1), ("turn", 360)
            ]
            for angleUnit in angleUnits where token.hasSuffix(angleUnit.unit) {
                guard let number = Self.number(from: token.dropLast(angleUnit.unit.count)) else { return nil }
                let degrees = number * angleUnit.degrees
                guard degrees.isFinite else { return nil }
                self = .angle(degrees: degrees)
                return
            }
            guard let number = Self.number(from: token[...]) else { return nil }
            self = .number(number)
        }

        /// Resolves a number-or-percentage component.
        /// e.g. an `rgb()` channel uses `numberScale: 1 / 255, percentageScale: 1 / 100`.
        func resolved(numberScale: Double, percentageScale: Double) -> Double? {
            let value: Double
            switch self {
            case .number(let number): value = number * numberScale
            case .percentage(let percentage): value = percentage * percentageScale
            case .none: return 0
            case .angle: return nil
            }
            return value.isFinite ? value : nil
        }

        /// Resolves a hue component, which accepts a plain number or an angle.
        var hueDegrees: Double? {
            switch self {
            case .number(let number): return number
            case .angle(let degrees): return degrees
            case .none: return 0
            case .percentage: return nil
            }
        }

        /// `Double.init(_:)` also accepts hex floats and "inf" / "nan";
        /// restrict input to the CSS `<number>` character set.
        private static func number(from text: Substring) -> Double? {
            guard !text.isEmpty, text.allSatisfy({ "0123456789+-.e".contains($0) }),
                  let number = Double(text), number.isFinite else { return nil }
            return number
        }
    }
}

private extension Double {
    func clamped(to range: ClosedRange<Double>) -> Double {
        Swift.min(Swift.max(self, range.lowerBound), range.upperBound)
    }
}
