import AppKit
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
    @Shared(.clearsHistoryOnQuit)
    private var clearsHistoryOnQuit
    @Shared(.clearsHistoryPeriodically)
    private var clearsHistoryPeriodically
    @Shared(.historyClearInterval)
    private var historyClearInterval
    @Shared(.statusItemDisplayMode)
    private var statusItemDisplayMode
    @Shared(.collectsCrashReports)
    private var collectsCrashReports
    @Shared(.snippetExportDirectoryPath)
    private var snippetExportDirectoryPath

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

            SettingsSection(title: .Settings.sortHistoryBy, bottomDivider: true) {
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

            SettingsSection(title: .Settings.automaticHistoryDeletion, bottomDivider: true) {
                Toggle(
                    .Settings.clearHistoryWhenClipyQuits,
                    isOn: Binding($clearsHistoryOnQuit)
                )
                Toggle(
                    .Settings.clearHistoryPeriodically,
                    isOn: Binding($clearsHistoryPeriodically)
                )
                Picker(
                    .Settings.clearHistoryPeriodically,
                    selection: Binding($historyClearInterval)
                ) {
                    ForEach(HistoryClearInterval.allCases, id: \.self) { interval in
                        Text(interval.title).tag(interval)
                    }
                }
                .labelsHidden()
                .padding(.leading, 20)
                .disabled(!clearsHistoryPeriodically)
                Text(.Settings.historyClearIntervalDescription)
                    .settingDescription()
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

            SettingsSection(title: .Settings.snippets, bottomDivider: true) {
                HStack {
                    Text(snippetExportDirectoryPath.isEmpty ? "—" : snippetExportDirectoryPath)
                        .lineLimit(1)
                        .truncationMode(.middle)
                        .frame(maxWidth: .infinity, alignment: .leading)
                    Button {
                        chooseSnippetExportDirectory()
                    } label: {
                        Image(systemName: "folder")
                    }
                }
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

    private func chooseSnippetExportDirectory() {
        let panel = NSOpenPanel()
        panel.allowsMultipleSelection = false
        panel.canChooseFiles = false
        panel.canChooseDirectories = true
        panel.canCreateDirectories = true
        if !snippetExportDirectoryPath.isEmpty {
            panel.directoryURL = URL(fileURLWithPath: snippetExportDirectoryPath, isDirectory: true)
        }

        guard panel.runModal() == .OK, let url = panel.url else { return }
        $snippetExportDirectoryPath.withLock { $0 = url.path }
    }
}
