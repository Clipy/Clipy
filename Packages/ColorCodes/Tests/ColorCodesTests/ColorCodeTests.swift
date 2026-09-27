//
//  ColorCodeTests.swift
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
import ColorCodes
import Testing

@Suite
struct ColorCodeTests {

    // MARK: - Hex
    @Test
    func parsesHex() {
        expectSRGB("#ff8942", 1, 137 / 255, 66 / 255, type: .hex)
        expectSRGB("#FF8942", 1, 137 / 255, 66 / 255, type: .hex)
        expectSRGB("#ff894280", 1, 137 / 255, 66 / 255, alpha: 128 / 255, type: .hexWithAlpha)
        expectSRGB("#f84", 1, 136 / 255, 68 / 255, type: .shortHex)
        expectSRGB("#f848", 1, 136 / 255, 68 / 255, alpha: 136 / 255, type: .shortHexWithAlpha)
    }

    @Test(arguments: ["#", "#f", "#ff", "#ff894", "#ff89422", "#ff8942801", "#ggg", "#ff 89 42"])
    func rejectsInvalidHex(code: String) {
        #expect(ColorCode(code) == nil)
    }

    /// Bare hex strings without `#` must not be detected by default: everyday
    /// words such as "decade" and plain numbers would otherwise show up as colors.
    @Test(arguments: ["ff8942", "decade", "facade", "beef", "cafe", "123456"])
    func rejectsBareHexStringsByDefault(code: String) {
        #expect(ColorCode(code) == nil)
    }

    @Test
    func parsesBareHexWhenAllowed() {
        expectSRGB("ff8942", 1, 137 / 255, 66 / 255, type: .hex, options: [.bareHex])
        expectSRGB("FF894280", 1, 137 / 255, 66 / 255, alpha: 128 / 255, type: .hexWithAlpha, options: [.bareHex])
        expectSRGB("f84", 1, 136 / 255, 68 / 255, type: .shortHex, options: [.bareHex])
        expectSRGB("beef", 187 / 255, 238 / 255, 238 / 255, type: .shortHexWithAlpha, options: [.bareHex])
        expectSRGB("123456", 0x12 / 255, 0x34 / 255, 0x56 / 255, type: .hex, options: [.bareHex])
        #expect(ColorCode("ggg", options: [.bareHex]) == nil)
        #expect(ColorCode("ff894", options: [.bareHex]) == nil)
        #expect(ColorCode("tomato", options: [.bareHex]) == nil)
    }

    @Test
    func combinesParseOptions() {
        expectSRGB("tomato", 1, 99 / 255, 71 / 255, type: .cssKeyword, options: [.cssKeyword, .bareHex])
        expectSRGB("beef", 187 / 255, 238 / 255, 238 / 255, type: .shortHexWithAlpha, options: [.cssKeyword, .bareHex])
    }

    // MARK: - Detection Boundaries
    @Test
    func trimsSurroundingWhitespace() {
        expectSRGB("  #ff8942\n", 1, 137 / 255, 66 / 255, type: .hex)
        expectSRGB("\t rgb(255 137 66) ", 1, 137 / 255, 66 / 255, type: .cssRGB)
    }

    @Test(arguments: ["", "color: #fff;", "background: rgb(0, 0, 0)", "rgb", "rgb()", "url(#ff8942)"])
    func rejectsNonColorStrings(code: String) {
        #expect(ColorCode(code) == nil)
    }

    @Test
    func rejectsTrailingContentAfterWhitespace() {
        let code = "rgb(255 137 66)" + String(repeating: " ", count: 200) + "!"
        #expect(ColorCode(code) == nil)
    }

    @Test(arguments: [129, 4096])
    func parsesLongColorStrings(length: Int) {
        let padding = String(repeating: " ", count: length - "rgb(255 137 66)".count)
        expectSRGB("rgb(\(padding)255 137 66)", 1, 137 / 255, 66 / 255, type: .cssRGB)
    }

    // MARK: - Invalid Numbers and Conversion Safety
    @Test(arguments: [
        "rgba(255,255,255,.)", "hsla(0,100%,50%,.)", "rgb(.,0,0)",
        "rgb(1..0 0 0)", "rgb(1e 0 0)", "rgb(nan 0 0)", "rgb(inf 0 0)",
        "rgb(0 0 0 / nan)", "rgb(0x1p2 0 0)", "rgb(１ 0 0)"
    ])
    func rejectsMalformedNumbersWithoutTrapping(code: String) {
        #expect(ColorCode(code) == nil)
    }

    @Test(arguments: [
        "rgb(1e999 0 0)", "rgb(-1e999 0 0)", "rgb(1e999% 0 0)",
        "rgb(0 0 0 / 1e999)", "hsl(1e999 100% 50%)", "hsl(1e308turn 100% 50%)",
        "hwb(1e308rad 0% 0%)", "lab(50 1.7e308% 0)", "lch(50 1.7e308% 0)",
        "color(display-p3 1e999 0 0)", "color(a98-rgb 0 0 0 / 1e999)"
    ])
    func rejectsOverflowingNumbersAndUnits(code: String) {
        #expect(ColorCode(code) == nil)
    }

    @Test(arguments: [
        "lab(50 1e308 1e308)", "lab(50 1e308% 0)", "lch(50 1e308 0)",
        "oklab(0.5 1e200 1e200)", "oklch(0.5 1e200 0)",
        "color(prophoto-rgb 1e308 0 0)", "color(rec2020 1e308 0 0)",
        "color(xyz-d50 1e308 1e308 1e308)"
    ])
    func rejectsOverflowDuringColorConversion(code: String) {
        #expect(ColorCode(code) == nil)
    }

    @Test(arguments: ["lch(50 20", "oklch(0.5 0.1"])
    func normalizesLargeLChHueBeforeTrigonometry(prefix: String) throws {
        let positive = try #require(ColorCode("\(prefix) 1e308)"))
        let negative = try #require(ColorCode("\(prefix) -1e308)"))
        #expect(positive == (try #require(ColorCode("\(prefix) 296)"))))
        #expect(negative == (try #require(ColorCode("\(prefix) 64)"))))
    }

    @Test
    func acceptedNumericBoundaryInputsHaveFiniteComponents() {
        let values = ["none", "0", "-0", "1e-999", "1e-308", "-1e308", "1e100", "1e308", "1e999", ".", "nan"]
        let functions = ["rgb", "hsl", "hwb", "lab", "lch", "oklab", "oklch", "color"]
        for function in functions {
            for value in values {
                for index in 0..<4 {
                    var components = ["0.5", "0.5", "0.5", "1"]
                    components[index] = value
                    let profile = function == "color" ? "xyz-d50 " : ""
                    let code = "\(function)(\(profile)\(components[0]) \(components[1]) \(components[2]) / \(components[3]))"
                    guard let color = ColorCode(code)?.color else { continue }
                    for component in [color.redComponent, color.greenComponent, color.blueComponent, color.alphaComponent] {
                        #expect(component.isFinite, "Non-finite component for \(code)")
                        #expect((0...1).contains(component), "Out-of-range component for \(code)")
                    }
                }
            }
        }
    }

    // MARK: - rgb() / rgba()
    @Test
    func parsesRGB() {
        expectSRGB("rgb(255, 137, 66)", 1, 137 / 255, 66 / 255, type: .cssRGB)
        expectSRGB("rgba(255, 137, 66, 0.5)", 1, 137 / 255, 66 / 255, alpha: 0.5, type: .cssRGB)
        expectSRGB("rgba(255, 0, 0, 50%)", 1, 0, 0, alpha: 0.5, type: .cssRGB)
        expectSRGB("rgb(255 137 66)", 1, 137 / 255, 66 / 255, type: .cssRGB)
        expectSRGB("rgb(255 137 66 / 50%)", 1, 137 / 255, 66 / 255, alpha: 0.5, type: .cssRGB)
        expectSRGB("rgb(255 137 66/.5)", 1, 137 / 255, 66 / 255, alpha: 0.5, type: .cssRGB)
        expectSRGB("RGB(100% 50% 0%)", 1, 0.5, 0, type: .cssRGB)
        expectSRGB("rgb(2.55e2 0 0)", 1, 0, 0, type: .cssRGB)
        expectSRGB("rgb(none none none)", 0, 0, 0, type: .cssRGB)
    }

    @Test
    func clampsOutOfRangeRGB() {
        expectSRGB("rgb(300, -20, 66)", 1, 0, 66 / 255)
        expectSRGB("rgb(255 0 0 / 1.5)", 1, 0, 0, alpha: 1)
    }

    @Test(arguments: [
        "rgb(255, 137 66)", "rgb(255 137)", "rgb(255, 137)", "rgb(255 137 66 / 1 2)",
        "rgb(one two three)", "rgb(255, 137, 66, 1, 0)", "rgb(0xff 0 0)", "rgb(inf 0 0)", "rgb(255 137 66 /)"
    ])
    func rejectsInvalidRGB(code: String) {
        #expect(ColorCode(code) == nil)
    }

    // MARK: - hsl() / hsla()
    @Test
    func parsesHSL() {
        // WFColorCode previously generated NaN from black through division by zero.
        expectSRGB("hsl(0,0%,0%)", 0, 0, 0, type: .cssHSL)
        expectSRGB("hsl(360 100% 0%)", 0, 0, 0, type: .cssHSL)
        expectSRGB("hsl(0, 100%, 50%)", 1, 0, 0, type: .cssHSL)
        expectSRGB("hsl(120deg 100% 25%)", 0, 0.5, 0, type: .cssHSL)
        expectSRGB("hsl(0.5turn 100% 50%)", 0, 1, 1, type: .cssHSL)
        expectSRGB("hsl(200grad 100% 50%)", 0, 1, 1, type: .cssHSL)
        expectSRGB("hsl(3.141592653589793rad 100% 50%)", 0, 1, 1, type: .cssHSL)
        expectSRGB("hsl(-120, 100%, 50%)", 0, 0, 1, type: .cssHSL)
        expectSRGB("hsl(0 100 50)", 1, 0, 0, type: .cssHSL)
        expectSRGB("hsla(240, 100%, 50%, 0.3)", 0, 0, 1, alpha: 0.3, type: .cssHSL)
    }

    // MARK: - hwb()
    @Test
    func parsesHWB() {
        expectSRGB("hwb(0 0% 0%)", 1, 0, 0, type: .cssHWB)
        expectSRGB("hwb(120 30% 20%)", 0.3, 0.8, 0.3, type: .cssHWB)
        expectSRGB("hwb(0 60% 60%)", 0.5, 0.5, 0.5, type: .cssHWB)
    }

    @Test
    func rejectsCommaSyntaxHWB() {
        #expect(ColorCode("hwb(0, 0%, 0%)") == nil)
    }

    // MARK: - lab() / lch()
    @Test
    func parsesLab() {
        expectSRGB("lab(54.29% 80.81 69.89)", 1, 0, 0, type: .cssLab)
        expectSRGB("lab(29.2345% 39.3825 20.0664)", 0.490634, 0.138671, 0.159006, type: .cssLab)
        // 100% of the a / b axes represents ±125
        expectSRGB("lab(29.2345% 31.506% 16.05312%)", 0.490634, 0.138671, 0.159006, type: .cssLab)
    }

    @Test
    func parsesLCh() {
        expectSRGB("lch(29.2345% 44.2 27)", 0.490634, 0.138671, 0.159006, type: .cssLCh)
        // 100% of chroma represents 150
        expectSRGB("lch(29.2345% 29.4667% 27deg)", 0.490634, 0.138671, 0.159006, type: .cssLCh)
    }

    @Test(arguments: ["lab(29, 39, 20)", "lch(29, 44, 27)"])
    func rejectsCommaSyntaxLab(code: String) {
        #expect(ColorCode(code) == nil)
    }

    // MARK: - oklab() / oklch()
    @Test
    func parsesOKLab() {
        expectSRGB("oklab(0.627955 0.224863 0.125846)", 1, 0, 0, type: .cssOKLab)
        expectSRGB("oklab(62.7955% 0.224863 0.125846)", 1, 0, 0, type: .cssOKLab)
    }

    @Test
    func parsesOKLCh() {
        expectSRGB("oklch(0.627955 0.257683 29.2339)", 1, 0, 0, type: .cssOKLCh)
        // Tailwind CSS v4 default palette (orange-500); red is out of gamut and clips to 1
        expectSRGB("oklch(0.705 0.213 47.604)", 1, 0.410734, 0, type: .cssOKLCh)
        // 100% of chroma represents 0.4
        expectSRGB("oklch(70.5% 53.25% 47.604)", 1, 0.410734, 0, type: .cssOKLCh)
        expectSRGB("oklch(none none none)", 0, 0, 0, type: .cssOKLCh)
    }

    // MARK: - color()
    @Test
    func parsesColorFunction() {
        expectSRGB("color(srgb 1 0.5 0)", 1, 0.5, 0, type: .cssColorFunction)
        expectSRGB("color(srgb 100% 50% 0%)", 1, 0.5, 0, type: .cssColorFunction)
        expectSRGB("color(srgb 1 0 0 / 0.25)", 1, 0, 0, alpha: 0.25, type: .cssColorFunction)
        expectSRGB("color(srgb-linear 0.5 0.5 0.5)", 0.735357, 0.735357, 0.735357, type: .cssColorFunction)
        expectSRGB("color(prophoto-rgb 0.5 0.5 0.5)", 0.572307, 0.572307, 0.572307, type: .cssColorFunction)
        expectSRGB("color(rec2020 1 0 0)", 1, 0, 0, type: .cssColorFunction)
        expectSRGB("color(xyz 0.41239079926595934 0.21263900587151027 0.01933081871559182)", 1, 0, 0, type: .cssColorFunction)
        expectSRGB("color(xyz-d65 0.41239079926595934 0.21263900587151027 0.01933081871559182)", 1, 0, 0, type: .cssColorFunction)
        expectSRGB("color(xyz-d50 0.9642956764295677 1 0.8251046025104602)", 1, 1, 1, type: .cssColorFunction)
    }

    @Test
    func parsesDisplayP3InNativeColorSpace() throws {
        let colorCode = try #require(ColorCode("color(display-p3 1 0 0)"))
        #expect(colorCode.type == .cssColorFunction)
        let converted = try #require(colorCode.color.usingColorSpace(.displayP3))
        #expect(abs(converted.redComponent - 1) <= 0.001)
        #expect(abs(converted.greenComponent) <= 0.001)
        #expect(abs(converted.blueComponent) <= 0.001)
    }

    @Test
    func parsesA98RGBInNativeColorSpace() throws {
        let colorCode = try #require(ColorCode("color(a98-rgb 1 0.5 0)"))
        #expect(colorCode.type == .cssColorFunction)
        let converted = try #require(colorCode.color.usingColorSpace(.adobeRGB1998))
        #expect(abs(converted.redComponent - 1) <= 0.001)
        #expect(abs(converted.greenComponent - 0.5) <= 0.001)
        #expect(abs(converted.blueComponent) <= 0.001)
    }

    @Test(arguments: ["color(rec709 1 0 0)", "color(srgb, 1, 0, 0)", "color(srgb 1 0)", "color(srgb 1 0 0 0)"])
    func rejectsInvalidColorFunction(code: String) {
        #expect(ColorCode(code) == nil)
    }

    // MARK: - CSS Keywords
    @Test(arguments: ["tomato", "RebeccaPurple", "transparent"])
    func rejectsKeywordsByDefault(code: String) {
        #expect(ColorCode(code) == nil)
    }

    @Test
    func parsesKeywordsWhenAllowed() {
        expectSRGB("tomato", 1, 99 / 255, 71 / 255, type: .cssKeyword, options: [.cssKeyword])
        expectSRGB("RebeccaPurple", 102 / 255, 51 / 255, 153 / 255, type: .cssKeyword, options: [.cssKeyword])
        expectSRGB("transparent", 0, 0, 0, alpha: 0, type: .cssKeyword, options: [.cssKeyword])
        #expect(ColorCode("notacolor", options: [.cssKeyword]) == nil)
    }

    // MARK: - Helpers
    private func expectSRGB(_ code: String,
                            _ red: Double, _ green: Double, _ blue: Double, alpha: Double = 1,
                            type: ColorCode.CodeType? = nil,
                            options: ColorCode.ParseOptions = [],
                            accuracy: Double = 0.001,
                            sourceLocation: SourceLocation = #_sourceLocation) {
        guard let colorCode = ColorCode(code, options: options) else {
            Issue.record("Failed to parse \(code)", sourceLocation: sourceLocation)
            return
        }
        if let type {
            #expect(colorCode.type == type, sourceLocation: sourceLocation)
        }
        guard let converted = colorCode.color.usingColorSpace(.sRGB) else {
            Issue.record("Failed to convert \(code) to sRGB", sourceLocation: sourceLocation)
            return
        }
        #expect(abs(Double(converted.redComponent) - red) <= accuracy,
                "red is \(converted.redComponent), expected \(red) for \(code)", sourceLocation: sourceLocation)
        #expect(abs(Double(converted.greenComponent) - green) <= accuracy,
                "green is \(converted.greenComponent), expected \(green) for \(code)", sourceLocation: sourceLocation)
        #expect(abs(Double(converted.blueComponent) - blue) <= accuracy,
                "blue is \(converted.blueComponent), expected \(blue) for \(code)", sourceLocation: sourceLocation)
        #expect(abs(Double(converted.alphaComponent) - alpha) <= accuracy,
                "alpha is \(converted.alphaComponent), expected \(alpha) for \(code)", sourceLocation: sourceLocation)
    }
}
