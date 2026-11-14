//
//  AlternaiveSearchMenu.swift
//  Clipy
//
//  Copyright © 2015-2026 Clipy Project.
//

import AppKit
import AlternativeMenu
import Dependencies
import Sharing
import SwiftUI

@MainActor
final class AlternaiveSearchMenu {
    private let seatchState = AlternativeMenuSearchBar.State()
    let scope: MenuSearchScope
    private let builder: AlternativeMenuBuilder
    private var initialItems: [AlternativeMenuItem] = []
    private var searchTask: Task<Void, Never>?
    private var query = ""
    private var highlightsFirstSearchResult = false
    private lazy var session = makeSession()

    @Dependency(\.continuousClock) private var clock
    @Dependency(\.pasteboardHistoryRepository) private var pasteboardHistoryRepository
    @Dependency(\.snippetRepository) private var snippetRepository

    @Shared(.highlightsFirstItemOnPopUp) private var highlightsFirstItemOnPopUp
    @Shared(.reordersClipsAfterPasting) private var reordersClipsAfterPasting
    @Shared(.showsImagesInMenu) private var showsImagesInMenu
    @Shared(.showsColorPreviewInMenu) private var showsColorPreviewInMenu

    init(scope: MenuSearchScope, builder: AlternativeMenuBuilder) {
        self.scope = scope
        self.builder = builder
    }

    deinit {
        searchTask?.cancel()
    }

    func present(
        at point: NSPoint = NSEvent.mouseLocation,
        initialItems: [AlternativeMenuItem]
    ) {
        cancelSearch()
        seatchState.text = ""
        seatchState.isSearching = false
        query = ""
        highlightsFirstSearchResult = false
        self.initialItems = initialItems
        session.present(at: point, highlightsFirstItem: highlightsFirstItemOnPopUp, menu: AlternativeMenu(items: initialItems))
        guard session.isVisible else { return }
        seatchState.focusRequest = UUID()
    }

    func dismiss() {
        session.dismiss()
    }

    private func refreshResults() {
        guard session.isVisible else { return }
        let newQuery = seatchState.text.trimmingCharacters(in: .whitespacesAndNewlines)
        guard query != newQuery else { return }
        query = newQuery
        search()
    }

    private func search() {
        cancelSearch()
        if query.isEmpty {
            seatchState.isSearching = false
            updateItems(initialItems)
            return
        }
        let needsLoadingDelay = !seatchState.isSearching
        let query = query
        let sortsByCreatedAt = !reordersClipsAfterPasting
        let includesThumbnailAsset = showsImagesInMenu || showsColorPreviewInMenu
        searchTask = Task { [weak self, scope, clock, pasteboardHistoryRepository, snippetRepository] in
            await withTaskGroup(of: Void.self) { [weak self] group in
                if needsLoadingDelay {
                    group.addTask { @MainActor [weak self] in
                        try? await clock.sleep(for: .milliseconds(200))
                        guard !Task.isCancelled, let self, session.isVisible else { return }
                        seatchState.isSearching = true
                    }
                }
                let limit = 15
                let (histories, snippets) = await Self.search(
                    query,
                    scope: scope,
                    sortsByCreatedAt: sortsByCreatedAt,
                    includesThumbnailAsset: includesThumbnailAsset,
                    limit: limit,
                    pasteboardHistoryRepository: pasteboardHistoryRepository,
                    snippetRepository: snippetRepository
                )
                group.cancelAll()
                guard !Task.isCancelled, let self, session.isVisible else { return }
                let items = builder.makeSearchItems(histories: histories, snippets: snippets, limit: limit)
                updateItems(items)
                seatchState.isSearching = false
                searchTask = nil
            }
        }
    }

    private func updateItems(_ items: [AlternativeMenuItem]) {
        highlightsFirstSearchResult = highlightsFirstItemOnPopUp || session.hasHighlightedItem || highlightsFirstSearchResult
        session.updateItems(items, highlightsFirstItem: highlightsFirstSearchResult)
    }

    private func cancelSearch() {
        searchTask?.cancel()
        searchTask = nil
    }

    private func makeSession() -> AlternativeMenuSession {
        // Match the search field in AppKit's Help menu.
        let fieldHeight: CGFloat = 24
        let topPadding: CGFloat = 10
        let bottomPadding: CGFloat = 6
        return AlternativeMenuSession(
            headerHeight: topPadding + fieldHeight + bottomPadding,
            minimumSymbolColumnWidth: builder.minimumSymbolColumnWidth,
            passesScrollWheelToHeader: false,
            shouldHandleMenuKey: { [weak self] _ in
                self?.refreshResults()
                return true
            },
            onDismiss: { [weak self] in
                self?.cancelSearch()
                self?.seatchState.isSearching = false
                self?.seatchState.focusRequest = nil
            },
            header: {
                AlternativeMenuSearchBar(state: seatchState) { [weak self] in self?.refreshResults() }
                    .frame(height: fieldHeight)
                    .padding(.horizontal, 10)
                    .padding(.top, topPadding)
                    .padding(.bottom, bottomPadding)
            }
        )
    }
}

private extension AlternaiveSearchMenu {
    @concurrent
    nonisolated static func search( // swiftlint:disable:this function_parameter_count
        _ query: String,
        scope: MenuSearchScope,
        sortsByCreatedAt: Bool,
        includesThumbnailAsset: Bool,
        limit: Int,
        pasteboardHistoryRepository: any PasteboardHistoryRepositoryProtocol,
        snippetRepository: any SnippetRepositoryProtocol
    ) async -> (histories: [PasteboardHistoryDetail], snippets: [SnippetSearchResult]) {
        guard !Task.isCancelled else {
            return ([], [])
        }

        let histories: [PasteboardHistoryDetail]
        switch scope {
        case .menu(.main), .menu(.history):
            histories = pasteboardHistoryRepository.searchHistoryDetails(
                query: query,
                sortsByCreatedAt: sortsByCreatedAt,
                includesThumbnailAsset: includesThumbnailAsset,
                limit: limit
            )
        case .menu(.snippet), .snippetFolder:
            histories = []
        }

        let snippets: [SnippetSearchResult]
        switch scope {
        case .menu(.main), .menu(.snippet):
            snippets = snippetRepository.searchSnippets(query: query, folderID: nil, limit: limit)
        case .menu(.history):
            snippets = []
        case .snippetFolder(let id):
            snippets = snippetRepository.searchSnippets(query: query, folderID: id, limit: limit)
        }

        return (histories, snippets)
    }
}
