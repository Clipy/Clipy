//
//  CPYDraggedData.swift
//  Clipy
//
//  Created by 古林俊佑 on 2016/07/14.
//  Copyright © 2016年 Shunsuke Furubayashi. All rights reserved.
//

import Foundation

final class CPYDraggedData: NSObject, NSCoding {

    // MARK: - Protperties
    let type: DragType
    let folderIdentifier: String?
    let snippetIdentifier: String?
    let index: Int

    // MARK: - Enums
    enum DragType: Int {
        case Folder, Snippet
    }

    // MARK: - Initialize
    init(type: DragType, folderIdentifier: String?, snippetIdentifier: String?, index: Int) {
        self.type = type
        self.folderIdentifier = folderIdentifier
        self.snippetIdentifier = snippetIdentifier
        self.index = index
        super.init()
    }

    // MARK: - NSCoding
    required init?(coder aDecoder: NSCoder) {
        self.type               = DragType(rawValue: aDecoder.decodeIntegerForKey("type")) ?? .Folder
        self.folderIdentifier   = aDecoder.decodeObjectForKey("folderIdentifier") as? String
        self.snippetIdentifier  = aDecoder.decodeObjectForKey("snippetIdentifier") as? String
        self.index              = aDecoder.decodeIntegerForKey("index")
        super.init()
    }

    func encodeWithCoder(aCoder: NSCoder) {
        aCoder.encodeInteger(type.rawValue, forKey: "type")
        aCoder.encodeObject(folderIdentifier, forKey: "folderIdentifier")
        aCoder.encodeObject(snippetIdentifier, forKey: "snippetIdentifier")
        aCoder.encodeInteger(index, forKey: "index")
    }
}
