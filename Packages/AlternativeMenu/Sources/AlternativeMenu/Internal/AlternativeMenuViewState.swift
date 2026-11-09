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
    private let minimumSymbolColumnWidth: CGFloat
    private(set) var rows: [AlternativeMenuRowData] = []
    private(set) var keyColumnWidth: CGFloat = 0
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
        (rows.last?.verticalRange.upperBound ?? 0) + AlternativeMenuStyle.padding
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
        let trailingColumnWidth = rows.lazy.map { $0.trailingWidth(keyColumnWidth: keyColumnWidth) }.max() ?? 0
        let symbolColumnWidth = max(
            minimumSymbolColumnWidth,
            rows.lazy.filter(\.isSymbolImage).compactMap { $0.image?.size.width }.max() ?? 0
        )
        var iconWidths = Array(repeating: CGFloat.zero, count: rows.count)
        var start = 0
        for end in rows.indices where rows[end].kind == .separator || rows[end].kind == .header || end == rows.count - 1 {
            // A header's symbol belongs to the preceding group; the next row starts a new one.
            let group = start...end
            // Only groups containing a symbol reserve its column for imageless rows.
            // Symbol-bearing groups still share the menu's widest symbol.
            let groupSymbolWidth = group.contains { rows[$0].isSymbolImage } ? symbolColumnWidth : 0
            for index in group {
                iconWidths[index] = rows[index].iconWidth(symbolColumnWidth: groupSymbolWidth)
            }
            start = end + 1
        }
        // NSMenu measures its overall width using the global symbol column,
        // even when a row's group does not reserve that column while drawing.
        let contentWidth =
            (rows.lazy.map { row in
                let iconWidth = row.iconWidth(symbolColumnWidth: symbolColumnWidth)
                return row.titleWidthWithInsets + (iconWidth > 0 ? iconWidth + AlternativeMenuStyle.iconSpacing : 0)
            }.max() ?? 0) + trailingColumnWidth

        self.rows = rows
        self.keyColumnWidth = keyColumnWidth
        self.trailingColumnWidth = trailingColumnWidth
        self.iconWidths = iconWidths
        self.contentWidth = contentWidth
    }

    func scrollAvailability(viewportHeight: CGFloat, topPadding: CGFloat) -> AlternativeMenuScrollAvailability {
        AlternativeMenuScrollAvailability(offset: offset, contentHeight: contentHeight, viewportHeight: viewportHeight, topPadding: topPadding)
    }
}
