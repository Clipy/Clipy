// Generated using SwiftGen, by O.Halligon — https://github.com/SwiftGen/SwiftGen

#if os(OSX)
  import AppKit.NSColor
  internal typealias Color = NSColor
#elseif os(iOS) || os(tvOS) || os(watchOS)
  import UIKit.UIColor
  internal typealias Color = UIColor
#endif

// swiftlint:disable superfluous_disable_command
// swiftlint:disable file_length

// swiftlint:disable operator_usage_whitespace
internal extension Color {
  convenience init(rgbaValue: UInt32) {
    let red   = CGFloat((rgbaValue >> 24) & 0xff) / 255.0
    let green = CGFloat((rgbaValue >> 16) & 0xff) / 255.0
    let blue  = CGFloat((rgbaValue >>  8) & 0xff) / 255.0
    let alpha = CGFloat((rgbaValue      ) & 0xff) / 255.0

    self.init(red: red, green: green, blue: blue, alpha: alpha)
  }
}
// swiftlint:enable operator_usage_whitespace

// swiftlint:disable identifier_name line_length type_body_length
internal struct ColorName {
  internal let rgbaValue: UInt32
  internal var color: Color { return Color(named: self) }

  /// <span style="display:block;width:3em;height:2em;border:1px solid black;background:#2a84d2"></span>
  /// Alpha: 100% <br/> (0x2a84d2ff)
  internal static let clipy = ColorName(rgbaValue: 0x2a84d2ff)
  /// <span style="display:block;width:3em;height:2em;border:1px solid black;background:#999999"></span>
  /// Alpha: 100% <br/> (0x999999ff)
  internal static let tabTitle = ColorName(rgbaValue: 0x999999ff)
  /// <span style="display:block;width:3em;height:2em;border:1px solid black;background:#444444"></span>
  /// Alpha: 100% <br/> (0x444444ff)
  internal static let title = ColorName(rgbaValue: 0x444444ff)
}
// swiftlint:enable identifier_name line_length type_body_length

internal extension Color {
  convenience init(named color: ColorName) {
    self.init(rgbaValue: color.rgbaValue)
  }
}
