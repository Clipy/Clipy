import Settings
import Sharing
import SwiftUI

struct ClipboardTypeSettingsView: View {
    @Shared(.pasteboardTypeSettings) private var pasteboardTypeSettings
    @Shared(.ignoresConcealedPasteboardTypes) private var ignoresConcealedPasteboardTypes
    @Shared(.ignoresUniversalClipboard) private var ignoresUniversalClipboard

    var body: some View {
        SettingsGrid {
            SettingsSection(title: .Settings.saveToHistory, bottomDivider: true) {
                ForEach(PasteboardAvailableType.allCases, id: \.self) { type in
                    HStack {
                        Image(systemName: type.symbolName)
                            .font(.system(size: 11))
                            .foregroundStyle(.secondary)
                            .frame(width: 22, height: 22)
                            .background(.quaternary, in: RoundedRectangle(cornerRadius: 4))
                            .opacity(pasteboardTypeSettings[type] ? 1 : 0.4)
                            .accessibilityHidden(true)
                        Toggle(
                            type.description,
                            isOn: Binding($pasteboardTypeSettings[type])
                        )
                    }
                }
            }

            SettingsSection {
                Toggle(
                    .Settings.doNotSaveContentMarkedAsConfidentialToHistory,
                    isOn: Binding($ignoresConcealedPasteboardTypes)
                )
                Text(.Settings.someAppsSuchAsPasswordManagersMarkSensitiveContentAsConfidential)
                    .settingDescription()

                Toggle(
                    .Settings.doNotSaveUniversalClipboardContentToHistory,
                    isOn: Binding($ignoresUniversalClipboard)
                )
                Text(.Settings.contentCopiedOnOtherAppleDevicesWillNotBeSavedToHistory)
                    .settingDescription()
            }
        }
    }
}

private extension PasteboardAvailableType {
    var description: LocalizedStringResource {
        switch self {
        case .string:
            .Settings.plainText
        case .rtf:
            .Settings.richTextFormatRtf
        case .rtfd:
            .Settings.richTextFormatDirectoryRtfd
        case .pdf:
            .Settings.pdf
        case .filenames:
            .Settings.files
        case .url:
            .Settings.url
        case .tiff:
            .Settings.images
        case .html:
            .Settings.html
        }
    }
}
