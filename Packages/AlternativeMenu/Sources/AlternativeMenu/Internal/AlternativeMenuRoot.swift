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
                Color.clear.frame(height: AlternativeMenuStyle.padding)
            }
            GeometryReader { geometry in
                menuItems(viewportHeight: geometry.size.height)
            }
        }
        .alternativeMenuBackground(cornerRadius: AlternativeMenuStyle.cornerRadius)
    }

    private func menuItems(viewportHeight: CGFloat) -> some View {
        ScrollView {
            // Keep exact positions for every row, including large images.
            // Lazy height estimates can select a different row than the one hit.
            VStack(spacing: 0) {
                ForEach(Array(state.rows.enumerated()), id: \.offset) { index, row in
                    AlternativeMenuRow(
                        row: row,
                        isSelected: state.highlightedRowIndex == index,
                        iconWidth: state.iconWidths[index],
                        trailingColumnWidth: state.trailingColumnWidth,
                        keyColumnWidth: state.keyColumnWidth
                    )
                }
            }
            .padding(.bottom, AlternativeMenuStyle.padding)
            .background(alignment: .top) {
                let row = state.highlightedRow
                // Move one material view between rows so its backdrop does not
                // have to be recreated on every hover change.
                AlternativeMenuVisualEffectView(
                    material: .selection,
                    cornerRadius: AlternativeMenuStyle.cornerRadius - AlternativeMenuStyle.padding
                )
                .frame(height: row?.rowHeight ?? 0)
                .padding(.top, row?.verticalRange.lowerBound ?? 0)
                .allowsHitTesting(false)
            }
            .introspectScrollView { scrollView in
                scrollView?.hasVerticalScroller = false
                scrollView?.hasHorizontalScroller = false
                scrollView?.verticalScrollElasticity = .none
                scrollView?.setAccessibilityElement(false)
                state.scrollView = scrollView
            }
            .onGeometryChange(for: AlternativeMenuScrollAvailability.self) { [contentHeight = state.contentHeight] geometry in
                AlternativeMenuScrollAvailability(
                    offset: max(0, -geometry.frame(in: .named(menuItemsCoordinateSpace)).minY),
                    contentHeight: contentHeight,
                    viewportHeight: viewportHeight
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
            }
        }
        .overlay(alignment: .bottom) {
            if state.scrollAvailability.canScrollDown {
                scrollIndicator("chevron.down")
            }
        }
        .alternativeMenuAccessibility(menuAccessibilityView)
        .padding(.horizontal, AlternativeMenuStyle.padding)
    }

    private func scrollIndicator(_ symbol: String) -> some View {
        Image(systemName: symbol)
            .font(.system(size: 13, weight: .semibold))
            .foregroundStyle(.secondary)
            .imageScale(.small)
            .frame(height: AlternativeMenuStyle.scrollIndicatorHeight)
            .allowsHitTesting(false)
            .accessibilityHidden(true)
    }
}
