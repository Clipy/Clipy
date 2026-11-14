//
//  MenuSearchScope.swift
//  Clipy
//
//  Copyright © 2015-2026 Clipy Project.
//

enum MenuSearchScope: Equatable, Sendable {
    case menu(MenuType)
    case snippetFolder(SnippetFolder.ID)
}
