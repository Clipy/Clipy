# Clipy Project Context

## Project Overview
Clipy is a clipboard extension app for macOS. It supports tracking clipboard history, organizing snippets, and pasting them via a menu interface.

**Key Technologies:**
-   **Platform:** macOS 10.10+ (Target 10.15 in CI)
-   **Language:** Swift 5.3+
-   **Core Frameworks:** Cocoa (AppKit)
-   **Data Persistence:** Realm (RealmSwift)
-   **Reactive Programming:** RxSwift, RxCocoa
-   **Utilities:** Sparkle (Updates), Magnet (HotKeys), KeyHolder, LoginServiceKit
-   **Dependency Management:** CocoaPods (iOS/macOS libs), Bundler (Ruby tools)

## Architecture & Design
The application follows a structure that separates concerns into Models, Views, Controllers (WindowControllers), and a heavy use of **Services** and **Managers** accessed via a global `AppEnvironment`.

### Key Components
-   **AppEnvironment (`Clipy/Sources/Environments/`):** Acts as a dependency container, providing access to singleton services like `ClipService`, `PasteService`, `HotKeyService`, etc.
-   **Services (`Clipy/Sources/Services/`):** Handle business logic.
    -   `ClipService`: Monitors and manages clipboard data.
    -   `PasteService`: Handles the actual pasting logic.
    -   `HotKeyService`: Manages global keyboard shortcuts.
    -   `ExcludeAppService`: Manages apps where Clipy should be disabled.
-   **Managers (`Clipy/Sources/Managers/`):** specialized controllers, e.g., `MenuManager` handles the status bar menu and popups.
-   **Models (`Clipy/Sources/Models/`):** Realm objects (`CPYClip`, `CPYSnippet`, `CPYFolder`).
-   **UI:** Uses XIBs and Storyboards (though XIBs seem more prevalent for specific windows like `CPYPreferencesWindowController`).

### Data Flow
1.  `AppDelegate` initializes `AppEnvironment` and starts services.
2.  `ClipService` monitors the system pasteboard.
3.  New clips are saved to Realm.
4.  `MenuManager` updates the NSMenu based on Realm data.
5.  User interaction (Menu selection/HotKey) triggers `PasteService`.

## Building and Running

### Prerequisites
-   Xcode (12.2+ recommended per CI/Docs)
-   Ruby (2.6+) & Bundler

### Setup
Initialize dependencies:
```bash
bundle install --path=vendor/bundle
bundle exec pod install
```

### Build
Open `Clipy.xcworkspace` in Xcode and build the `Clipy` scheme.

### Testing
Run unit tests using Fastlane:
```bash
bundle exec fastlane test
```
Or use `Cmd+U` in Xcode.

### Linting
The project uses **SwiftLint**.
```bash
bundle exec fastlane lint # If a lint lane exists, otherwise check .swiftlint.yml
```
(Note: `danger-swiftlint` is in Gemfile, suggesting linting happens during CI).

### Code Generation
**SwiftGen** is used for strongly typed resources (Images, Colors, Strings).
-   Config: `swiftgen.yml`
-   Generated files are in `Clipy/Generated/`.
-   Run `Pods/SwiftGen/bin/swiftgen` (or via build phase) to update.

## Directory Structure
-   `Clipy/`: Main application source.
    -   `Sources/`: Swift source files.
    -   `Resources/`: Assets, Localization files (.lproj), Plists.
    -   `Generated/`: SwiftGen outputs.
-   `ClipyTests/`: Unit tests.
-   `fastlane/`: Fastlane configuration for CI/CD.
-   `Podfile`: CocoaPods dependencies.
-   `Gemfile`: Ruby dependencies.

## Development Conventions
-   **Localization:** Uses `BartyCrouch` and standard `.strings` files.
-   **Reactive:** Heavy use of `DisposeBag` and `RxSwift` in ViewControllers and Services.
-   **Formatting:** Strict adherence to SwiftLint rules.
-   **Git:** CI runs on push/PR. Use feature branches.
