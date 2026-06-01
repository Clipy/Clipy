//
//  DraggedData.swift
//
//  Clipy
//  GitHub: https://github.com/clipy
//  HP: https://clipy-app.com
//
//  Created by Econa77 on 2016/07/14.
//
//  Copyright © 2015-2018 Clipy Project.
//

import Foundation

final class DraggedData: NSObject, NSSecureCoding {
    // MARK: - Properties
    static let supportsSecureCoding: Bool = true

    let type: DragType
    let folderID: SnippetFolder.ID?
    let snippetID: Snippet.ID?
    let index: Int

    // MARK: - Enums
    enum DragType: Int {
        case folder, snippet
    }

    // MARK: - Initialize
    init(type: DragType, folderID: SnippetFolder.ID?, snippetID: Snippet.ID?, index: Int) {
        self.type = type
        self.folderID = folderID
        self.snippetID = snippetID
        self.index = index
        super.init()
    }

    // MARK: - NSCoding
    required init?(coder aDecoder: NSCoder) {
        self.type = DragType(rawValue: aDecoder.decodeInteger(forKey: "type")) ?? .folder
        self.folderID = (aDecoder.decodeObject(forKey: "folderID") as? UUID).map { .init(rawValue: $0) }
        self.snippetID = (aDecoder.decodeObject(forKey: "snippetID") as? UUID).map { .init(rawValue: $0) }
        self.index = aDecoder.decodeInteger(forKey: "index")
        super.init()
    }

    func encode(with aCoder: NSCoder) {
        aCoder.encode(type.rawValue, forKey: "type")
        aCoder.encode(folderID?.rawValue, forKey: "folderID")
        aCoder.encode(snippetID?.rawValue, forKey: "snippetID")
        aCoder.encode(index, forKey: "index")
    }
}
