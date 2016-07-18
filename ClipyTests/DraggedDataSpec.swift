import Quick
import Nimble
@testable import Clipy

class DraggedDataSpec: QuickSpec {
    override func spec() {

        describe("NSCoding") {

            it("Archive data") {
                let draggedData = CPYDraggedData(type: .Folder, folderIdentifier: NSUUID().UUIDString, snippetIdentifier: nil, index: 10)
                let data = NSKeyedArchiver.archivedDataWithRootObject(draggedData)

                let unarchiveData = NSKeyedUnarchiver.unarchiveObjectWithData(data) as? CPYDraggedData
                expect(unarchiveData).toNot(beNil())
                expect(unarchiveData?.type).to(equal(draggedData.type))
                expect(unarchiveData?.folderIdentifier).to(equal(draggedData.folderIdentifier))
                expect(unarchiveData?.snippetIdentifier).to(beNil())
                expect(unarchiveData?.index).to(equal(draggedData.index))
            }

        }

    }
}
