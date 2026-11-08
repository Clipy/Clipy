import AppKit
import Testing
@testable import AlternativeMenu

@MainActor
struct NSImageExtensionsTests {
    @Test(arguments: [false, true])
    func identifiesSymbolsWithAndWithoutAnExplicitScale(hasSmallScale: Bool) throws {
        let image = try #require(NSImage(systemSymbolName: "doc", accessibilityDescription: nil))
        let candidate = hasSmallScale ? try #require(image.withSymbolConfiguration(.init(scale: .small))) : image

        #expect(candidate.isSymbolImage)
    }

    @Test(arguments: [false, true])
    func doesNotIdentifyBitmapsAsSymbols(isTemplate: Bool) throws {
        let representation = try #require(NSBitmapImageRep(
            bitmapDataPlanes: nil, pixelsWide: 16, pixelsHigh: 16, bitsPerSample: 8,
            samplesPerPixel: 4, hasAlpha: true, isPlanar: false, colorSpaceName: .deviceRGB,
            bytesPerRow: 0, bitsPerPixel: 0
        ))
        let image = NSImage(size: representation.size)
        image.addRepresentation(representation)
        image.isTemplate = isTemplate

        #expect(!image.isSymbolImage)
    }
}
