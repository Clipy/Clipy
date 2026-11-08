//
//  AlternativeMenuSubmenuGeometry.swift
//  AlternativeMenu
//
//  Copyright © 2015-2026 Clipy Project.
//

import AppKit

enum AlternativeMenuSubmenuGeometry {
    static func frame(
        preferredSize: NSSize,
        parentFrame: NSRect,
        rowFrame: NSRect,
        bounds: NSRect,
        prefersLeft: Bool
    ) -> NSRect {
        // The menu edges overlap by the same amount as the top row inset.
        let inset = AlternativeMenuStyle.padding
        let leftEdge = min(parentFrame.minX + inset, bounds.maxX)
        let rightEdge = max(parentFrame.maxX - inset, bounds.minX)
        let leftSpace = max(0, leftEdge - bounds.minX)
        let rightSpace = max(0, bounds.maxX - rightEdge)
        let preferredWidth = max(0, preferredSize.width)

        // Keep the locale's preferred direction when it fits. If neither side
        // fits, truncate in the larger space instead of covering the parent.
        let preferredSpace = prefersLeft ? leftSpace : rightSpace
        let oppositeSpace = prefersLeft ? rightSpace : leftSpace
        let usesPreferredSide = preferredSpace >= preferredWidth || preferredSpace >= oppositeSpace
        let opensLeft = usesPreferredSide ? prefersLeft : !prefersLeft

        let width = min(preferredWidth, opensLeft ? leftSpace : rightSpace)
        let height = min(max(0, preferredSize.height), bounds.height)
        let originY = min(max(rowFrame.maxY + inset - height, bounds.minY), bounds.maxY - height)
        return NSRect(x: opensLeft ? leftEdge - width : rightEdge, y: originY, width: width, height: height)
    }

    static func isMovingTowardSubmenu(
        from start: NSPoint,
        to point: NSPoint,
        submenuFrame: NSRect,
        opensLeft: Bool
    ) -> Bool {
        let direction: CGFloat = opensLeft ? -1 : 1
        let nearEdge = opensLeft ? submenuFrame.maxX : submenuFrame.minX
        let edgeDistance = (nearEdge - start.x) * direction
        let advance = (point.x - start.x) * direction
        guard !submenuFrame.isEmpty, advance > 0, advance <= edgeDistance else { return false }

        // Allow the same 15-degree margin around the submenu edge as NSMenu.
        let margin = CGFloat.pi / 12
        let motion = atan2(point.y - start.y, advance)
        let lower = atan2(submenuFrame.minY - start.y, edgeDistance) - margin
        let upper = atan2(submenuFrame.maxY - start.y, edgeDistance) + margin
        return motion >= lower && motion <= upper
    }
}
