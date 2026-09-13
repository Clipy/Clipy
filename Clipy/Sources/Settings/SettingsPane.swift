import Settings
import SwiftUI

enum SettingsPane: String, CaseIterable {
    case general
    case menu
    case clipboardType
    case excludedApplications
    case shortcuts
    case updates
    case beta

    var title: String {
        switch self {
        case .general:
            String(localized: .Settings.general)
        case .menu:
            String(localized: .Settings.menu)
        case .clipboardType:
            String(localized: .Settings.paneType)
        case .excludedApplications:
            String(localized: .Settings.exclude)
        case .shortcuts:
            String(localized: .Settings.shortcuts)
        case .updates:
            String(localized: .Settings.updates)
        case .beta:
            String(localized: .Settings.beta)
        }
    }

    private var symbol: String {
        switch self {
        case .general:
            "gearshape"
        case .menu:
            "filemenu.and.selection"
        case .clipboardType:
            "doc.on.clipboard"
        case .excludedApplications:
            "nosign"
        case .shortcuts:
            "keyboard"
        case .updates:
            "arrow.triangle.2.circlepath"
        case .beta:
            "testtube.2"
        }
    }

    @MainActor
    func asPanelConvertible() -> some SettingsPaneConvertible {
        Settings.Pane(
            identifier: Settings.PaneIdentifier(rawValue),
            title: title,
            toolbarIcon: NSImage(systemSymbolName: symbol, accessibilityDescription: title)!
        ) {
            content
        }
    }

    @MainActor
    @ViewBuilder
    private var content: some View {
        switch self {
        case .general:
            GeneralSettingsView()
        case .menu:
            MenuSettingsView()
        case .clipboardType:
            ClipboardTypeSettingsView()
        case .excludedApplications:
            ExcludedApplicationsSettingsView()
        case .shortcuts:
            ShortcutSettingsView()
        case .updates:
            UpdateSettingsView()
        case .beta:
            BetaSettingsView()
        }
    }
}
