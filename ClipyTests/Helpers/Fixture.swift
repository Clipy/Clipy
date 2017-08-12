import Foundation
import AEXML

struct Fixture {

    enum Xml: String {
        case exportHistories = "export_history"

        var xml: AEXMLDocument {
            guard let path = Bundle(for: Dummy.self).path(forResource: rawValue, ofType: "xml") else {
                fatalError("Could not file named \(self.rawValue).xml in test bundle.")
            }
            guard let data = try? Data(contentsOf: URL(fileURLWithPath: path)) else {
                fatalError("Could not read data from file at \(path).")
            }
            do {
                return try AEXMLDocument(xml: data)
            } catch {
                fatalError("Could not read xml from data")
            }
        }
    }

    private class Dummy {}

}
