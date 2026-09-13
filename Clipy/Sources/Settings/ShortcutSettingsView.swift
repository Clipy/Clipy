import Dependencies
import Sharing
import SwiftUI

struct ShortcutSettingsView: View {
    @Dependency(\.hotKeyService)
    private var hotKeyService

    @Shared(.mainKeyCombo)
    private var mainKeyCombo
    @Shared(.historyKeyCombo)
    private var historyKeyCombo
    @Shared(.snippetKeyCombo)
    private var snippetKeyCombo
    @Shared(.clearHistoryKeyCombo)
    private var clearHistoryKeyCombo

    var body: some View {
        SettingsGrid(minimumTitleWidth: 150) {
            SettingsSection(title: .Settings.mainMenu, verticalAlignment: .center) {
                ShortcutRecorder(keyCombo: mainKeyCombo) {
                    hotKeyService.change(with: .main, keyCombo: $0)
                }
                .frame(width: 240, height: 32)
                .frame(maxWidth: .infinity, alignment: .leading)
            }

            SettingsSection(title: .Settings.history, verticalAlignment: .center) {
                ShortcutRecorder(keyCombo: historyKeyCombo) {
                    hotKeyService.change(with: .history, keyCombo: $0)
                }
                .frame(width: 240, height: 32)
                .frame(maxWidth: .infinity, alignment: .leading)
            }

            SettingsSection(title: .Settings.snippets, bottomDivider: true, verticalAlignment: .center) {
                ShortcutRecorder(keyCombo: snippetKeyCombo) {
                    hotKeyService.change(with: .snippet, keyCombo: $0)
                }
                .frame(width: 240, height: 32)
                .frame(maxWidth: .infinity, alignment: .leading)
            }

            SettingsSection(title: .Settings.clearHistory, verticalAlignment: .center) {
                ShortcutRecorder(keyCombo: clearHistoryKeyCombo) {
                    hotKeyService.changeClearHistoryKeyCombo($0)
                }
                .frame(width: 240, height: 32)
                .frame(maxWidth: .infinity, alignment: .leading)
            }
        }
    }
}
