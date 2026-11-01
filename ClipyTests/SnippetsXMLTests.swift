import Dependencies
import DependenciesTestSupport
import Foundation
import Testing
@testable import Clipy

@MainActor
@Suite(
    .dependencies {
        try $0.bootstrapDatabase()
    }
)
struct SnippetsXMLTests {
    private let repository = SnippetRepository()

    @Test
    func importsLegacyXMLWithoutChangingExistingSelection() throws {
        let existing = try #require(repository.insertFolder())
        let viewModel = makeViewModel()
        let content = "  first <tag> & \"quote\"\n\t日本語\n  "
        let xml = """
        <folders>
            <folder>
                <title>日本語 &amp; Folder</title>
                <snippets>
                    <snippet>
                        <title>Snippet &lt;1&gt;</title>
                        <content>  first &lt;tag&gt; &amp; "quote"\n\t日本語\n  </content>
                    </snippet>
                </snippets>
            </folder>
            <folder><title>Empty</title><snippets/></folder>
        </folders>
        """

        try withTemporaryXML(Data(xml.utf8)) { url in
            viewModel.importSnippets(from: url)
        }

        let folders = repository.fetchFolderDetails()
        #expect(folders.map(\.folder.title) == [existing.title, "日本語 & Folder", "Empty"])
        let imported = try #require(folders.dropFirst().first)
        #expect(imported.snippets.map(\.title) == ["Snippet <1>"])
        #expect(imported.snippets.map(\.content) == [content])
        #expect(folders.last?.snippets.isEmpty == true)
        #expect(viewModel.folders.map(\.id) == folders.map(\.folder.id))
        #expect(viewModel.selection == .folder(existing.id))
        #expect(viewModel.structureVersion == 1)
    }

    @Test
    func exportedXMLPreservesTitlesContentsAndOrderWhenImported() throws {
        let content = "  <tag attr=\"value\"> & 日本語 📝\n\tsecond line\n  "
        let folders = try #require(repository.insertFolders([
            (title: "Folder & <1>", snippets: [
                (title: "日本語 \"snippet\"", content: content),
                (title: "Empty content", content: "")
            ]),
            (title: "Empty folder", snippets: [])
        ]))
        let exportFile = makeViewModel().makeExportFile()
        for folder in folders {
            repository.deleteFolder(folder.folder.id)
        }
        let viewModel = makeViewModel()

        try withTemporaryXML(exportFile.data) { url in
            viewModel.importSnippets(from: url)
        }

        let imported = repository.fetchFolderDetails()
        #expect(imported.map(\.folder.title) == ["Folder & <1>", "Empty folder"])
        let first = try #require(imported.first)
        #expect(first.snippets.map(\.title) == ["日本語 \"snippet\"", "Empty content"])
        #expect(first.snippets.map(\.content) == [content, ""])
        #expect(imported.last?.snippets.isEmpty == true)
        #expect(viewModel.folders.map(\.id) == imported.map(\.folder.id))
    }

    private func makeViewModel() -> SnippetsEditorViewModel {
        withDependencies {
            $0.snippetRepository = repository
        } operation: {
            SnippetsEditorViewModel()
        }
    }

    private func withTemporaryXML(_ data: Data, operation: (URL) throws -> Void) throws {
        let directory = FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString, isDirectory: true)
        try FileManager.default.createDirectory(at: directory, withIntermediateDirectories: true)
        defer { try? FileManager.default.removeItem(at: directory) }
        let url = directory.appendingPathComponent("snippets.xml")
        try data.write(to: url)
        try operation(url)
    }
}
