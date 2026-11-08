import AppKit
import Testing

@testable import AlternativeMenu

struct AlternativeMenuSubmenuGeometryTests {
    @Test
    func alignsSubmenuToParentRowInBothLayoutDirections() {
        let bounds = NSRect(x: 6, y: 6, width: 988, height: 788)
        let parent = NSRect(x: 300, y: 200, width: 150, height: 150)
        let row = NSRect(x: 305, y: 280, width: 140, height: 24)
        let size = NSSize(width: 200, height: 120)

        #expect(
            AlternativeMenuSubmenuGeometry.frame(
                preferredSize: size,
                parentFrame: parent,
                rowFrame: row,
                bounds: bounds,
                prefersLeft: false
            ) == NSRect(x: 445, y: 189, width: 200, height: 120)
        )
        #expect(
            AlternativeMenuSubmenuGeometry.frame(
                preferredSize: size,
                parentFrame: parent,
                rowFrame: row,
                bounds: bounds,
                prefersLeft: true
            ) == NSRect(x: 105, y: 189, width: 200, height: 120)
        )
    }

    @Test
    func flipsAndClampsSubmenuAtScreenEdges() {
        let bounds = NSRect(x: 6, y: 6, width: 988, height: 788)
        let parent = NSRect(x: 830, y: 20, width: 150, height: 110)
        let row = NSRect(x: 835, y: 25, width: 140, height: 24)

        #expect(
            AlternativeMenuSubmenuGeometry.frame(
                preferredSize: NSSize(width: 200, height: 120),
                parentFrame: parent,
                rowFrame: row,
                bounds: bounds,
                prefersLeft: false
            ) == NSRect(x: 635, y: 6, width: 200, height: 120)
        )
    }

    @Test
    func oversizedSubmenuUsesWiderSideWithoutCoveringParent() {
        let bounds = NSRect(x: -994, y: 6, width: 988, height: 788)
        let parent = NSRect(x: -750, y: 100, width: 300, height: 200)
        let row = NSRect(x: -745, y: 130, width: 290, height: 24)

        #expect(
            AlternativeMenuSubmenuGeometry.frame(
                preferredSize: NSSize(width: 900, height: 1000),
                parentFrame: parent,
                rowFrame: row,
                bounds: bounds,
                prefersLeft: true
            ) == NSRect(x: -455, y: 6, width: 449, height: 788)
        )
    }

    @Test
    func recognizesDiagonalMotionWithTheNativeAngularMargin() {
        let child = NSRect(x: 200, y: 100, width: 100, height: 100)

        #expect(
            AlternativeMenuSubmenuGeometry.isMovingTowardSubmenu(
                from: NSPoint(x: 100, y: 150),
                to: NSPoint(x: 150, y: 192),
                submenuFrame: child,
                opensLeft: false
            )
        )
        #expect(
            AlternativeMenuSubmenuGeometry.isMovingTowardSubmenu(
                from: NSPoint(x: 400, y: 150),
                to: NSPoint(x: 350, y: 108),
                submenuFrame: child,
                opensLeft: true
            )
        )
    }

    @Test
    func rejectsStationaryWrongWayAndOutOfCorridorMotion() {
        let child = NSRect(x: 200, y: 100, width: 100, height: 100)
        let start = NSPoint(x: 100, y: 150)

        #expect(
            !AlternativeMenuSubmenuGeometry.isMovingTowardSubmenu(
                from: start,
                to: start,
                submenuFrame: child,
                opensLeft: false
            )
        )
        #expect(
            !AlternativeMenuSubmenuGeometry.isMovingTowardSubmenu(
                from: start,
                to: NSPoint(x: 90, y: 150),
                submenuFrame: child,
                opensLeft: false
            )
        )
        #expect(
            !AlternativeMenuSubmenuGeometry.isMovingTowardSubmenu(
                from: start,
                to: NSPoint(x: 150, y: 198),
                submenuFrame: child,
                opensLeft: false
            )
        )
        #expect(
            !AlternativeMenuSubmenuGeometry.isMovingTowardSubmenu(
                from: start,
                to: NSPoint(x: 210, y: 150),
                submenuFrame: child,
                opensLeft: false
            )
        )
    }
}
