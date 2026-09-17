import Settings
import Sharing
import SwiftUI

struct ClipboardTypeSettingsView: View {
    @Shared(.pasteboardTypeSettings) private var pasteboardTypeSettings
    @Shared(.ignoresConcealedPasteboardTypes) private var ignoresConcealedPasteboardTypes

    var body: some View {
        SettingsGrid {
            SettingsSection(title: .Settings.saveToHistory, bottomDivider: true) {
                Toggle(
                    .Settings.plainText,
                    isOn: Binding($pasteboardTypeSettings[.string])
                )

                Toggle(
                    .Settings.richTextFormatRtf,
                    isOn: Binding($pasteboardTypeSettings[.rtf])
                )
                Toggle(
                    .Settings.richTextFormatDirectoryRtfd,
                    isOn: Binding($pasteboardTypeSettings[.rtfd])
                )
                Toggle(
                    .Settings.pdf,
                    isOn: Binding($pasteboardTypeSettings[.pdf])
                )
                Toggle(
                    .Settings.files,
                    isOn: Binding($pasteboardTypeSettings[.filenames])
                )
                Toggle(
                    .Settings.url,
                    isOn: Binding($pasteboardTypeSettings[.url])
                )
                Toggle(
                    .Settings.images,
                    isOn: Binding($pasteboardTypeSettings[.tiff])
                )
                Toggle(
                    .Settings.html,
                    isOn: Binding($pasteboardTypeSettings[.html])
                )
            }

            SettingsSection(title: .Settings.privacy) {
                Toggle(
                    .Settings.doNotSaveContentMarkedAsConfidentialToHistory,
                    isOn: Binding($ignoresConcealedPasteboardTypes)
                )
                Text(.Settings.someAppsSuchAsPasswordManagersMarkSensitiveContentAsConfidential)
                    .settingDescription()
            }
        }
    }
}
