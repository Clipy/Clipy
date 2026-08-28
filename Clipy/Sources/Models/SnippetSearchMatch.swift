//
//  SnippetSearchMatch.swift
//
//  Clipy
//  GitHub: https://github.com/clipy
//  HP: https://clipy-app.com
//
//  Created by Shunsuke Furubayashi on 2026/08/25.
//
//  Copyright © 2015-2026 Clipy Project.
//

/// A snippet that matched a search query, paired with the folder that contains it.
///
/// The folder is carried along so that search results can show which folder a snippet came from,
/// which is the only context the flat result list has (the menu conveys it through the hierarchy).
struct SnippetSearchMatch: Equatable {
    let snippet: Snippet
    let folder: SnippetFolder
}
