//
//  Constants.swift
//
//  Clipy
//  GitHub: https://github.com/clipy
//  HP: https://clipy-app.com
//
//  Created by Econa77 on 2016/04/17.
//
//  Copyright © 2015-2018 Clipy Project.
//

import Foundation

struct Constants {

    struct Application {
        #if DEBUG
            static let name = "ClipyDEBUG"
        #else
            static let name = "Clipy"
        #endif
    }

    struct Menu {
        static let clip = "ClipMenu"
        static let history = "HistoryMenu"
        static let snippet = "SnippetsMenu"
    }

    struct Common {
        static let index = "index"
        static let title = "title"
        static let snippets = "snippets"
        static let content = "content"
        static let selector = "selector"
        static let draggedDataType = "public.data"
    }

    struct Xml {
        static let fileType = "xml"
        static let type = "type"
        static let rootElement = "folders"
        static let folderElement = "folder"
        static let snippetElement = "snippet"
        static let titleElement = "title"
        static let snippetsElement = "snippets"
        static let contentElement = "content"
    }
}
