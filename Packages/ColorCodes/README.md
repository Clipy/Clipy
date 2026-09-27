# ColorCodes

A macOS Swift package that parses color code strings into `NSColor` values.
Requires Swift 6.0 or later and macOS 13 or later.

```swift
import ColorCodes

let color = ColorCode("oklch(0.627955 0.257683 29.2339)")?.color
let namedColor = ColorCode("tomato", options: [.cssKeyword])?.color
let bareHex = ColorCode("ff8942", options: [.bareHex])?.color
```

Supported forms include hex, `rgb()`, `rgba()`, `hsl()`, `hsla()`, `hwb()`,
`lab()`, `lch()`, `oklab()`, `oklch()`, and predefined `color()` spaces.
CSS named colors and hex without `#` are opt-in. Parsing requires the whole
string to be a color code, except for surrounding whitespace.

The package has no input-length limit. Clipy applies its 128-character preview
limit before calling the parser. Existing color conversion behavior is retained:
some color spaces are converted and clipped to sRGB. Full CSS conformance,
`calc()`, relative colors, and `color-mix()` are not currently supported.
