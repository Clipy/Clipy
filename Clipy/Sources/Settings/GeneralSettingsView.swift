import Settings
import Sharing
import SwiftUI
import UniformTypeIdentifiers

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
    @Shared(.isSnippetSyncEnabled)
    private var isSnippetSyncEnabled
    @Shared(.snippetSyncFolderPath)
    private var snippetSyncFolderPath

    @State
    private var isChoosingSyncFolder = false

    private var syncToggleBinding: Binding<Bool> {
        Binding(
            get: { isSnippetSyncEnabled },
            set: { newValue in
                if newValue, snippetSyncFolderPath == nil, let defaultURL = Self.defaultSyncFolderURL {
                    $snippetSyncFolderPath.withLock { $0 = defaultURL.path }
                }
                $isSnippetSyncEnabled.withLock { $0 = newValue }
            }
        )
    }

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

            SettingsSection(title: .Settings.snippetSync, bottomDivider: true) {
                Toggle(
                    .Settings.syncSnippetsThroughASharedFolder,
                    isOn: syncToggleBinding
                )
                Text(.Settings.snippetsAndSnippetFoldersAreKeptInSyncWithYourOtherMacsThroughTheFolderBelowClipboardHistoryIsNotSynced)
                    .settingDescription()

                HStack {
                    Image(systemName: "folder")
                        .foregroundStyle(.secondary)
                    Text(snippetSyncFolderPath ?? String(localized: .Settings.noFolderSelected))
                        .foregroundStyle(snippetSyncFolderPath == nil ? Color.secondary : Color.primary)
                        .lineLimit(1)
                        .truncationMode(.middle)
                    Spacer()
                    Button(.Settings.chooseFolder) {
                        isChoosingSyncFolder = true
                    }
                }
                .padding(.leading, 20)
                .disabled(!isSnippetSyncEnabled)
                Text(.Settings.anySyncedFolderWorksSuchAsICloudDriveGoogleDriveOrDropbox)
                    .settingDescription()
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
        .fileImporter(
            isPresented: $isChoosingSyncFolder,
            allowedContentTypes: [.folder],
            allowsMultipleSelection: false,
            onCompletion: handleSyncFolderSelection
        )
    }
}

private extension GeneralSettingsView {
    static var defaultSyncFolderURL: URL? {
        let url = FileManager.default.homeDirectoryForCurrentUser
            .appendingPathComponent("Library/Mobile Documents/com~apple~CloudDocs", isDirectory: true)
        var isDirectory: ObjCBool = false
        guard FileManager.default.fileExists(atPath: url.path, isDirectory: &isDirectory), isDirectory.boolValue else {
            return nil
        }
        return url
    }

    func handleSyncFolderSelection(_ result: Result<[URL], Error>) {
        guard case let .success(urls) = result, let url = urls.first else { return }
        $snippetSyncFolderPath.withLock { $0 = url.path }
    }
}
