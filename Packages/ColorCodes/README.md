# ColorCodes

A Swift package for parsing hexadecimal and CSS color strings into AppKit
`NSColor` values. Use it to display color previews or recognize color strings
in macOS apps.

## Requirements

- macOS 14 or later
- Swift 6.0 or later

## Usage

```swift
import ColorCodes

let color = ColorCode("oklch(70% 0.15 50)")?.color // NSColor?
let format = ColorCode("#ff894280")?.type // .hexWithAlpha

ColorCode("  #f84\n") // Surrounding whitespace is ignored.
ColorCode("background: #f84;") // nil: the whole string must be a color value.
```

`ColorCode` has a failable initializer. It returns `nil` when the input cannot
be parsed. A successful result exposes the color through `color` and the
recognized format through `type`.

### Optional formats

CSS named colors and hexadecimal strings without `#` are disabled by default.
Enable either format with `ColorCode.ParseOptions`:

```swift
let named = ColorCode("tomato", options: [.cssNamedColors])
let hex = ColorCode("ff8942", options: [.bareHex])
let either = ColorCode("rebeccapurple", options: [.cssNamedColors, .bareHex])
```

The `.cssNamedColors` option also accepts the `transparent` keyword.
These values have the `.cssNamedColor` type.

These options can also recognize ordinary text as colors. For example,
`.cssNamedColors` accepts words such as `orange` and `tan`, while `.bareHex` accepts
`beef`, `facade`, and `123456`. Choose the options that suit your input source.

## Supported formats

| Format | Examples |
| --- | --- |
| Hexadecimal | `#f84`, `#f848`, `#ff8942`, `#ff894280` |
| RGB | `rgb(255, 137, 66)`, `rgb(100% 50% 0% / 50%)`, `rgba(255, 137, 66, .5)` |
| HSL | `hsl(23 100% 63%)`, `hsla(23, 100%, 63%, .5)` |
| HWB | `hwb(23 26% 0%)` |
| CIE Lab and LCh | `lab(69% 41 63)`, `lch(69% 75 57)` |
| Oklab and OkLCh | `oklab(0.75 0.09 0.12)`, `oklch(0.75 0.15 55)` |
| Predefined color spaces | `color(display-p3 1 0.54 0.26)` |
| CSS named colors (optional) | `tomato`, `rebeccapurple` |
| Hexadecimal without `#` (optional) | `f84`, `ff8942`, `ff894280` |

Hex values with alpha use RGBA order, with alpha in the last digit or byte.
Function names, color-space names, and color names are case-insensitive.
`rgb()` / `rgba()` and `hsl()` / `hsla()` accept both comma-separated and modern
space-separated syntax. The other functions accept space-separated syntax.

The `color()` function supports `srgb`, `srgb-linear`, `display-p3`, `a98-rgb`,
`prophoto-rgb`, `rec2020`, `xyz`, `xyz-d65`, and `xyz-d50`.

## Color conversion and limitations

Most formats return an sRGB color. `color(display-p3 ...)` and
`color(a98-rgb ...)` retain their native Display P3 and Adobe RGB (1998) color
spaces. Other supported spaces are converted to sRGB, with out-of-gamut
components clipped to `0...1`.

ColorCodes implements a subset of CSS color syntax. It does not evaluate
`calc()`, relative colors, `color-mix()`, CSS variables, `currentcolor`, or
system colors. It parses individual color values rather than CSS declarations
or stylesheets. It is not a full CSS conformance validator, and simple gamut
clipping can produce different results from browser gamut mapping.
