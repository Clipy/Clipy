import Settings
import Sharing
import SwiftUI

struct GeneralSettingsView: View {
    @Shared(.isLaunchAtLogin)
    private var isLaunchAtLogin
    @Shared(.pastesAutomatically)
    private var pastesAutomatically
    @Shared(.maximumHistoryCount)
    private var maximumHistoryCount
    @Shared(.reordersClipsAfterPasting)
    private var reordersClipsAfterPasting
    @Shared(.statusItemDisplayMode)
    private var statusItemDisplayMode
    @Shared(.collectsCrashReports)
    private var collectsCrashReports

    var body: some View {
        SettingsGrid {
            SettingsSection(title: .Settings.behavior, bottomDivider: true) {
                Toggle(
                    .Settings.launchOnLogin,
                    isOn: Binding($isLaunchAtLogin)
                )
                Toggle(
                    .Settings.pasteAutomaticallyAfterSelection,
                    isOn: Binding($pastesAutomatically)
                )
            }

            SettingsSection(title: .Settings.historyLimit) {
                SettingsNumberField(
                    value: Binding($maximumHistoryCount),
                    in: 1...1000
                )
            }

            SettingsSection(title: .Settings.sortHistoryBy) {
                Picker(
                    .Settings.sortHistoryBy,
                    selection: Binding($reordersClipsAfterPasting)
                ) {
                    Text(.Settings.dateCreated)
                        .tag(false)
                    Text(.Settings.lastUsed)
                        .tag(true)
                }
                .labelsHidden()
            }

            SettingsSection(title: .Settings.menuBarIcon, bottomDivider: true) {
                Picker(
                    .Settings.menuBarIcon,
                    selection: Binding($statusItemDisplayMode)
                ) {
                    Image(.statusbarMenuBlack)
                        .accessibilityLabel(Text(.Settings.standard))
                        .tag(1)
                    Image(.statusbarMenuWhite)
                        .accessibilityLabel(Text(.Settings.reversed))
                        .tag(2)
                    Text(.Settings.hidden)
                        .tag(0)
                }
                .labelsHidden()
            }

            SettingsSection(title: .Settings.diagnostics) {
                Toggle(
                    .Settings.sendCrashReportsAndUsageLogs,
                    isOn: Binding($collectsCrashReports)
                )
                Text(.Settings.changesTakeEffectTheNextTimeClipyLaunchesUsageLogsDoNotIncludePersonalInformationOrCopiedContent)
                    .settingDescription()
            }
        }
    }
}
