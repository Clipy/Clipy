import Quick
import Nimble
@testable import Clipy

class DraggedDataSpec: QuickSpec {
    override func spec() {

        describe("NSCoding") {

            it("Archive data") {
                let draggedData = CPYDraggedData(type: .folder, folderIdentifier: NSUUID().uuidString, snippetIdentifier: nil, index: 10)
                let data = NSKeyedArchiver.archivedData(withRootObject: draggedData)

                let unarchiveData = NSKeyedUnarchiver.unarchiveObject(with: data) as? CPYDraggedData
                expect(unarchiveData).toNot(beNil())
                expect(unarchiveData?.type).to(equal(draggedData.type))
                expect(unarchiveData?.folderIdentifier).to(equal(draggedData.folderIdentifier))
                expect(unarchiveData?.snippetIdentifier).to(beNil())
                expect(unarchiveData?.index).to(equal(draggedData.index))
            }

        }

    }
}
