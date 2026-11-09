//
//  AlternativeMenuRow.swift
//  AlternativeMenu
//
//  Copyright © 2015-2026 Clipy Project.
//

import AppKit
import SwiftUI

struct AlternativeMenuRow: View {
    let row: AlternativeMenuRowData
    let isEmphasized: Bool
    let iconWidth: CGFloat
    let trailingColumnWidth: CGFloat
    let keyColumnWidth: CGFloat

    @Environment(\.layoutDirection)
    private var layoutDirection

    private var textColor: Color {
        if isEmphasized {
            return Color(nsColor: .selectedMenuItemTextColor)
        }
        if row.kind == .header {
            return Color(nsColor: .tertiaryLabelColor)
        }
        return Color(nsColor: row.kind == .item ? .labelColor : .disabledControlTextColor)
    }

    var body: some View {
        Group {
            if row.kind == .separator {
                Divider()
            } else {
                label
            }
        }
        .padding(.leading, row.leadingInset)
        .padding(.trailing, AlternativeMenuStyle.current.rowTextInset)
        .frame(maxWidth: .infinity, alignment: .leading)
        .frame(height: row.rowHeight)
    }

    private var label: some View {
        HStack(spacing: AlternativeMenuStyle.current.iconSpacing) {
            if let image = row.image {
                Image(nsImage: image)
                    .renderingMode(image.isTemplate ? .template : .original)
                    .frame(width: iconWidth)
            } else if iconWidth > 0 {
                Color.clear
                    .frame(width: iconWidth, height: 0)
            }
            title
                .font(Font(row.font))
                .lineLimit(1)
                .truncationMode(.tail)
                .multilineTextAlignment(.leading)
                .offset(y: row.kind == .header ? 2 : 0)
        }
        .frame(maxWidth: .infinity, alignment: .leading)
        .padding(.trailing, trailingColumnWidth)
        .overlay(alignment: .trailing) {
            if row.hasSubmenu {
                Image(systemName: "chevron.forward")
                    .font(.system(size: AlternativeMenuStyle.menuFont.pointSize, weight: .bold))
                    .imageScale(.small)
                    .padding(.trailing, layoutDirection == .rightToLeft ? -2.5 : 0.5)
            } else if row.keyEquivalent.attributedText.length > 0 {
                AlternativeMenuKeyEquivalentLabel(
                    display: row.keyEquivalent,
                    keyColumnWidth: keyColumnWidth,
                    color: isEmphasized ? .selectedMenuItemTextColor : .tertiaryLabelColor
                )
                .frame(
                    width: max(0, trailingColumnWidth - AlternativeMenuStyle.current.keyEquivalentGap),
                    alignment: .trailing
                )
                // Match NSMenu's left-to-right shortcut order in RTL menus.
                .environment(\.layoutDirection, .leftToRight)
                // NSMenu draws modifiers as images; this view uses text with its measured widths.
                // A small RTL position difference (about 2 pt for some modifier combinations) is accepted.
                .padding(.trailing, layoutDirection == .rightToLeft ? -5 : -1)
                .allowsHitTesting(false)
            }
        }
        .foregroundStyle(textColor)
    }

    private var title: Text {
        switch row.title {
        case .plain(let title):
            Text(verbatim: title)
        case .attributed(let title):
            Text(AttributedString(title))
        }
    }
}
