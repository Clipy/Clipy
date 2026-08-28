//
//  SearchService.swift
//
//  Clipy
//  GitHub: https://github.com/clipy
//  HP: https://clipy-app.com
//
//  Created by Shunsuke Furubayashi on 2026/08/25.
//
//  Copyright © 2015-2026 Clipy Project.
//

import AppKit
import Dependencies
import Sharing

protocol SearchServiceProtocol {
    func search(query: String, limit: Int) -> [SearchResultItem]
}

final class SearchService: SearchServiceProtocol {
    @Dependency(\.pasteboardHistoryRepository)
    private var pasteboardHistoryRepository
    @Dependency(\.snippetRepository)
    private var snippetRepository
    @Dependency(\.defaultAppStorage)
    private var appStorage

    /// Searches history and snippets, returning snippets first.
    ///
    /// Snippets lead because they are curated by hand and therefore usually the intended target,
    /// whereas history is a rolling buffer. Within each kind the repository's own ordering is kept
    /// (relevance for full-text matches, recency for the short-query fallback).
    ///
    /// An empty query yields the most recent history, so opening the panel immediately shows
    /// something useful instead of a blank list.
    func search(query: String, limit: Int) -> [SearchResultItem] {
        let includesThumbnailAsset = appStorage.bool(forKey: Constants.UserDefaults.showImageInTheMenu)
            || appStorage.bool(forKey: Constants.UserDefaults.showColorPreviewInTheMenu)

        guard !query.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty else {
            let reorderClipsAfterPasting = appStorage.bool(forKey: Constants.UserDefaults.reorderClipsAfterPasting)
            return pasteboardHistoryRepository
                .fetchHistoryDetails(
                    sortsByCreatedAt: !reorderClipsAfterPasting,
                    includesThumbnailAsset: includesThumbnailAsset,
                    limit: limit
                )
                .map { .history($0) }
        }

        let snippets = snippetRepository
            .searchSnippets(matching: query, limit: limit)
            .map { SearchResultItem.snippet($0) }
        let histories = pasteboardHistoryRepository
            .searchHistories(
                matching: query,
                includesThumbnailAsset: includesThumbnailAsset,
                limit: limit
            )
            .map { SearchResultItem.history($0) }

        return Array((snippets + histories).prefix(limit))
    }
}

extension DependencyValues {
    var searchService: SearchServiceProtocol {
        get { self[SearchServiceKey.self] }
        set { self[SearchServiceKey.self] = newValue }
    }

    private enum SearchServiceKey: DependencyKey {
        static let liveValue: any SearchServiceProtocol = SearchService()
    }
}
