import SwiftUI

struct SettingsSection {
    let title: LocalizedStringResource?
    let content: AnyView
    let bottomDivider: Bool
    let verticalAlignment: VerticalAlignment

    init(
        title: LocalizedStringResource? = nil,
        bottomDivider: Bool = false,
        verticalAlignment: VerticalAlignment = .firstTextBaseline,
        @ViewBuilder content: () -> some View
    ) {
        self.title = title
        self.content = AnyView(VStack(alignment: .leading, content: content))
        self.bottomDivider = bottomDivider
        self.verticalAlignment = verticalAlignment
    }
}

@resultBuilder
enum SettingsSectionBuilder {
    static func buildBlock(_ sections: SettingsSection...) -> [SettingsSection] {
        sections
    }
}
