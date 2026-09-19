import SwiftUI

struct SettingsGrid: View {
    private let contentWidth: CGFloat
    private let minimumTitleWidth: CGFloat?
    private let sections: [SettingsSection]

    init(
        contentWidth: CGFloat = 500,
        minimumTitleWidth: CGFloat? = nil,
        @SettingsSectionBuilder content: () -> [SettingsSection]
    ) {
        self.contentWidth = contentWidth
        self.minimumTitleWidth = minimumTitleWidth.map { min($0, contentWidth / 2) }
        sections = content()
    }

    var body: some View {
        Grid(alignment: .trailingFirstTextBaseline) {
            ForEach(sections.indices, id: \.self) { index in
                let section = sections[index]
                GridRow(alignment: section.verticalAlignment) {
                    if let title = section.title {
                        Text(title)
                            .font(.system(size: 13))
                            .multilineTextAlignment(.trailing)
                            .frame(
                                minWidth: minimumTitleWidth,
                                maxWidth: contentWidth / 2,
                                alignment: .trailing
                            )
                            .fixedSize()
                    } else {
                        Color.clear
                            .frame(width: 0, height: 0)
                    }
                    section.content
                        .frame(maxWidth: .infinity, alignment: .leading)
                        .fixedSize(horizontal: false, vertical: true)
                }
                if section.bottomDivider, index != sections.indices.last {
                    Divider()
                        .frame(height: 20)
                        .gridCellUnsizedAxes(.horizontal)
                }
            }
        }
        .frame(width: contentWidth, alignment: .leading)
        .fixedSize(horizontal: false, vertical: true)
        .padding(.vertical, 20)
        .padding(.horizontal, 30)
    }
}
