import Foundation
import Testing

@testable import AlternativeMenu

struct AlternativeMenuScrollAvailabilityTests {
    @Test(arguments: [CGFloat(0), 0.5, 5])
    func omitsScrollArrowsWhenOnlyBottomPaddingIsClipped(_ clippedPadding: CGFloat) {
        let contentHeight = 100 + AlternativeMenuStyle.current.padding
        let availability = AlternativeMenuScrollAvailability(
            offset: 0,
            contentHeight: contentHeight,
            viewportHeight: contentHeight - clippedPadding,
            topPadding: AlternativeMenuStyle.current.padding
        )

        #expect(!availability.canScrollUp)
        #expect(!availability.canScrollDown)
        #expect(availability.bottomInset == 0)
    }

    @Test
    func keepsTheDownArrowWhenActualRowsOverflow() {
        let availability = AlternativeMenuScrollAvailability(
            offset: 0,
            contentHeight: 100 + AlternativeMenuStyle.current.padding,
            viewportHeight: 98,
            topPadding: AlternativeMenuStyle.current.padding
        )

        #expect(availability.canScrollDown)
        #expect(availability.bottomInset == AlternativeMenuStyle.current.minimumRowHeight)
    }
}
