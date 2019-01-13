// swiftlint:disable all
// Generated using SwiftGen, by O.Halligon — https://github.com/SwiftGen/SwiftGen

#if os(OSX)
  import AppKit.NSImage
  internal typealias AssetColorTypeAlias = NSColor
  internal typealias AssetImageTypeAlias = NSImage
#elseif os(iOS) || os(tvOS) || os(watchOS)
  import UIKit.UIImage
  internal typealias AssetColorTypeAlias = UIColor
  internal typealias AssetImageTypeAlias = UIImage
#endif

// swiftlint:disable superfluous_disable_command
// swiftlint:disable file_length

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

internal struct ColorAsset {
  internal fileprivate(set) var name: String

  @available(iOS 11.0, tvOS 11.0, watchOS 4.0, OSX 10.13, *)
  internal var color: AssetColorTypeAlias {
    return AssetColorTypeAlias(asset: self)
  }
}

internal extension AssetColorTypeAlias {
  @available(iOS 11.0, tvOS 11.0, watchOS 4.0, OSX 10.13, *)
  convenience init!(asset: ColorAsset) {
    let bundle = Bundle(for: BundleToken.self)
    #if os(iOS) || os(tvOS)
    self.init(named: asset.name, in: bundle, compatibleWith: nil)
    #elseif os(OSX)
    self.init(named: NSColor.Name(asset.name), bundle: bundle)
    #elseif os(watchOS)
    self.init(named: asset.name)
    #endif
  }
}

internal struct DataAsset {
  internal fileprivate(set) var name: String

  #if os(iOS) || os(tvOS) || os(OSX)
  @available(iOS 9.0, tvOS 9.0, OSX 10.11, *)
  internal var data: NSDataAsset {
    return NSDataAsset(asset: self)
  }
  #endif
}

#if os(iOS) || os(tvOS) || os(OSX)
@available(iOS 9.0, tvOS 9.0, OSX 10.11, *)
internal extension NSDataAsset {
  convenience init!(asset: DataAsset) {
    let bundle = Bundle(for: BundleToken.self)
    #if os(iOS) || os(tvOS)
    self.init(name: asset.name, bundle: bundle)
    #elseif os(OSX)
    self.init(name: NSDataAsset.Name(asset.name), bundle: bundle)
    #endif
  }
}
#endif

internal struct ImageAsset {
  internal fileprivate(set) var name: String

  internal var image: AssetImageTypeAlias {
    let bundle = Bundle(for: BundleToken.self)
    #if os(iOS) || os(tvOS)
    let image = AssetImageTypeAlias(named: name, in: bundle, compatibleWith: nil)
    #elseif os(OSX)
    let image = bundle.image(forResource: NSImage.Name(name))
    #elseif os(watchOS)
    let image = AssetImageTypeAlias(named: name)
    #endif
    guard let result = image else { fatalError("Unable to load image named \(name).") }
    return result
  }
}

internal extension AssetImageTypeAlias {
  @available(iOS 1.0, tvOS 1.0, watchOS 1.0, *)
  @available(OSX, deprecated,
    message: "This initializer is unsafe on macOS, please use the ImageAsset.image property")
  convenience init!(asset: ImageAsset) {
    #if os(iOS) || os(tvOS)
    let bundle = Bundle(for: BundleToken.self)
    self.init(named: asset.name, in: bundle, compatibleWith: nil)
    #elseif os(OSX)
    self.init(named: NSImage.Name(asset.name))
    #elseif os(watchOS)
    self.init(named: asset.name)
    #endif
  }
}

private final class BundleToken {}
