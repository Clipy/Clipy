import Foundation
import Testing
@testable import Clipy

@Suite
struct DraggedDataTests {
    @Test
    func archiveData() throws {
        let draggedData = CPYDraggedData(type: .folder, folderIdentifier: UUID().uuidString, snippetIdentifier: nil, index: 10)
        let data = NSKeyedArchiver.archivedData(withRootObject: draggedData)

        let unarchiveData = try #require(NSKeyedUnarchiver.unarchiveObject(with: data) as? CPYDraggedData)
        #expect(unarchiveData.type == draggedData.type)
        #expect(unarchiveData.folderIdentifier == draggedData.folderIdentifier)
        #expect(unarchiveData.snippetIdentifier == nil)
        #expect(unarchiveData.index == draggedData.index)
    }
}
