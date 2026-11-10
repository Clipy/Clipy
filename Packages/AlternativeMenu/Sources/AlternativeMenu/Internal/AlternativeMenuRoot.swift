//
//  AlternativeMenuRoot.swift
//  AlternativeMenu
//
//  Copyright © 2015-2026 Clipy Project.
//

import AppKit
import SwiftUI

struct AlternativeMenuRoot: View {
    let state: AlternativeMenuViewState
    let header: AlternativeMenuHeader?
    let menuAccessibilityView: AlternativeMenuAccessibilityView

    @Environment(\.colorScheme)
    private var colorScheme

    private let menuItemsCoordinateSpace = "AlternativeMenuRoot.menuItems"

    var body: some View {
        VStack(spacing: 0) {
            if let header {
                header.content
                    .frame(maxWidth: .infinity)
                    .frame(height: header.height)
                    .clipped()
                    .contentShape(Rectangle())
            } else {
                Color.clear.frame(height: AlternativeMenuStyle.current.padding)
            }
            GeometryReader { geometry in
                menuItems(viewportHeight: geometry.size.height)
            }
        }
        .alternativeMenuBackground(cornerRadius: AlternativeMenuStyle.current.cornerRadius)
    }

    private func menuItems(viewportHeight: CGFloat) -> some View {
        ScrollView(showsIndicators: false) {
            // Keep exact positions for every row, including large images.
            // Lazy height estimates can select a different row than the one hit.
            VStack(spacing: 0) {
                ForEach(Array(state.rows.enumerated()), id: \.offset) { index, row in
                    AlternativeMenuRow(
                        row: row,
                        isEmphasized: state.highlightedRowIndex == index && state.isSelectionEmphasized,
                        iconWidth: state.iconWidths[index],
                        trailingColumnWidth: state.trailingColumnWidth,
                        keyColumnWidth: state.keyColumnWidth,
                        modifierColumnWidth: state.modifierColumnWidth
                    )
                }
            }
            .padding(.bottom, AlternativeMenuStyle.current.padding)
            .background(alignment: .top) {
                let row = state.highlightedRow
                // Move one material view between rows so its backdrop does not
                // have to be recreated on every hover change.
                AlternativeMenuVisualEffectView(
                    material: AlternativeMenuVisualEffectView.menuSelectionMaterial,
                    cornerRadius: AlternativeMenuStyle.current.selectionCornerRadius,
                    isEmphasized: state.isSelectionEmphasized
                )
                .frame(height: row?.rowHeight ?? 0)
                .padding(.top, row?.verticalRange.lowerBound ?? 0)
                .allowsHitTesting(false)
            }
            .introspectScrollView { scrollView in
                scrollView?.verticalScrollElasticity = .none
                scrollView?.setAccessibilityElement(false)
                state.scrollView = scrollView
            }
            .onGeometryChange(for: AlternativeMenuScrollAvailability.self) { [contentHeight = state.contentHeight, topPadding = header == nil ? AlternativeMenuStyle.current.padding : 0] geometry in
                AlternativeMenuScrollAvailability(
                    offset: max(0, -geometry.frame(in: .named(menuItemsCoordinateSpace)).minY),
                    contentHeight: contentHeight,
                    viewportHeight: viewportHeight,
                    topPadding: topPadding
                )
            } action: {
                state.scrollAvailability = $0
            }
        }
        .coordinateSpace(name: menuItemsCoordinateSpace)
        .mask {
            Rectangle()
                .padding(.top, state.scrollAvailability.topInset)
                .padding(.bottom, state.scrollAvailability.bottomInset)
        }
        .overlay(alignment: .top) {
            if state.scrollAvailability.canScrollUp {
                scrollIndicator("chevron.up")
                    // The menu's top padding belongs to its rows, not the scroll arrow.
                    .offset(y: header == nil ? -AlternativeMenuStyle.current.padding : 0)
            }
        }
        .overlay(alignment: .bottom) {
            if state.scrollAvailability.canScrollDown {
                scrollIndicator("chevron.down")
            }
        }
        .alternativeMenuAccessibility(menuAccessibilityView)
        .padding(.horizontal, AlternativeMenuStyle.current.padding)
    }

    private var scrollIndicatorColor: Color {
        if #available(macOS 26, *) {
            return .secondary
        }
        return colorScheme == .dark ? .white.opacity(0.9) : Color(nsColor: .labelColor)
    }

    private func scrollIndicator(_ symbol: String) -> some View {
        Image(systemName: symbol)
            .font(.system(size: 13, weight: .semibold))
            .foregroundStyle(scrollIndicatorColor)
            .imageScale(.small)
            .frame(height: AlternativeMenuStyle.current.minimumRowHeight)
            .allowsHitTesting(false)
            .accessibilityHidden(true)
    }
}
