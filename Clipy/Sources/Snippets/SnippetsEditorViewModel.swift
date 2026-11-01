//
//  SnippetsEditorViewModel.swift
//
//  Clipy
//  GitHub: https://github.com/clipy
//  HP: https://clipy-app.com
//
//  Created by Shunsuke Furubayashi on 2026/07/05.
//
//  Copyright © 2015-2026 Clipy Project.
//

import AEXML
import AppKit
import Combine
import Dependencies
import Magnet

final class SnippetsEditorViewModel: ObservableObject {

    // MARK: - Types
    enum Selection: Equatable {
        case folder(SnippetFolder.ID)
        case snippet(Snippet.ID)
    }

    // MARK: - Properties
    @Published private(set) var folders = [EditorSnippetFolder]()
    @Published var selection: Selection?
    @Published var isDeleteAlertPresented = false
    /// Incremented whenever items are added, removed or reordered so the outline view knows to reload.
    @Published private(set) var structureVersion = 0

    @Dependency(\.snippetRepository)
    private var snippetRepository

    @Dependency(\.hotKeyService)
    private var hotKeyService

    private enum XML {
        static let rootElement = "folders"
        static let folderElement = "folder"
        static let snippetElement = "snippet"
        static let titleElement = "title"
        static let snippetsElement = "snippets"
        static let contentElement = "content"
    }

    var selectedFolder: EditorSnippetFolder? {
        switch selection {
        case .folder(let id):
            return folders.first { $0.id == id }
        case .snippet(let id):
            return folders.first { folder in folder.snippets.contains { $0.id == id } }
        case nil:
            return nil
        }
    }

    var selectedSnippet: EditorSnippet? {
        guard case .snippet(let id) = selection else { return nil }
        for folder in folders {
            if let snippet = folder.snippets.first(where: { $0.id == id }) {
                return snippet
            }
        }
        return nil
    }

    var folderForSettings: EditorSnippetFolder? {
        guard case .folder = selection else { return nil }
        return selectedFolder
    }

    // MARK: - Initialize
    init() {
        folders = snippetRepository.fetchFolderDetails().map(EditorSnippetFolder.init)
        if let folder = folders.first {
            selection = .folder(folder.id)
        }
    }
}

// MARK: - Add / Delete / Status
extension SnippetsEditorViewModel {
    func addSnippet() {
        guard let folder = selectedFolder, let snippet = snippetRepository.insertSnippet(to: folder.id) else {
            NSSound.beep()
            return
        }
        let editorSnippet = EditorSnippet(snippet: snippet)
        folder.snippets.append(editorSnippet)
        structureVersion += 1
        selection = .snippet(editorSnippet.id)
    }

    func addFolder() {
        guard let folder = snippetRepository.insertFolder() else {
            NSSound.beep()
            return
        }
        let editorFolder = EditorSnippetFolder(folder: folder)
        folders.append(editorFolder)
        structureVersion += 1
        selection = .folder(editorFolder.id)
    }

    func confirmDeleteSelectedItem() {
        guard selection != nil else {
            NSSound.beep()
            return
        }
        isDeleteAlertPresented = true
    }

    func deleteSelectedItem() {
        switch selection {
        case .folder(let id):
            folders.removeAll { $0.id == id }
            snippetRepository.deleteFolder(id)
            hotKeyService.unregisterSnippetHotKey(with: id.uuidString)
        case .snippet(let id):
            guard let folder = selectedFolder else { return }
            folder.snippets.removeAll { $0.id == id }
            snippetRepository.deleteSnippet(id)
        case nil:
            return
        }
        selection = nil
        structureVersion += 1
    }

    func toggleSelectedItemIsEnabled() {
        switch selection {
        case .folder:
            guard let folder = selectedFolder else { return }
            folder.isEnabled.toggle()
            snippetRepository.updateFolderIsEnabled(folder.id, isEnabled: folder.isEnabled)
        case .snippet:
            guard let snippet = selectedSnippet else { return }
            snippet.isEnabled.toggle()
            snippetRepository.updateSnippetIsEnabled(snippet.id, isEnabled: snippet.isEnabled)
        case nil:
            NSSound.beep()
            return
        }
        structureVersion += 1
    }
}

// MARK: - Rename / Content / Shortcut
extension SnippetsEditorViewModel {
    func rename(folder: EditorSnippetFolder, title: String) {
        folder.title = title
        snippetRepository.updateFolderTitle(folder.id, title: title)
        objectWillChange.send()
    }

    func rename(snippet: EditorSnippet, title: String) {
        snippet.title = title
        snippetRepository.updateSnippetTitle(snippet.id, title: title)
        objectWillChange.send()
    }

    func updateContent(of snippet: EditorSnippet, content: String) {
        snippet.content = content
        snippetRepository.updateSnippetContent(snippet.id, content: content)
    }

    func keyCombo(for folder: EditorSnippetFolder) -> KeyCombo? {
        hotKeyService.snippetKeyCombo(forIdentifier: folder.id.uuidString)
    }

    func updateKeyCombo(_ keyCombo: KeyCombo?, for folder: EditorSnippetFolder) {
        guard let keyCombo else {
            hotKeyService.unregisterSnippetHotKey(with: folder.id.uuidString)
            return
        }
        hotKeyService.registerSnippetHotKey(with: folder.id.uuidString, keyCombo: keyCombo)
    }
}

// MARK: - Drag and Drop
extension SnippetsEditorViewModel {
    func moveFolder(_ draggedData: DraggedData, to index: Int) -> Bool {
        guard index != draggedData.index, index >= 0 else { return false }
        guard let folder = folders.first(where: { $0.id == draggedData.folderID }) else { return false }
        folders.insert(folder, at: index)
        let removedIndex = (index < draggedData.index) ? draggedData.index + 1 : draggedData.index
        folders.remove(at: removedIndex)
        snippetRepository.updateFolderIndexes(folders.map(\.id))
        structureVersion += 1
        selection = .folder(folder.id)
        return true
    }

    func moveSnippet(_ draggedData: DraggedData, to folder: EditorSnippetFolder?, at index: Int) -> Bool {
        guard let fromFolder = folders.first(where: { $0.id == draggedData.folderID }) else { return false }
        guard let toFolder = folder else { return false }
        guard let snippet = fromFolder.snippets.first(where: { $0.id == draggedData.snippetID }) else { return false }

        if fromFolder.id == toFolder.id {
            guard index >= 0, index != draggedData.index else { return false }
            // Move to same folder
            fromFolder.snippets.insert(snippet, at: index)
            let removedIndex = (index < draggedData.index) ? draggedData.index + 1 : draggedData.index
            fromFolder.snippets.remove(at: removedIndex)
            snippetRepository.updateSnippetIndexes(fromFolder.snippets.map(\.id))
        } else {
            // Move to other folder
            let index = max(0, index)
            toFolder.snippets.insert(snippet, at: index)
            fromFolder.snippets.removeAll { $0.id == snippet.id }
            snippet.folderID = toFolder.id
            snippetRepository.moveSnippet(snippet.id, to: toFolder.id, snippetIDs: toFolder.snippets.map(\.id))
        }
        structureVersion += 1
        selection = .snippet(snippet.id)
        return true
    }
}

// MARK: - Import / Export
extension SnippetsEditorViewModel {
    func importSnippets(from url: URL) {
        let hasAccess = url.startAccessingSecurityScopedResource()
        defer {
            if hasAccess {
                url.stopAccessingSecurityScopedResource()
            }
        }

        do {
            let data = try Data(contentsOf: url)
            var options = AEXMLOptions()
            options.parserSettings.shouldTrimWhitespace = false
            let xmlDocument = try AEXMLDocument(xml: data, options: options)
            let folders = xmlDocument[XML.rootElement]
                .children
                .map { folderElement in
                    let title = folderElement[XML.titleElement].value ?? "untitled folder"
                    let snippets = folderElement[XML.snippetsElement][XML.snippetElement]
                        .all?
                        .map { (title: $0[XML.titleElement].value ?? "untitled snippet", content: $0[XML.contentElement].value ?? "") } ?? []
                    return (title: title, snippets: snippets)
                }
            guard let folderDetails = snippetRepository.insertFolders(folders) else {
                NSSound.beep()
                return
            }
            self.folders.append(contentsOf: folderDetails.map(EditorSnippetFolder.init))
            structureVersion += 1
        } catch {
            NSSound.beep()
        }
    }

    func makeExportFile() -> SnippetsXMLFile {
        let xmlDocument = AEXMLDocument()
        let rootElement = xmlDocument.addChild(name: XML.rootElement)

        folders.forEach { folder in
            let folderElement = rootElement.addChild(name: XML.folderElement)

            folderElement.addChild(name: XML.titleElement, value: folder.title)

            let snippetsElement = folderElement.addChild(name: XML.snippetsElement)
            folder.snippets
                .forEach { snippet in
                    let snippetElement = snippetsElement.addChild(name: XML.snippetElement)
                    snippetElement.addChild(name: XML.titleElement, value: snippet.title)
                    snippetElement.addChild(name: XML.contentElement, value: snippet.content)
                }
        }

        return SnippetsXMLFile(data: Data(xmlDocument.xml.utf8))
    }
}
