# AlternativeMenu Development Guidelines

## Purpose and Design

This library uses custom views to let users switch IMEs or input sources and
continue typing while a menu stays open, addressing a limitation encountered
when embedding a text field in `NSMenu`.

- Match the actual `NSMenu` in appearance and behavior. Before implementing or changing menu behavior, reproduce the case in `NSMenu` under the same conditions and use the observed result as the specification. Verify the custom menu against it. Preserve explicitly requested differences, including custom headers and input-source switching.
- The library owns presentation, menu interaction, and action dispatch. The application owns headers, search, data, wording, enabled state, and pasting. Keep search-specific state and settings out of the library.
- Keep implementations and state management simple. Before adding a complex mechanism, discuss its purpose, concrete design, and a smaller alternative with the user.
- Prefer public APIs. Keep the existing private AppKit integrations listed below isolated and availability-checked, and preserve their fallbacks instead of duplicating AppKit behavior.
- Use AppKit's localized accessibility descriptions, with no caller override for the menu's name. Keep application wording in the app and avoid localization resources in the package.
- Keep lasting principles here, API usage in the [README](README.md), and implementation-specific explanations beside the code and tests. Do not add investigation logs or environment-specific paths.

## Accepted Differences from NSMenu

The following intentional extensions and scope limits qualify the parity rule
above. API scope is included even where it does not imply different visible
behavior. Existing bugs and unverified environments are not accepted differences.

| Area | Accepted behavior or scope |
| --- | --- |
| Presentation and IME | Use custom popup panels and the application's normal event handling so input-source switching and composition can continue while open. Do not replace this with native menu tracking. Opening another system menu or losing focus may still dismiss the popup; keeping it open through every input-source selection route is not a requirement. |
| Reopening | Calling `present` on the same session replaces the current popup and its submenu chain at the requested location. It does not stack another tracking loop. This does not change how the application's remaining native menus reopen. |
| Custom header | Accept an application-provided SwiftUI header with a caller-specified height, kept outside the scrolling rows. A header-only popup is allowed. Search fields, loading indicators, empty-result wording, and result limits belong to the application. |
| Header input | The caller controls focus, menu-key routing, and whether wheel events go to the header. Focus detection covers `NSControl` and `NSTextView`; marked-text detection uses `NSTextView`. A general adapter for arbitrary custom responders or editors is outside the current scope. |
| Menu model | Use `AlternativeMenu` and `AlternativeMenuItem` values with actions, submenus, section headers, and separators. Actions are closures invoked after dismissal. Full `NSMenu`/`NSMenuItem` API compatibility, including target/action dispatch, delegates, check states, alternate items, and custom per-row views, is outside the current scope. |
| Enabled state | The application supplies `isEnabled` and replaces items through `updateItems`. There is no responder-chain validation or automatic enabling equivalent to `NSMenu.autoenablesItems`. Within this model, disabled-parent and empty-submenu interaction must still follow measured native behavior. |
| Typing selection | Exclude disabled items from title matching before choosing a candidate. For example, typing `a` with a disabled `A` and an enabled `Apple` selects `Apple`, whereas the measured `NSMenu` leaves the menu unselected. Preserve this intentional difference for exact, prefix, and nearest matches. |
| Item replacement and selection | `updateItems` replaces the root contents, closes children, and resets scrolling and selection rather than preserving item identity. The caller explicitly chooses `highlightsFirstItem` on presentation and updates; its default is `false`. |
| Confirmation timing | Animated action confirmation uses a fixed sequence of approximately 350 ms. Ignore additional menu input during confirmation instead of shortening the animation in response to further keys or clicks. Explicit dismissal or reopening still cancels the pending confirmation. |
| Title layout | Render plain and attributed titles with SwiftUI `Text`, using fixed leading alignment (left in LTR) and single-line tail truncation. Unlike `NSMenu`, do not honor the attributed title's paragraph alignment or line-break settings. Remove paragraph attributes from the shared display/measurement copy while preserving the caller's original attributed title in the model. |
| Mixed font sizes | Base row height on the largest font line height, while still accounting for title and image bounds. Accept the resulting height differences from `NSMenu` without introducing a separate sizing path for mixed font sizes. Single-font row heights remain subject to native parity. |
| Shortcut rendering | Render keyboard shortcuts as text in a single `NSTextField`. Accept small differences in the glyph shapes and positions of special keys and modifier symbols compared with `NSMenu`'s image rendering. Shared-column alignment and menu sizing remain subject to native parity. |
| Edge antialiasing | Accept minor differences in edge softness and pixel colors of symbols, bitmap images, and emoji that are noticeable mainly in close comparison or magnification. Layout size, placement, column alignment, and overall color remain subject to native parity. |
| Image visibility | Always display an image supplied by the caller, including on macOS 27 and later. Do not expose an image-visibility setting or adopt `NSMenuItem.preferredImageVisibility` and AppKit's automatic image-hiding policy. |
| Stable layout | The header initializer defaults to a caller-overridable `minimumWidth` of 240 pt; headerless menus have no such minimum. During one presentation, item updates may widen the popup but do not shrink it, subject to screen bounds. A nonzero `minimumSymbolColumnWidth` lets the caller reserve space across changing contents; zero retains content-based sizing. |
| Tooltip scheduling | Keep delay management in the library and delegate position, size, and drawing to AppKit. The initial delay uses `NSInitialToolTipDelay` when valid, otherwise 1 second; subsequent tooltips in the same hover sequence appear immediately. |

Native quirks that were deliberately retained, such as missing high-function-key
glyphs and long tooltips extending past a screen edge, are not deviations. Do
not add corrective behavior that would differ from the measured native result.

## Private AppKit APIs in Use

This inventory covers calls in the library, not APIs inspected only during
investigation. Keep each integration's purpose and unavailable-API behavior
explicit when changing it.

| API | Purpose | If unavailable |
| --- | --- | --- |
| `NSImage._isSymbolImage` (getter, read through KVC) | Identify SF Symbols without generating differently scaled images for every check. | Compare the sizes produced by small and large symbol configurations. |
| `NSVisualEffectView.Material(rawValue: 36)` | Use NSMenu's highlight material for active selections and unemphasized submenu ancestors, retaining the menu backdrop in both states. | Use `.selection` if construction fails. AppKit exposes no availability query for this undocumented material; reverify it on supported OS versions. |
| `NSKeyboardShortcut.shortcutWithKeyEquivalent:modifierMask:`; the returned object's `localizedModifierMaskDisplayName` and `localizedKeyEquivalentDisplayName` | Obtain AppKit's modifier symbols and readable, localized key names from the same prepared values used for native key matching, avoiding a separate translation table. | Omit the shortcut label; native key matching remains active. |
| `NSContextMenuItemView.menuItemCommandKeyFont` and `keyEquivalentStringForItem:withFontPtr:` | Obtain the actual drawing string and font for the same prepared `NSMenuItem` used for matching. Seed the in/out font argument with AppKit's command-key font so Backtab and function keys use native formatting. Directional arrow keys still use triangle characters in the custom view. | Omit the shortcut label; native key matching remains active. |
| `_NSMenuToolTipManager.sharedToolTipManager`, `_displayTemporaryToolTipForView:withString:`, and `orderOutToolTipForView:` | Let AppKit position, size, draw, and dismiss text tooltips using a row anchor, while retaining the library's delay management. | Omit the tooltip. |
| `NSWindow._hasActiveAppearanceIgnoringKeyFocus` (override) | Keep menu glass active without moving keyboard focus to submenus, matching `NSMenu`. | The override has no effect if AppKit stops consulting this selector; normal window appearance applies. |
| `NSGlassEffectView` KVC keys `_variant`, `_subvariant`, `_scrimState`, `_subduedState`, `_interactionState`, `_contentLensing`, `_adaptiveAppearance`, and `_useReducedShadowRadius` | On macOS 26+, configure glass to approximate the measured native menu backdrop and follow application appearance. Current values are `0`, `"menu"`, `0`, `0`, `0`, `0`, `1`, and `false`, respectively. | Check each setter independently and apply every supported override. Skip only unavailable settings, leaving their defaults intact; a missing setter must not prevent other settings from being applied. Earlier systems use public `NSVisualEffectView.Material.menu`. |

The following related dependencies are not private method calls:

- `NSInitialToolTipDelay` is an undocumented system defaults key read through public `UserDefaults`. Accept values from 1 through 10,000 milliseconds; use the default above otherwise.
- `NSMenu.accessibilityChildren()` is public, but using it to prepare localized/mirrored shortcut values before first display relies on an undocumented side effect. Read display data from the same prepared items used by `performKeyEquivalent(with:)`, keep their values fixed during the popup, and rebuild on reopening or item replacement. Reverify this preparation on supported OS versions; it is not a documented AppKit guarantee.
- `UCTypeSelectFindItem` is public, but its callback uses undocumented option bit `0x4`, as observed in `NSMenu`, to prefer the first prefix match in menu order while prioritizing exact matches. CoreServices owns the input buffer and locale-aware matching. The bit has no availability query or separate fallback; reverify its behavior on supported OS versions.

## Easily Missed Debugging Cases

Choose cases relevant to the change and combine them where appropriate. This
is a list of checks to perform, not a record of verified behavior.

| Area | Cases to exercise |
| --- | --- |
| IME and text input | Actually switch input sources while open; compose, select candidates, commit, and cancel. Check editing keys versus menu navigation. `setMarkedText` alone does not verify input-source switching. |
| Height and scrolling | Content that fits, barely overflows, or has about 500 items; large images or fonts in a few rows; opening near the top and bottom; with and without a header. Keep the header visible and both ends reachable. Combine wheel, momentum, edge scrolling, arrows, Page Up/Down, and Home/End. |
| Screens and placement | All screen edges, small displays, different scale factors, multiple displays with negative coordinates, RTL, and Dock/menu bar placement. |
| Row layout | Mixed symbols, bitmaps, and imageless rows; separators and section headers; long titles, mixed fonts, and shortcuts. Check drawing, measured size, and hit testing together. |
| Submenus | Multiple levels, oversized children, empty submenus, disabled parents, and entirely disabled children. Combine scrolling, diagonal pointer movement, hover, keyboard entry, and dismissal. |
| Updates and reopening | Counts changing through 0 → 1 → many → 0 while scrolled, selected, or showing a child. Reopen at another location and change enabled states. Check stale search results, selection, shortcuts, timers, and accessibility elements. |
| Selection and activation | Mouse, keyboard, title typing, duplicate shortcuts, modifiers, and unopened submenus. Include mouse-down opening, dragging, and releasing outside. Check action dispatch and dismissal together. |
| Keyboard layouts | Shortcut display and matching before the first key event, input-source changes while open, reopening, item updates, and RTL. Recheck behavior that depends on undocumented AppKit side effects on supported OS versions. |
| Hover and tooltips | A stationary pointer after keyboard navigation, scrolling beneath the pointer, parent/child transitions, initial and subsequent tooltips, and long text near screen edges. |
| Accessibility and appearance | VoiceOver reading and activation, duplicate or stale elements, light/dark mode, accent colors, and Reduce Transparency. |

500 items is Clipy's expected validation scale, not a library limit.

## Verification and Reporting

- Use isolated data for UI checks. Synthetic `NSEvent` objects can behave differently from real keystrokes. Unit tests alone do not establish parity; inspect the visible menus and exercise actual input paths.
- For performance comparisons, keep item counts, content, Debug/Release configuration, and measurement scope consistent. Separate data preparation from display and input latency.
- Report the macOS/SDK versions, input source, display setup, reproduction steps, and observed differences from `NSMenu`. Mark unavailable environments or blocked checks as unverified. Documentation-only changes do not require rerunning UI checks.
