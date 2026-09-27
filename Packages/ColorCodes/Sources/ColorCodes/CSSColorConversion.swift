//
//  CSSColorConversion.swift
//
//  ColorCodes
//  GitHub: https://github.com/clipy
//  HP: https://clipy-app.com
//
//  Created by Shunsuke Furubayashi on 2026/07/05.
//
//  Copyright © 2015-2026 Clipy Project.
//

import Foundation
import simd

/// Color space conversion math for CSS colors.
///
/// All functions return gamma-encoded sRGB components. Out-of-gamut colors can
/// produce components outside `0...1`; clamping is left to the caller.
/// Matrices and constants are taken from the CSS Color Module Level 4
/// sample code: https://drafts.csswg.org/css-color-4/#color-conversion-code
enum CSSColorConversion {

    // MARK: - Cylindrical sRGB spaces
    /// - Parameters:
    ///   - hue: degrees in `0..<360`
    ///   - saturation: `0...1`
    ///   - lightness: `0...1`
    static func srgbFromHSL(hue: Double, saturation: Double, lightness: Double) -> SIMD3<Double> {
        func channel(_ offset: Double) -> Double {
            let position = (offset + hue / 30).truncatingRemainder(dividingBy: 12)
            let amplitude = saturation * min(lightness, 1 - lightness)
            return lightness - amplitude * max(-1, min(position - 3, 9 - position, 1))
        }
        return SIMD3(channel(0), channel(8), channel(4))
    }

    /// - Parameters:
    ///   - hue: degrees in `0..<360`
    ///   - whiteness: `0...1`
    ///   - blackness: `0...1`
    static func srgbFromHWB(hue: Double, whiteness: Double, blackness: Double) -> SIMD3<Double> {
        guard whiteness + blackness < 1 else {
            return SIMD3(repeating: whiteness / (whiteness + blackness))
        }
        let rgb = srgbFromHSL(hue: hue, saturation: 1, lightness: 0.5)
        return rgb * (1 - whiteness - blackness) + SIMD3(repeating: whiteness)
    }

    // MARK: - CIE Lab / LCh (D50-based)
    /// - Parameters:
    ///   - lightness: `0...100`
    ///   - aAxis: green-red axis, unbounded (±125 covers `±100%`)
    ///   - bAxis: blue-yellow axis, unbounded
    static func srgbFromLab(lightness: Double, aAxis: Double, bAxis: Double) -> SIMD3<Double> {
        srgbFromXYZD50(xyzD50FromLab(lightness: lightness, aAxis: aAxis, bAxis: bAxis))
    }

    static func srgbFromLCh(lightness: Double, chroma: Double, hue: Double) -> SIMD3<Double> {
        let radians = hue * .pi / 180
        return srgbFromLab(lightness: lightness, aAxis: chroma * cos(radians), bAxis: chroma * sin(radians))
    }

    // MARK: - OKLab / OKLCh (D65-based)
    /// - Parameters:
    ///   - lightness: `0...1`
    ///   - aAxis: green-red axis, unbounded (±0.4 covers `±100%`)
    ///   - bAxis: blue-yellow axis, unbounded
    static func srgbFromOKLab(lightness: Double, aAxis: Double, bAxis: Double) -> SIMD3<Double> {
        let lms = oklabToLMS * SIMD3(lightness, aAxis, bAxis)
        return srgbFromXYZD65(lmsToXYZD65 * (lms * lms * lms))
    }

    static func srgbFromOKLCh(lightness: Double, chroma: Double, hue: Double) -> SIMD3<Double> {
        let radians = hue * .pi / 180
        return srgbFromOKLab(lightness: lightness, aAxis: chroma * cos(radians), bAxis: chroma * sin(radians))
    }

    // MARK: - color() spaces
    static func srgbFromLinearSRGB(_ linear: SIMD3<Double>) -> SIMD3<Double> {
        gammaEncoded(linear)
    }

    static func srgbFromProPhoto(_ prophoto: SIMD3<Double>) -> SIMD3<Double> {
        // Transfer curve is gamma 1.8 with a small linear portion.
        let linearThreshold = 16.0 / 512
        let linear = prophoto.mapComponents { value in
            abs(value) <= linearThreshold ? value / 16 : copysign(pow(abs(value), 1.8), value)
        }
        return srgbFromXYZD50(linearProPhotoToXYZD50 * linear)
    }

    static func srgbFromRec2020(_ rec2020: SIMD3<Double>) -> SIMD3<Double> {
        // Reference EOTF from Rec. ITU-R BT.1886: a pure 2.4 power function.
        let linear = rec2020.mapComponents { copysign(pow(abs($0), 2.4), $0) }
        return srgbFromXYZD65(linearRec2020ToXYZD65 * linear)
    }

    static func srgbFromXYZD65(_ xyz: SIMD3<Double>) -> SIMD3<Double> {
        gammaEncoded(xyzD65ToLinearSRGB * xyz)
    }

    static func srgbFromXYZD50(_ xyz: SIMD3<Double>) -> SIMD3<Double> {
        srgbFromXYZD65(xyzD50ToD65 * xyz)
    }
}

// MARK: - Private
private extension CSSColorConversion {
    static func xyzD50FromLab(lightness: Double, aAxis: Double, bAxis: Double) -> SIMD3<Double> {
        let kappa = 24389.0 / 27    // 29^3 / 3^3
        let epsilon = 216.0 / 24389 // 6^3 / 29^3
        let factorY = (lightness + 16) / 116
        let factorX = aAxis / 500 + factorY
        let factorZ = factorY - bAxis / 200
        let xyz = SIMD3(
            pow(factorX, 3) > epsilon ? pow(factorX, 3) : (116 * factorX - 16) / kappa,
            lightness > kappa * epsilon ? pow(factorY, 3) : lightness / kappa,
            pow(factorZ, 3) > epsilon ? pow(factorZ, 3) : (116 * factorZ - 16) / kappa
        )
        return xyz * d50White
    }

    /// Converts linear-light sRGB to gamma-encoded form.
    /// Negative values mirror the curve through the origin (extended transfer function).
    static func gammaEncoded(_ linear: SIMD3<Double>) -> SIMD3<Double> {
        linear.mapComponents { value in
            abs(value) > 0.0031308 ? copysign(1.055 * pow(abs(value), 1 / 2.4) - 0.055, value) : 12.92 * value
        }
    }

    // MARK: Constants
    static let d50White = SIMD3<Double>(0.3457 / 0.3585, 1, (1.0 - 0.3457 - 0.3585) / 0.3585)

    static let xyzD65ToLinearSRGB = simd_double3x3(rows: [
        SIMD3(12831.0 / 3959, -329.0 / 214, -1974.0 / 3959),
        SIMD3(-851781.0 / 878810, 1648619.0 / 878810, 36519.0 / 878810),
        SIMD3(705.0 / 12673, -2585.0 / 12673, 705.0 / 667)
    ])

    /// Bradford chromatic adaptation from D50 to D65.
    static let xyzD50ToD65 = simd_double3x3(rows: [
        SIMD3(0.955473421488075, -0.02309845494876471, 0.06325924320057072),
        SIMD3(-0.0283697093338637, 1.0099953980813041, 0.021041441191917323),
        SIMD3(0.012314014864481998, -0.020507649298898964, 1.330365926242124)
    ])

    static let linearProPhotoToXYZD50 = simd_double3x3(rows: [
        SIMD3(0.79776664490064230, 0.13518129740053308, 0.03134773412839220),
        SIMD3(0.28807482881940130, 0.71183523424187300, 0.00008993693872564),
        SIMD3(0, 0, 0.82510460251046020)
    ])

    static let linearRec2020ToXYZD65 = simd_double3x3(rows: [
        SIMD3(63426534.0 / 99577255, 20160776.0 / 139408157, 47086771.0 / 278816314),
        SIMD3(26158966.0 / 99577255, 472592308.0 / 697040785, 8267143.0 / 139408157),
        SIMD3(0, 19567812.0 / 697040785, 295819943.0 / 278816314)
    ])

    static let oklabToLMS = simd_double3x3(rows: [
        SIMD3(1, 0.3963377773761749, 0.2158037573099136),
        SIMD3(1, -0.1055613458156586, -0.0638541728258133),
        SIMD3(1, -0.0894841775298119, -1.2914855480194092)
    ])

    static let lmsToXYZD65 = simd_double3x3(rows: [
        SIMD3(1.2268798758459243, -0.5578149944602171, 0.2813910456659647),
        SIMD3(-0.0405757452148008, 1.1122868032803170, -0.0717110580655164),
        SIMD3(-0.0763729366746601, -0.4214933324022432, 1.5869240198367816)
    ])
}

private extension SIMD3<Double> {
    func mapComponents(_ transform: (Double) -> Double) -> SIMD3<Double> {
        SIMD3(transform(x), transform(y), transform(z))
    }
}
