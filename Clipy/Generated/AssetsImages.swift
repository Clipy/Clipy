// swiftlint:disable all
// Generated using SwiftGen — https://github.com/SwiftGen/SwiftGen

#if os(macOS)
  import AppKit
#elseif os(iOS)
  import UIKit
#elseif os(tvOS) || os(watchOS)
  import UIKit
#endif

// Deprecated typealiases
@available(*, deprecated, renamed: "ImageAsset.Image", message: "This typealias will be removed in SwiftGen 7.0")
internal typealias AssetImageTypeAlias = ImageAsset.Image

// swiftlint:disable superfluous_disable_command file_length implicit_return

// MARK: - Asset Catalogs

// swiftlint:disable identifier_name line_length nesting type_body_length type_name
internal enum Asset {
  internal static let iconFolder = ImageAsset(name: "icon_folder")
  internal static let iconText = ImageAsset(name: "icon_text")
  internal static let prefBeta = ImageAsset(name: "pref_beta")
  internal static let prefBetaOn = ImageAsset(name: "pref_beta_on")
  internal static let prefExcluded = ImageAsset(name: "pref_excluded")
  internal static let prefExcludedOn = ImageAsset(name: "pref_excluded_on")
  internal static let prefGeneral = ImageAsset(name: "pref_general")
  internal static let prefGeneralOn = ImageAsset(name: "pref_general_on")
  internal static let prefMenu = ImageAsset(name: "pref_menu")
  internal static let prefMenuOn = ImageAsset(name: "pref_menu_on")
  internal static let prefShortcut = ImageAsset(name: "pref_shortcut")
  internal static let prefShortcutOn = ImageAsset(name: "pref_shortcut_on")
  internal static let prefType = ImageAsset(name: "pref_type")
  internal static let prefTypeOn = ImageAsset(name: "pref_type_on")
  internal static let prefUpdate = ImageAsset(name: "pref_update")
  internal static let prefUpdateOn = ImageAsset(name: "pref_update_on")
  internal static let snippetsAddFolder = ImageAsset(name: "snippets_add_folder")
  internal static let snippetsAddFolderOn = ImageAsset(name: "snippets_add_folder_on")
  internal static let snippetsAddSnippet = ImageAsset(name: "snippets_add_snippet")
  internal static let snippetsAddSnippetOn = ImageAsset(name: "snippets_add_snippet_on")
  internal static let snippetsDeleteSnippet = ImageAsset(name: "snippets_delete_snippet")
  internal static let snippetsDeleteSnippetOn = ImageAsset(name: "snippets_delete_snippet_on")
  internal static let snippetsEnableSnippet = ImageAsset(name: "snippets_enable_snippet")
  internal static let snippetsEnableSnippetOn = ImageAsset(name: "snippets_enable_snippet_on")
  internal static let snippetsExport = ImageAsset(name: "snippets_export")
  internal static let snippetsExportOn = ImageAsset(name: "snippets_export_on")
  internal static let snippetsIconFolderBlue = ImageAsset(name: "snippets_icon_folder_blue")
  internal static let snippetsIconFolderWhite = ImageAsset(name: "snippets_icon_folder_white")
  internal static let snippetsImport = ImageAsset(name: "snippets_import")
  internal static let snippetsImportOn = ImageAsset(name: "snippets_import_on")
  internal static let statusbarMenuBlack = ImageAsset(name: "statusbar_menu_black")
  internal static let statusbarMenuWhite = ImageAsset(name: "statusbar_menu_white")
}
// swiftlint:enable identifier_name line_length nesting type_body_length type_name

// MARK: - Implementation Details

internal struct ImageAsset {
  internal fileprivate(set) var name: String

  #if os(macOS)
  internal typealias Image = NSImage
  #elseif os(iOS) || os(tvOS) || os(watchOS)
  internal typealias Image = UIImage
  #endif

  @available(iOS 8.0, tvOS 9.0, watchOS 2.0, macOS 10.7, *)
  internal var image: Image {
    let bundle = BundleToken.bundle
    #if os(iOS) || os(tvOS)
    let image = Image(named: name, in: bundle, compatibleWith: nil)
    #elseif os(macOS)
    let name = NSImage.Name(self.name)
    let image = (bundle == .main) ? NSImage(named: name) : bundle.image(forResource: name)
    #elseif os(watchOS)
    let image = Image(named: name)
    #endif
    guard let result = image else {
      fatalError("Unable to load image asset named \(name).")
    }
    return result
  }

  #if os(iOS) || os(tvOS)
  @available(iOS 8.0, tvOS 9.0, *)
  internal func image(compatibleWith traitCollection: UITraitCollection) -> Image {
    let bundle = BundleToken.bundle
    guard let result = Image(named: name, in: bundle, compatibleWith: traitCollection) else {
      fatalError("Unable to load image asset named \(name).")
    }
    return result
  }
  #endif
}

internal extension ImageAsset.Image {
  @available(iOS 8.0, tvOS 9.0, watchOS 2.0, *)
  @available(macOS, deprecated,
    message: "This initializer is unsafe on macOS, please use the ImageAsset.image property")
  convenience init!(asset: ImageAsset) {
    #if os(iOS) || os(tvOS)
    let bundle = BundleToken.bundle
    self.init(named: asset.name, in: bundle, compatibleWith: nil)
    #elseif os(macOS)
    self.init(named: NSImage.Name(asset.name))
    #elseif os(watchOS)
    self.init(named: asset.name)
    #endif
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
