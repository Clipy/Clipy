import Settings
import Sharing
import SwiftUI

struct BetaSettingsView: View {
    @Shared(.allowsAgentHistoryAccess)
    private var allowsAgentHistoryAccess

    @Shared(.pastesPlainTextWithModifier)
    private var pastesPlainTextWithModifier
    @Shared(.plainTextPasteModifier)
    private var plainTextPasteModifier
    @Shared(.deletesHistoryWithModifier)
    private var deletesHistoryWithModifier
    @Shared(.historyDeletionModifier)
    private var historyDeletionModifier
    @Shared(.pastesAndDeletesHistoryWithModifier)
    private var pastesAndDeletesHistoryWithModifier
    @Shared(.pasteAndDeleteHistoryModifier)
    private var pasteAndDeleteHistoryModifier
    @Shared(.observesScreenshots)
    private var observesScreenshots

    var body: some View {
        VStack(spacing: 0) {
            Text(.Settings.betaFeatureSettingsMayNotBePreservedAfterAnUpdate)
                .foregroundStyle(.secondary)
                .frame(width: 500)
                .padding(.top, 20)

            SettingsGrid {
                SettingsSection(title: "AI agent access", bottomDivider: true) {
                    Toggle("Allow agents to query text history", isOn: Binding($allowsAgentHistoryAccess))
                    Text("The optional local MCP bridge reads saved text history. Your agent may share it with its AI provider. Only connect trusted clients: copied text can contain passwords and private data. Disabling access stops future queries but cannot recall shared data.")
                        .settingDescription()
                }

                SettingsSection(title: .Settings.modifierKeyActions, bottomDivider: true) {
                    Toggle(
                        .Settings.pasteAsPlainText,
                        isOn: Binding($pastesPlainTextWithModifier)
                    )
                    modifierKeyPicker(
                        .Settings.pasteAsPlainText,
                        selection: Binding($plainTextPasteModifier)
                    )
                    .disabled(!pastesPlainTextWithModifier)

                    Toggle(
                        .Settings.deleteFromHistory,
                        isOn: Binding($deletesHistoryWithModifier)
                    )
                    modifierKeyPicker(
                        .Settings.deleteFromHistory,
                        selection: Binding($historyDeletionModifier)
                    )
                    .disabled(!deletesHistoryWithModifier)

                    Toggle(
                        .Settings.pasteAndDeleteFromHistory,
                        isOn: Binding($pastesAndDeletesHistoryWithModifier)
                    )
                    modifierKeyPicker(
                        .Settings.pasteAndDeleteFromHistory,
                        selection: Binding($pasteAndDeleteHistoryModifier)
                    )
                    .disabled(!pastesAndDeletesHistoryWithModifier)

                    Text(.Settings.holdTheSelectedKeyWhileChoosingAHistoryItemToPerformTheAction)
                        .settingDescription()
                }

                SettingsSection(title: .Settings.screenshot) {
                    Toggle(
                        .Settings.saveScreenshotsInHistory,
                        isOn: Binding($observesScreenshots)
                    )
                }
            }
        }
    }

    private func modifierKeyPicker(
        _ title: LocalizedStringResource,
        selection: Binding<Int>
    ) -> some View {
        Picker(title, selection: selection) {
            Text(.Settings.command)
                .tag(0)
            Text(.Settings.shift)
                .tag(1)
            Text(.Settings.control)
                .tag(2)
            Text(.Settings.option)
                .tag(3)
        }
        .labelsHidden()
        .fixedSize(horizontal: true, vertical: false)
        .padding(.leading, 20)
    }
}
