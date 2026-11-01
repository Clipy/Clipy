//
//  SnippetContentTextView.swift
//
//  Clipy
//  GitHub: https://github.com/clipy
//  HP: https://clipy-app.com
//
//  Created by Shunsuke Furubayashi on 2026/07/05.
//
//  Copyright © 2015-2026 Clipy Project.
//

import AppKit
import SwiftUI

struct SnippetContentTextView: View {
    let snippetID: Snippet.ID
    let content: String
    let onEdit: (String) -> Void

    @State private var isEmpty: Bool

    init(snippetID: Snippet.ID, content: String, onEdit: @escaping (String) -> Void) {
        self.snippetID = snippetID
        self.content = content
        self.onEdit = onEdit
        _isEmpty = State(initialValue: content.isEmpty)
    }

    var body: some View {
        SnippetTextView(snippetID: snippetID, content: content) { text in
            isEmpty = text.isEmpty
            onEdit(text)
        }
        .overlay(alignment: .topLeading) {
            if isEmpty {
                Text(.SnippetEditor.pleaseFillInTheContentsOfTheSnippet)
                    .font(.system(size: 14))
                    .foregroundStyle(Color(nsColor: .placeholderTextColor))
                    .padding(.horizontal, 5)
                    .padding(.vertical, 7)
                    .allowsHitTesting(false)
                    .accessibilityHidden(true)
            }
        }
        .onChange(of: snippetID) {
            isEmpty = content.isEmpty
        }
    }
}

/// Keeps control over automatic substitutions and the native undo history.
private struct SnippetTextView: NSViewRepresentable {
    let snippetID: Snippet.ID
    let content: String
    let onEdit: (String) -> Void

    func makeCoordinator() -> Coordinator {
        Coordinator(onEdit: onEdit)
    }

    func makeNSView(context: Context) -> NSScrollView {
        let scrollView = NSScrollView()
        scrollView.hasVerticalScroller = true
        scrollView.autohidesScrollers = true
        scrollView.drawsBackground = true

        let textView = NSTextView(frame: .zero)
        textView.font = NSFont.systemFont(ofSize: 14)
        textView.isAutomaticQuoteSubstitutionEnabled = false
        textView.isAutomaticDashSubstitutionEnabled = false
        textView.enabledTextCheckingTypes = 0
        textView.isRichText = false
        textView.allowsUndo = true
        textView.textContainerInset = NSSize(width: 0, height: 7)
        textView.isVerticallyResizable = true
        textView.isHorizontallyResizable = false
        textView.autoresizingMask = .width
        textView.minSize = NSSize(width: 0, height: 0)
        textView.maxSize = NSSize(width: CGFloat.greatestFiniteMagnitude, height: CGFloat.greatestFiniteMagnitude)
        textView.textContainer?.widthTracksTextView = true
        textView.textContainer?.containerSize = NSSize(width: 0, height: CGFloat.greatestFiniteMagnitude)
        textView.delegate = context.coordinator

        scrollView.documentView = textView

        textView.string = content
        context.coordinator.snippetID = snippetID
        return scrollView
    }

    func updateNSView(_ scrollView: NSScrollView, context: Context) {
        let coordinator = context.coordinator
        coordinator.onEdit = onEdit
        guard let textView = scrollView.documentView as? NSTextView else { return }
        // Replace the contents only when another snippet is selected, so typing
        // does not reset the selection or the scroll position.
        guard coordinator.snippetID != snippetID else { return }
        coordinator.snippetID = snippetID
        textView.string = content
        textView.scrollToBeginningOfDocument(nil)
        // Reset TextView Undo/Redo history
        textView.undoManager?.removeAllActions()
    }

    // MARK: - Coordinator
    final class Coordinator: NSObject, NSTextViewDelegate {
        var snippetID: Snippet.ID?
        var onEdit: (String) -> Void

        init(onEdit: @escaping (String) -> Void) {
            self.onEdit = onEdit
            super.init()
        }

        func textDidChange(_ notification: Notification) {
            guard let textView = notification.object as? NSTextView else { return }
            onEdit(textView.string)
        }
    }
}
