// swiftlint:disable all
// Generated using SwiftGen — https://github.com/SwiftGen/SwiftGen

import Foundation

// swiftlint:disable superfluous_disable_command file_length implicit_return prefer_self_in_static_references

// MARK: - Strings

// swiftlint:disable explicit_type_interface function_parameter_count identifier_name line_length
// swiftlint:disable nesting type_body_length type_name vertical_whitespace_opening_braces
internal enum L10n {
  /// Add
  internal static let add = L10n.tr("Localizable", "Add", fallback: "Add")
  /// Are you sure want to delete this item?
  internal static let areYouSureWantToDeleteThisItem = L10n.tr("Localizable", "Are you sure want to delete this item?", fallback: "Are you sure want to delete this item?")
  /// Are you sure you want to clear your clipboard history?
  internal static let areYouSureYouWantToClearYourClipboardHistory = L10n.tr("Localizable", "Are you sure you want to clear your clipboard history?", fallback: "Are you sure you want to clear your clipboard history?")
  /// Cancel
  internal static let cancel = L10n.tr("Localizable", "Cancel", fallback: "Cancel")
  /// Clear History
  internal static let clearHistory = L10n.tr("Localizable", "Clear History", fallback: "Clear History")
  /// Delete Item
  internal static let deleteItem = L10n.tr("Localizable", "Delete Item", fallback: "Delete Item")
  /// Don't Launch
  internal static let donTLaunch = L10n.tr("Localizable", "Don't Launch", fallback: "Don't Launch")
  /// Edit Snippets...
  internal static let editSnippets = L10n.tr("Localizable", "Edit Snippets", fallback: "Edit Snippets...")
  /// General
  internal static let general = L10n.tr("Localizable", "General", fallback: "General")
  /// History
  internal static let history = L10n.tr("Localizable", "History", fallback: "History")
  /// Launch Clipy on system startup?
  internal static let launchClipyOnSystemStartup = L10n.tr("Localizable", "Launch Clipy on system startup?", fallback: "Launch Clipy on system startup?")
  /// Launch on system startup
  internal static let launchOnSystemStartup = L10n.tr("Localizable", "Launch on system startup", fallback: "Launch on system startup")
  /// Menu
  internal static let menu = L10n.tr("Localizable", "Menu", fallback: "Menu")
  /// Open System Preferences
  internal static let openSystemPreferences = L10n.tr("Localizable", "Open System Preferences", fallback: "Open System Preferences")
  /// Please allow Accessibility.
  internal static let pleaseAllowAccessibility = L10n.tr("Localizable", "Please allow Accessibility", fallback: "Please allow Accessibility.")
  /// Please fill in the contents of the snippet
  internal static let pleaseFillInTheContentsOfTheSnippet = L10n.tr("Localizable", "Please fill in the contents of the snippet", fallback: "Please fill in the contents of the snippet")
  /// Preferences...
  internal static let preferences = L10n.tr("Localizable", "Preferences", fallback: "Preferences...")
  /// Quit Clipy
  internal static let quitClipy = L10n.tr("Localizable", "Quit Clipy", fallback: "Quit Clipy")
  /// Shortcuts
  internal static let shortcuts = L10n.tr("Localizable", "Shortcuts", fallback: "Shortcuts")
  /// Snippet
  internal static let snippet = L10n.tr("Localizable", "Snippet", fallback: "Snippet")
  /// To do this action please allow Accessibility in Security & Privacy preferences, located in System Preferences.
  internal static let toDoThisActionPleaseAllowAccessibilityInSecurityPrivacyPreferencesLocatedInSystemPreferences = L10n.tr("Localizable", "To do this action please allow Accessibility in Security Privacy preferences located in System Preferences", fallback: "To do this action please allow Accessibility in Security & Privacy preferences, located in System Preferences.")
  /// Type
  internal static let type = L10n.tr("Localizable", "Type", fallback: "Type")
  /// Updates
  internal static let updates = L10n.tr("Localizable", "Updates", fallback: "Updates")
  /// You can change this setting in the Preferences if you want.
  internal static let youCanChangeThisSettingInThePreferencesIfYouWant = L10n.tr("Localizable", "You can change this setting in the Preferences if you want", fallback: "You can change this setting in the Preferences if you want.")
}
// swiftlint:enable explicit_type_interface function_parameter_count identifier_name line_length
// swiftlint:enable nesting type_body_length type_name vertical_whitespace_opening_braces

// MARK: - Implementation Details

extension L10n {
  private static func tr(_ table: String, _ key: String, _ args: CVarArg..., fallback value: String) -> String {
    let format = BundleToken.bundle.localizedString(forKey: key, value: value, table: table)
    return String(format: format, locale: Locale.current, arguments: args)
  }
}

// swiftlint:disable convenience_type
private final class BundleToken {
  static let bundle: Bundle = {
    #if SWIFT_PACKAGE
    return Bundle.module
    #else
    return Bundle(for: BundleToken.self)
    #endif
  }()
}
// swiftlint:enable convenience_type
