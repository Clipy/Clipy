//
//  AlternativeMenuSearchBar.swift
//  Clipy
//
//  Copyright © 2015-2026 Clipy Project.
//

import SwiftUI

struct AlternativeMenuSearchBar: View {
    @MainActor
    @Observable
    final class State {
        var text = ""
        var isSearching = false
        var focusRequest: UUID?
    }

    @Bindable var state: State
    let onChange: () -> Void

    @FocusState private var isFocused: Bool

    var body: some View {
        HStack(spacing: 8) {
            Group {
                if state.isSearching {
                    ProgressView().controlSize(.small)
                } else {
                    Image(systemName: "magnifyingglass")
                        // AppKit places the search symbol just below the field's center.
                        .offset(y: 1)
                }
            }
            .frame(width: 16, height: 16)
            .foregroundStyle(.secondary)
            .accessibilityHidden(true)

            TextField("Search", text: $state.text)
                .textFieldStyle(.plain)
                .focused($isFocused)
                .accessibilityLabel(Text("Search"))

            if !state.text.isEmpty {
                Button {
                    state.text = ""
                    isFocused = true
                } label: {
                    Image(systemName: "xmark.circle.fill")
                }
                .buttonStyle(.plain)
                .foregroundStyle(.secondary)
                .accessibilityLabel(Text("Clear Search"))
            }
        }
        .font(Font(NSFont.menuFont(ofSize: 0)))
        .padding(.horizontal, 10)
        .frame(maxHeight: .infinity)
        .background(Color(nsColor: .textBackgroundColor), in: Capsule())
        .overlay {
            Capsule()
                .strokeBorder(.primary.opacity(0.12), lineWidth: 0.5)
                .allowsHitTesting(false)
        }
        .contentShape(Capsule())
        .onTapGesture { isFocused = true }
        .onChange(of: state.text) { onChange() }
        .onChange(of: state.focusRequest, initial: true) {
            isFocused = state.focusRequest != nil
        }
    }
}
