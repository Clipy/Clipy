import Foundation
import Testing
@testable import Clipy

@MainActor
struct SnippetsEditorTests {
    @Test
    func exportFileNameUsesGregorianLocalDate() throws {
        var utcCalendar = Calendar(identifier: .gregorian)
        utcCalendar.timeZone = try #require(TimeZone(secondsFromGMT: 0))
        let date = try #require(
            utcCalendar.date(
                from: DateComponents(year: 2026, month: 9, day: 29, hour: 16, minute: 30)
            )
        )
        let taipeiTimeZone = try #require(TimeZone(secondsFromGMT: 8 * 60 * 60))

        #expect(
            CPYSnippetsEditorWindowController.snippetExportFileName(for: date, timeZone: taipeiTimeZone)
                == "snippets_20260930.xml"
        )
    }
}
