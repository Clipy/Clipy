// Generated using SwiftGen, by O.Halligon — https://github.com/SwiftGen/SwiftGen

import Foundation

// swiftlint:disable superfluous_disable_command
// swiftlint:disable file_length

// swiftlint:disable explicit_type_interface identifier_name line_length nesting type_body_length type_name
internal enum L10n {
  /// Add
  internal static let add = L10n.tr("Localizable", "Add")
  /// Are you sure want to delete this item?
  internal static let areYouSureWantToDeleteThisItem = L10n.tr("Localizable", "Are you sure want to delete this item?")
  /// Are you sure you want to clear your clipboard history?
  internal static let areYouSureYouWantToClearYourClipboardHistory = L10n.tr("Localizable", "Are you sure you want to clear your clipboard history?")
  /// Cancel
  internal static let cancel = L10n.tr("Localizable", "Cancel")
  /// Clear History
  internal static let clearHistory = L10n.tr("Localizable", "Clear History")
  /// Delete Item
  internal static let deleteItem = L10n.tr("Localizable", "Delete Item")
  /// Don't Launch
  internal static let donTLaunch = L10n.tr("Localizable", "Don't Launch")
  /// Edit Snippets...
  internal static let editSnippets = L10n.tr("Localizable", "Edit Snippets")
  /// General
  internal static let general = L10n.tr("Localizable", "General")
  /// History
  internal static let history = L10n.tr("Localizable", "History")
  /// Launch Clipy on system startup?
  internal static let launchClipyOnSystemStartup = L10n.tr("Localizable", "Launch Clipy on system startup?")
  /// Launch on system startup
  internal static let launchOnSystemStartup = L10n.tr("Localizable", "Launch on system startup")
  /// Menu
  internal static let menu = L10n.tr("Localizable", "Menu")
  /// Please fill in the contents of the snippet
  internal static let pleaseFillInTheContentsOfTheSnippet = L10n.tr("Localizable", "Please fill in the contents of the snippet")
  /// Preferences...
  internal static let preferences = L10n.tr("Localizable", "Preferences")
  /// Quit Clipy
  internal static let quitClipy = L10n.tr("Localizable", "Quit Clipy")
  /// Shortcuts
  internal static let shortcuts = L10n.tr("Localizable", "Shortcuts")
  /// Snippet
  internal static let snippet = L10n.tr("Localizable", "Snippet")
  /// Type
  internal static let type = L10n.tr("Localizable", "Type")
  /// Updates
  internal static let updates = L10n.tr("Localizable", "Updates")
  /// You can change this setting in the Preferences if you want.
  internal static let youCanChangeThisSettingInThePreferencesIfYouWant = L10n.tr("Localizable", "You can change this setting in the Preferences if you want")
}
// swiftlint:enable explicit_type_interface identifier_name line_length nesting type_body_length type_name

extension L10n {
  private static func tr(_ table: String, _ key: String, _ args: CVarArg...) -> String {
    let format = NSLocalizedString(key, tableName: table, bundle: Bundle(for: BundleToken.self), comment: "")
    return String(format: format, locale: Locale.current, arguments: args)
  }
}

private final class BundleToken {}
