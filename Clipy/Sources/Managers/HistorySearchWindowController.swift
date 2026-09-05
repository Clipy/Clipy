//
//  HistorySearchWindowController.swift
//
//  Clipy
//  GitHub: https://github.com/clipy
//  HP: https://clipy-app.com
//
//  Copyright © 2015-2026 Clipy Project.
//

import AppKit
import Combine
import Dependencies
import SwiftUI

final class HistorySearchWindowController: NSWindowController {
    static let shared = HistorySearchWindowController()

    private let viewModel = HistorySearchViewModel()
    private var previousApplication: NSRunningApplication?

    private lazy var hostingController = NSHostingController(
        rootView: HistorySearchView(viewModel: viewModel)
    )

    private init() {
        let window = NSWindow(
            contentRect: NSRect(x: 0, y: 0, width: 680, height: 480),
            styleMask: [.titled, .closable, .miniaturizable, .resizable],
            backing: .buffered,
            defer: false
        )
        window.title = String(localized: "Search History")
        window.isReleasedWhenClosed = false
        window.minSize = NSSize(width: 480, height: 320)
        window.collectionBehavior = [.moveToActiveSpace]
        window.setFrameAutosaveName("HistorySearchWindow")

        super.init(window: window)

        window.contentViewController = hostingController
        window.delegate = self
        viewModel.onPaste = { [weak self] id in
            self?.pasteHistory(id: id)
        }
    }

    @available(*, unavailable)
    required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }

    func showSearchWindow() {
        let frontmostApplication = NSWorkspace.shared.frontmostApplication
        if frontmostApplication?.processIdentifier != ProcessInfo.processInfo.processIdentifier {
            previousApplication = frontmostApplication
        }

        viewModel.reset()
        NSApp.activate(ignoringOtherApps: true)
        showWindow(nil)
        window?.makeKeyAndOrderFront(nil)
    }
}

private extension HistorySearchWindowController {
    func pasteHistory(id: PasteboardHistory.ID) {
        @Dependency(\.pasteboardHistoryRepository) var pasteboardHistoryRepository

        guard let content = pasteboardHistoryRepository.fetchContent(id: id) else {
            NSSound.beep()
            return
        }

        window?.orderOut(nil)
        let targetApplication = previousApplication
        previousApplication = nil
        targetApplication?.activate(options: [.activateIgnoringOtherApps])

        DispatchQueue.main.asyncAfter(deadline: .now() + 0.15) {
            AppEnvironment.current.pasteService.paste(id: id, content: content)
        }
    }

    func reactivatePreviousApplication() {
        previousApplication?.activate(options: [.activateIgnoringOtherApps])
        previousApplication = nil
    }
}

extension HistorySearchWindowController: NSWindowDelegate {
    func windowWillClose(_ notification: Notification) {
        reactivatePreviousApplication()
    }
}

private final class HistorySearchViewModel: ObservableObject {
    @Published var query = ""
    @Published private(set) var results = [PasteboardHistoryDetail]()
    @Published var selectedHistoryID: PasteboardHistory.ID?
    @Published private(set) var focusRequest = UUID()

    var onPaste: ((PasteboardHistory.ID) -> Void)?

    @Dependency(\.pasteboardHistoryRepository)
    private var pasteboardHistoryRepository
    private var cancellables = Set<AnyCancellable>()

    init() {
        $query
            .removeDuplicates()
            .debounce(for: .milliseconds(120), scheduler: RunLoop.main)
            .sink { [weak self] _ in self?.search() }
            .store(in: &cancellables)

        pasteboardHistoryRepository.observeHistories()
            .receive(on: DispatchQueue.main)
            .sink { [weak self] _ in
                guard self?.query.isEmpty == false else { return }
                self?.search()
            }
            .store(in: &cancellables)
    }

    func reset() {
        query = ""
        results = []
        selectedHistoryID = nil
        focusRequest = UUID()
    }

    func pasteSelectedHistory() {
        guard let id = selectedHistoryID ?? results.first?.history.id else { return }
        onPaste?(id)
    }

    func pasteHistory(id: PasteboardHistory.ID) {
        onPaste?(id)
    }

    private func search() {
        let fragment = query.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !fragment.isEmpty else {
            results = []
            selectedHistoryID = nil
            return
        }

        let results = pasteboardHistoryRepository.searchHistoryDetails(
            containing: fragment,
            includesThumbnailAsset: true,
            limit: 100
        )
        self.results = results
        if !results.contains(where: { $0.history.id == selectedHistoryID }) {
            selectedHistoryID = results.first?.history.id
        }
    }
}

private struct HistorySearchView: View {
    @ObservedObject var viewModel: HistorySearchViewModel
    @FocusState private var searchFieldIsFocused: Bool

    var body: some View {
        VStack(spacing: 12) {
            HStack(spacing: 8) {
                Image(systemName: "magnifyingglass")
                    .foregroundStyle(.secondary)
                TextField(String(localized: "Search clipboard history"), text: $viewModel.query)
                    .textFieldStyle(.roundedBorder)
                    .focused($searchFieldIsFocused)
            }

            searchContent

            HStack {
                Spacer()
                Button(String(localized: "Paste")) {
                    viewModel.pasteSelectedHistory()
                }
                .keyboardShortcut(.defaultAction)
                .disabled(viewModel.results.isEmpty)
            }
        }
        .padding(16)
        .onAppear {
            searchFieldIsFocused = true
        }
        .onChange(of: viewModel.focusRequest) { _ in
            searchFieldIsFocused = true
        }
    }

    @ViewBuilder
    private var searchContent: some View {
        if viewModel.query.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty {
            emptyState(
                systemImage: "text.magnifyingglass",
                message: String(localized: "Enter a continuous text fragment to search your clipboard history")
            )
        } else if viewModel.results.isEmpty {
            emptyState(
                systemImage: "doc.text.magnifyingglass",
                message: String(localized: "No matching history")
            )
        } else {
            List(viewModel.results, id: \.history.id, selection: $viewModel.selectedHistoryID) { detail in
                HistorySearchResultRow(detail: detail, query: viewModel.query)
                    .tag(detail.history.id)
                    .contentShape(Rectangle())
                    .simultaneousGesture(
                        TapGesture(count: 2).onEnded {
                            viewModel.pasteHistory(id: detail.history.id)
                        }
                    )
            }
            .listStyle(.inset)
        }
    }

    private func emptyState(systemImage: String, message: String) -> some View {
        VStack(spacing: 10) {
            Image(systemName: systemImage)
                .font(.system(size: 32))
                .foregroundStyle(.tertiary)
            Text(message)
                .foregroundStyle(.secondary)
                .multilineTextAlignment(.center)
        }
        .frame(maxWidth: .infinity, maxHeight: .infinity)
    }
}

private struct HistorySearchResultRow: View {
    let detail: PasteboardHistoryDetail
    let query: String

    var body: some View {
        HStack(spacing: 12) {
            thumbnail

            VStack(alignment: .leading, spacing: 5) {
                Text(detail.history.searchPreview(matching: query))
                    .lineLimit(2)
                    .frame(maxWidth: .infinity, alignment: .leading)
                Text(
                    Date(timeIntervalSince1970: TimeInterval(detail.history.updateAt))
                        .formatted(date: .abbreviated, time: .shortened)
                )
                .font(.caption)
                .foregroundStyle(.secondary)
            }
        }
        .padding(.vertical, 4)
    }

    @ViewBuilder
    private var thumbnail: some View {
        if let data = detail.thumbnailAsset?.data, let image = NSImage(data: data) {
            Image(nsImage: image)
                .resizable()
                .scaledToFit()
                .frame(width: 40, height: 40)
                .cornerRadius(4)
        } else {
            Image(systemName: "doc.on.clipboard")
                .font(.system(size: 22))
                .foregroundStyle(.secondary)
                .frame(width: 40, height: 40)
        }
    }
}

private extension PasteboardHistory {
    func searchPreview(matching query: String) -> String {
        let fragment = query.trimmingCharacters(in: .whitespacesAndNewlines)
        let source: String
        if title.range(of: fragment, options: .caseInsensitive) != nil {
            source = title
        } else if let ocrText, ocrText.range(of: fragment, options: .caseInsensitive) != nil {
            source = ocrText
        } else {
            source = title
        }

        guard !source.isEmpty else { return typedTitle }
        guard let match = source.range(of: fragment, options: .caseInsensitive) else {
            return source.replacingOccurrences(of: "\n", with: " ")
        }

        let radius = 60
        let lowerBound = source.index(match.lowerBound, offsetBy: -radius, limitedBy: source.startIndex) ?? source.startIndex
        let upperBound = source.index(match.upperBound, offsetBy: radius, limitedBy: source.endIndex) ?? source.endIndex
        var preview = String(source[lowerBound..<upperBound])
            .replacingOccurrences(of: "\n", with: " ")
            .replacingOccurrences(of: "\r", with: " ")
        if lowerBound != source.startIndex {
            preview = "…" + preview
        }
        if upperBound != source.endIndex {
            preview += "…"
        }
        return preview
    }
}
