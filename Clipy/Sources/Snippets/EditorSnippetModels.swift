//
//  EditorSnippetModels.swift
//
//  Clipy
//  GitHub: https://github.com/clipy
//  HP: https://clipy-app.com
//
//  Created by Shunsuke Furubayashi on 2026/07/05.
//
//  Copyright © 2015-2026 Clipy Project.
//

import Foundation

/// Snippet editor objects used only by the snippets editor outline view.
///
/// `NSOutlineView` infers visual state such as expansion and selection from item object
/// identity, so using SQLiteData table values directly can cause visual updates to be
/// treated as different items after reloads. Keep dedicated `NSObject` wrappers for this
/// screen so the outline view can maintain its UI state while the database remains table-based.
final class EditorSnippetFolder: NSObject {
    let id: SnippetFolder.ID
    var title: String
    var index: Int
    var isEnabled: Bool
    var snippets: [EditorSnippet]

    init(folderDetail: SnippetFolderDetail) {
        self.id = folderDetail.folder.id
        self.title = folderDetail.folder.title
        self.index = folderDetail.folder.index
        self.isEnabled = folderDetail.folder.isEnabled
        self.snippets = folderDetail.snippets.map(EditorSnippet.init)
        super.init()
    }

    init(folder: SnippetFolder) {
        self.id = folder.id
        self.title = folder.title
        self.index = folder.index
        self.isEnabled = folder.isEnabled
        self.snippets = []
        super.init()
    }
}

final class EditorSnippet: NSObject {
    let id: Snippet.ID
    var folderID: SnippetFolder.ID
    var title: String
    var content: String
    var index: Int
    var isEnabled: Bool

    init(snippet: Snippet) {
        self.id = snippet.id
        self.folderID = snippet.folderID
        self.title = snippet.title
        self.content = snippet.content
        self.index = snippet.index
        self.isEnabled = snippet.isEnabled
        super.init()
    }
}
