import Dependencies
import SwiftUI

struct UpdateSettingsView: View {
    @Dependency(\.sparkle)
    private var sparkle

    @State
    private var automaticallyChecksForUpdates = false
    @State
    private var updateCheckInterval: TimeInterval = 86_400
    @State
    private var lastUpdateCheckDate: Date?
    @State
    private var canCheckForUpdates = false

    var body: some View {
        VStack {
            SettingsGrid {
                SettingsSection(title: .Settings.softwareUpdate) {
                    Toggle(
                        .Settings.automaticallyCheckForUpdates,
                        isOn: Binding(
                            get: { automaticallyChecksForUpdates },
                            set: { sparkle.setAutomaticallyChecksForUpdates($0) }
                        )
                    )
                    Picker(
                        .Settings.checkInterval,
                        selection: Binding(
                            get: { updateCheckInterval },
                            set: { sparkle.setUpdateCheckInterval($0) }
                        )
                    ) {
                        Text(.Settings.daily)
                            .tag(TimeInterval(86_400))
                        Text(.Settings.weekly)
                            .tag(TimeInterval(604_800))
                        Text(.Settings.monthly)
                            .tag(TimeInterval(2_592_000))
                    }
                    .labelsHidden()
                    .padding(.leading, 20)
                    .disabled(!automaticallyChecksForUpdates)
                }

                SettingsSection(title: .Settings.lastChecked, bottomDivider: true) {
                    if let lastUpdateCheckDate {
                        Text(lastUpdateCheckDate, format: .dateTime.year().month().day().weekday().hour().minute().second())
                    } else {
                        Text(.Settings.never)
                            .foregroundStyle(.secondary)
                    }
                    Button(.Settings.checkNow) {
                        sparkle.checkForUpdates(sender: nil)
                    }
                    .disabled(!canCheckForUpdates)
                }

                SettingsSection(title: .Settings.version) {
                    Text(verbatim: Bundle.main.appVersion.map { "v\($0)" } ?? "—")
                        .textSelection(.enabled)
                }
            }
        }
        .onReceive(sparkle.automaticallyChecksForUpdates()) { isEnabled in
            automaticallyChecksForUpdates = isEnabled
        }
        .onReceive(sparkle.updateCheckInterval()) { interval in
            updateCheckInterval = interval
        }
        .onReceive(sparkle.lastUpdateCheckDate()) { date in
            lastUpdateCheckDate = date
        }
        .onReceive(sparkle.canCheckForUpdates()) { isEnabled in
            canCheckForUpdates = isEnabled
        }
    }
}
