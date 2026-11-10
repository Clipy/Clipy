//
//  AlternativeMenuViewState.swift
//  AlternativeMenu
//
//  Copyright © 2015-2026 Clipy Project.
//

import AppKit
import Observation

/// Shares menu presentation state and derived layout values between AppKit and SwiftUI.
@MainActor
@Observable
final class AlternativeMenuViewState {
    private static let maximumBitmapToSymbolWidthRatio: CGFloat = 1.5
    private static let maximumBitmapWidthDifference: CGFloat = 5
    private let minimumSymbolColumnWidth: CGFloat
    private(set) var rows: [AlternativeMenuRowData] = []
    private(set) var keyColumnWidth: CGFloat = 0
    private(set) var modifierColumnWidth: CGFloat = 0
    private(set) var trailingColumnWidth: CGFloat = 0
    private(set) var iconWidths: [CGFloat] = []
    private(set) var contentWidth: CGFloat = 0
    var selection: Int?
    var isSelectionEmphasized = true
    var hidesSelection = false
    var scrollAvailability = AlternativeMenuScrollAvailability(offset: 0, contentHeight: 0, viewportHeight: 0, topPadding: 0)
    @ObservationIgnored
    weak var scrollView: NSScrollView?

    var highlightedRowIndex: Int? {
        hidesSelection ? nil : selection
    }
    var highlightedRow: AlternativeMenuRowData? {
        guard let index = highlightedRowIndex,
            rows.indices.contains(index)
        else {
            return nil
        }
        return rows[index]
    }
    var contentHeight: CGFloat {
        (rows.last?.verticalRange.upperBound ?? 0) + AlternativeMenuStyle.current.padding
    }
    var offset: CGFloat {
        guard let scrollView, let document = scrollView.documentView else {
            return 0
        }
        let visible = scrollView.documentVisibleRect
        return max(0, document.isFlipped ? visible.minY - document.bounds.minY : document.bounds.maxY - visible.maxY)
    }

    init(minimumSymbolColumnWidth: CGFloat = 0) {
        precondition(
            minimumSymbolColumnWidth.isFinite && minimumSymbolColumnWidth >= 0,
            "The symbol column minimum width must be finite and nonnegative."
        )
        self.minimumSymbolColumnWidth = minimumSymbolColumnWidth
    }

    func setRows(_ rows: [AlternativeMenuRowData]) {
        let keyColumnWidth = rows.lazy.map(\.keyEquivalent).filter(\.usesSharedKeyColumn).map(\.keyWidth).max() ?? 0
        let modifierColumnWidth = rows.lazy.map(\.keyEquivalent.modifierColumnWidth).max() ?? 0
        let trailingColumnWidth = rows.lazy.map(\.trailingWidth).max() ?? 0
        var imageColumnWidth = rows.lazy.filter(\.isSymbolImage).map(\.imageLayoutSize.width).max() ?? 0
        let bitmapWidths = rows.lazy.filter { !$0.isSymbolImage }.compactMap { $0.image?.size.width }
        if let maximum = bitmapWidths.max(), let minimum = bitmapWidths.min(),
            maximum <= imageColumnWidth * Self.maximumBitmapToSymbolWidthRatio,
            maximum - minimum <= Self.maximumBitmapWidthDifference {
            // Native menus share similarly sized bitmaps with symbols, but leave larger thumbnails independent.
            imageColumnWidth = max(imageColumnWidth, maximum)
        }
        let symbolColumnWidth = max(minimumSymbolColumnWidth, imageColumnWidth)
        let measuredSymbolColumnWidth: CGFloat
        if #available(macOS 27, *), !rows.contains(where: { $0.kind != .header && $0.isSymbolImage }) {
            // A symbol confined to section headers no longer widens imageless rows.
            measuredSymbolColumnWidth = minimumSymbolColumnWidth
        } else {
            measuredSymbolColumnWidth = symbolColumnWidth
        }
        var iconWidths = Array(repeating: CGFloat.zero, count: rows.count)
        var start = 0
        for end in rows.indices where rows[end].kind == .separator || rows[end].kind == .header || end == rows.count - 1 {
            // A header ends the preceding group; the next row starts a new one.
            let group = start...end
            let indentsImagelessRows: Bool
            if #available(macOS 27, *) {
                // Native menus require a strict majority of symbol-bearing items in each group.
                // Disabled items count; headers and separators do not.
                let items = rows[group].filter { $0.kind != .header && $0.kind != .separator }
                indentsImagelessRows = items.filter(\.isSymbolImage).count > items.count / 2
            } else {
                indentsImagelessRows = group.contains { rows[$0].isSymbolImage }
            }
            for index in group {
                // Symbols always share the widest column, even when imageless rows stay unindented.
                let width = rows[index].image == nil && !indentsImagelessRows ? 0 : symbolColumnWidth
                iconWidths[index] = rows[index].iconWidth(symbolColumnWidth: width, bitmapColumnWidth: imageColumnWidth)
            }
            start = end + 1
        }
        // Width measurement reserves the shared column even for unindented groups and headers.
        let contentWidth =
            (rows.lazy.filter { $0.rowHeight > 0 }.map { row in
                let iconWidth: CGFloat
                if #available(macOS 26, *), row.image == nil {
                    iconWidth = measuredSymbolColumnWidth
                } else {
                    iconWidth = row.iconWidth(symbolColumnWidth: symbolColumnWidth, bitmapColumnWidth: imageColumnWidth)
                }
                return row.titleWidthWithInsets + (iconWidth > 0 ? iconWidth + AlternativeMenuStyle.current.iconSpacing : 0)
            }.max() ?? 0) + trailingColumnWidth

        self.rows = rows
        self.keyColumnWidth = keyColumnWidth
        self.modifierColumnWidth = modifierColumnWidth
        self.trailingColumnWidth = trailingColumnWidth
        self.iconWidths = iconWidths
        self.contentWidth = contentWidth
    }

    func scrollAvailability(viewportHeight: CGFloat, topPadding: CGFloat) -> AlternativeMenuScrollAvailability {
        AlternativeMenuScrollAvailability(
            offset: offset,
            contentHeight: contentHeight,
            viewportHeight: viewportHeight,
            topPadding: topPadding
        )
    }
}
