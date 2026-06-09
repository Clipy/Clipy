import Foundation
import Testing
@testable import Clipy

@Suite
struct DraggedDataTests {
    @Test
    func archiveData() throws {
        let draggedData = DraggedData(type: .folder, folderID: .init(rawValue: UUID()), snippetID: nil, index: 10)
        let data = try NSKeyedArchiver.archivedData(withRootObject: draggedData, requiringSecureCoding: true)

        let unarchiveData = try #require(NSKeyedUnarchiver.unarchivedObject(ofClasses: [DraggedData.self, NSUUID.self], from: data) as? DraggedData)
        #expect(unarchiveData.type == draggedData.type)
        #expect(unarchiveData.folderID == draggedData.folderID)
        #expect(unarchiveData.snippetID == nil)
        #expect(unarchiveData.index == draggedData.index)
    }
}
