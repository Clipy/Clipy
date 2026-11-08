import AppKit
import Testing
@testable import AlternativeMenu

@MainActor
struct AlternativeMenuTests {
    enum Factory: CaseIterable {
        case action, submenu, disabledAction, disabledSubmenu, sectionHeader

        func makeItem(attributedTitle: NSAttributedString) -> AlternativeMenuItem {
            switch self {
            case .action: .action(attributedTitle: attributedTitle) { _ in }
            case .submenu: .submenu(attributedTitle: attributedTitle, menu: .init(items: []))
            case .disabledAction: .action(attributedTitle: attributedTitle, isEnabled: false) { _ in }
            case .disabledSubmenu: .submenu(attributedTitle: attributedTitle, menu: .init(items: []), isEnabled: false)
            case .sectionHeader: .sectionHeader(attributedTitle: attributedTitle)
            }
        }
    }

    @Test(arguments: Factory.allCases, ["", "Formatted title"])
    func attributedTitleKeepsItsTextAndAttributesAfterSourceChanges(_ factory: Factory, title: String) {
        let font = NSFont.systemFont(ofSize: 24)
        let source = NSMutableAttributedString(string: title, attributes: [.font: font, .foregroundColor: NSColor.red, .kern: 2])
        let expected = NSAttributedString(attributedString: source)
        let item = factory.makeItem(attributedTitle: source)

        source.setAttributes([.font: NSFont.systemFont(ofSize: 12)], range: NSRange(location: 0, length: source.length))
        source.mutableString.setString("Changed title")

        #expect(item.title == title)
        guard case .attributed(let attributedTitle) = item.titleContent else {
            #expect(Bool(false), "An attributed title must retain its formatting")
            return
        }
        #expect(attributedTitle.string == item.title)
        #expect(attributedTitle.isEqual(to: expected))
    }

    @Test(arguments: ["", "Plain title"])
    func plainTitleIsStoredWithoutAttributes(_ title: String) {
        let item = AlternativeMenuItem.action(title) { _ in }

        #expect(item.title == title)
        guard case .plain(let plainTitle) = item.titleContent else {
            #expect(Bool(false), "A plain title must not acquire attributes")
            return
        }
        #expect(plainTitle == title)
    }

    @Test(arguments: [false, true], [false, true])
    func submenuEnabledStateIsIndependentOfItsChildren(isEnabled: Bool, hasChildren: Bool) {
        let children: [AlternativeMenuItem] = hasChildren ? [.action("Child") { _ in }] : []
        let menu = AlternativeMenu(items: children)
        let items: [AlternativeMenuItem] = [
            .submenu("Submenu", menu: menu, isEnabled: isEnabled),
            .submenu(attributedTitle: NSAttributedString(string: "Submenu"), menu: menu, isEnabled: isEnabled)
        ]

        for item in items {
            #expect(item.isEnabled == isEnabled)
            #expect(item.submenu?.items.count == children.count)
        }
        #expect(!AlternativeMenuItem.sectionHeader("Header").isEnabled)
        #expect(!AlternativeMenuItem.separator.isEnabled)
    }
}
