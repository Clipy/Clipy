//
//  SnippetsEditorView.swift
//
//  Clipy
//  GitHub: https://github.com/clipy
//  HP: https://clipy-app.com
//
//  Created by Shunsuke Furubayashi on 2026/07/05.
//
//  Copyright © 2015-2026 Clipy Project.
//

import SwiftUI
import UniformTypeIdentifiers

struct SnippetsEditorView: View {

    static let minimumContentSize = CGSize(width: 800, height: 600)

    @StateObject private var viewModel = SnippetsEditorViewModel()
    @State private var isImporting = false
    @State private var isExporting = false
    @State private var exportFile: SnippetsXMLFile?

    var body: some View {
        GeometryReader { proxy in
            NavigationSplitView(columnVisibility: .constant(.all)) {
                SnippetsOutlineView(viewModel: viewModel)
                    .toolbar(removing: .sidebarToggle)
                    .navigationSplitViewColumnWidth(
                        min: 150,
                        ideal: 250,
                        max: max(150, proxy.size.width / 2)
                    )
            } detail: {
                detail
                    .frame(minWidth: 300, maxWidth: .infinity, maxHeight: .infinity)
            }
            .navigationSplitViewStyle(.balanced)
        }
        .frame(minWidth: Self.minimumContentSize.width, minHeight: Self.minimumContentSize.height)
        .toolbar {
            ToolbarItem {
                Button(.SnippetEditor.addSnippet, systemImage: "text.badge.plus", action: viewModel.addSnippet)
                    .disabled(viewModel.selectedFolder == nil)
            }
            ToolbarItem {
                Button(.SnippetEditor.addFolder, systemImage: "folder.badge.plus", action: viewModel.addFolder)
            }
            ToolbarItem {
                Button(.SnippetEditor.delete, systemImage: "trash", action: viewModel.confirmDeleteSelectedItem)
                    .disabled(viewModel.selection == nil)
            }
            ToolbarItem {
                Button(.SnippetEditor.enableDisable, systemImage: "checkmark.circle", action: viewModel.toggleSelectedItemIsEnabled)
                    .disabled(viewModel.selection == nil)
            }
            ToolbarItem {
                Button(.SnippetEditor.import, systemImage: "square.and.arrow.down") {
                    isImporting = true
                }
            }
            ToolbarItem {
                Button(.SnippetEditor.export, systemImage: "square.and.arrow.up") {
                    exportFile = viewModel.makeExportFile()
                    isExporting = true
                }
            }
        }
        .fileImporter(
            isPresented: $isImporting,
            allowedContentTypes: [.xml],
            onCompletion: handleImport
        )
        .fileExporter(
            isPresented: $isExporting,
            document: exportFile,
            contentTypes: [.xml],
            defaultFilename: "snippets.xml",
            onCompletion: handleExport,
            onCancellation: { exportFile = nil }
        )
        .fileDialogDefaultDirectory(URL(fileURLWithPath: NSHomeDirectory()))
        .alert(Text(.SnippetEditor.deleteItem), isPresented: $viewModel.isDeleteAlertPresented) {
            Button(.SnippetEditor.deleteItem, role: .destructive) {
                viewModel.deleteSelectedItem()
            }
            Button(.SnippetEditor.cancel, role: .cancel) {}
        } message: {
            Text(.SnippetEditor.areYouSureWantToDeleteThisItem)
        }
    }

    private func handleImport(_ result: Result<URL, Error>) {
        switch result {
        case .success(let url):
            viewModel.importSnippets(from: url)
        case .failure(let error):
            if (error as? CocoaError)?.code != .userCancelled {
                NSSound.beep()
            }
        }
    }

    private func handleExport(_ result: Result<URL, Error>) {
        exportFile = nil
        if case .failure(let error) = result, (error as? CocoaError)?.code != .userCancelled {
            NSSound.beep()
        }
    }

    @ViewBuilder
    private var detail: some View {
        if let folder = viewModel.folderForSettings {
            FolderSettingsView(viewModel: viewModel, folder: folder)
        } else if let snippet = viewModel.selectedSnippet {
            SnippetContentTextView(snippetID: snippet.id, content: snippet.content) { content in
                viewModel.updateContent(of: snippet, content: content)
            }
        } else {
            VStack(spacing: 12) {
                Image(systemName: "doc.text").font(.largeTitle)
                Text(.SnippetEditor.pleaseFillInTheContentsOfTheSnippet)
            }
            .foregroundStyle(.secondary)
            .frame(maxWidth: .infinity, maxHeight: .infinity)
        }
    }
}

// MARK: - Folder Settings
private struct FolderSettingsView: View {

    @ObservedObject var viewModel: SnippetsEditorViewModel
    let folder: EditorSnippetFolder

    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            HStack(spacing: 8) {
                Image(systemName: "folder")
                Text(folder.title)
                    .font(.system(size: 14))
                    .foregroundStyle(.primary)
                    .lineLimit(1)
            }
            Rectangle()
                .fill(Color.secondary.opacity(0.3))
                .frame(height: 1)
            Text(.SnippetEditor.shortcut)
                .font(.system(size: 13, weight: .bold))
                .padding(.top, 8)
            ShortcutRecorder(keyCombo: viewModel.keyCombo(for: folder)) { keyCombo in
                viewModel.updateKeyCombo(keyCombo, for: folder)
            }
            .frame(width: 240, height: 32)
        }
        .frame(width: 250)
        .frame(maxWidth: .infinity, maxHeight: .infinity)
    }
}
