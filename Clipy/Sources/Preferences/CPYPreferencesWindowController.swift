//
//  CPYPreferencesWindowController.swift
//
//  Clipy
//  GitHub: https://github.com/clipy
//  HP: https://clipy-app.com
//
//  Created by Econa77 on 2016/02/25.
//
//  Copyright © 2015-2018 Clipy Project.
//

import Cocoa

final class CPYPreferencesWindowController: NSWindowController {

    // MARK: - Properties
    static let sharedController = CPYPreferencesWindowController(windowNibName: "CPYPreferencesWindowController")
    private var selectedTabIndex: Int = 0
    @IBOutlet private weak var toolBar: NSView!
    // ImageViews
    @IBOutlet private weak var generalImageView: NSImageView!
    @IBOutlet private weak var menuImageView: NSImageView!
    @IBOutlet private weak var typeImageView: NSImageView!
    @IBOutlet private weak var excludeImageView: NSImageView!
    @IBOutlet private weak var shortcutsImageView: NSImageView!
    @IBOutlet private weak var updatesImageView: NSImageView!
    @IBOutlet private weak var betaImageView: NSImageView!
    // Labels
    @IBOutlet private weak var generalTextField: NSTextField!
    @IBOutlet private weak var menuTextField: NSTextField!
    @IBOutlet private weak var typeTextField: NSTextField!
    @IBOutlet private weak var excludeTextField: NSTextField!
    @IBOutlet private weak var shortcutsTextField: NSTextField!
    @IBOutlet private weak var updatesTextField: NSTextField!
    @IBOutlet private weak var betaTextField: NSTextField!
    // Buttons
    @IBOutlet private weak var generalButton: NSButton!
    @IBOutlet private weak var menuButton: NSButton!
    @IBOutlet private weak var typeButton: NSButton!
    @IBOutlet private weak var excludeButton: NSButton!
    @IBOutlet private weak var shortcutsButton: NSButton!
    @IBOutlet private weak var updatesButton: NSButton!
    @IBOutlet private weak var betaButton: NSButton!
    // ViewController
    private let viewController = [NSViewController(nibName: "CPYGeneralPreferenceViewController", bundle: nil),
                                  NSViewController(nibName: "CPYMenuPreferenceViewController", bundle: nil),
                                  CPYTypePreferenceViewController(nibName: "CPYTypePreferenceViewController", bundle: nil),
                                  CPYExcludeAppPreferenceViewController(nibName: "CPYExcludeAppPreferenceViewController", bundle: nil),
                                  CPYShortcutsPreferenceViewController(nibName: "CPYShortcutsPreferenceViewController", bundle: nil),
                                  CPYUpdatesPreferenceViewController(nibName: "CPYUpdatesPreferenceViewController", bundle: nil),
                                  CPYBetaPreferenceViewController(nibName: "CPYBetaPreferenceViewController", bundle: nil)]

    // MARK: - Window Life Cycle
    override func windowDidLoad() {
        super.windowDidLoad()
        self.window?.collectionBehavior = .canJoinAllSpaces
        self.window?.backgroundColor = .windowBackgroundColor
        if #available(OSX 10.10, *) {
            self.window?.titlebarAppearsTransparent = true
        }
        toolBarItemTapped(generalButton)
        generalButton.sendAction(on: .leftMouseDown)
        menuButton.sendAction(on: .leftMouseDown)
        typeButton.sendAction(on: .leftMouseDown)
        excludeButton.sendAction(on: .leftMouseDown)
        shortcutsButton.sendAction(on: .leftMouseDown)
        updatesButton.sendAction(on: .leftMouseDown)
        betaButton.sendAction(on: .leftMouseDown)
        applyAppearance()
    }

    override func showWindow(_ sender: Any?) {
        super.showWindow(sender)
        window?.makeKeyAndOrderFront(self)
        applyAppearance()
    }
}

// MARK: - IBActions
extension CPYPreferencesWindowController {
    @IBAction private func toolBarItemTapped(_ sender: NSButton) {
        selectedTabIndex = sender.tag
        selectedTab(sender.tag)
        switchView(sender.tag)
    }
}

// MARK: - NSWindow Delegate
extension CPYPreferencesWindowController: NSWindowDelegate {
    func windowWillClose(_ notification: Notification) {
        if let viewController = viewController[2] as? CPYTypePreferenceViewController {
            AppEnvironment.current.defaults.set(viewController.storeTypes, forKey: Constants.UserDefaults.storeTypes)
            AppEnvironment.current.defaults.synchronize()
        }
        if let window = window, !window.makeFirstResponder(window) {
            window.endEditing(for: nil)
        }
        NSApp.deactivate()
    }
}

// MARK: - Layout
private extension CPYPreferencesWindowController {
    var selectedLabelColor: NSColor {
        if #available(macOS 10.14, *) {
            return .controlAccentColor
        }
        return ColorName.clipy.color
    }

    var borderSeparatorColor: NSColor {
        if #available(macOS 10.14, *) {
            return .separatorColor
        }
        return .gridColor
    }

    var unselectedLabelColor: NSColor {
        if #available(macOS 10.14, *) {
            return .secondaryLabelColor
        }
        return ColorName.tabTitle.color
    }

    func configureToolbarIcon(_ imageView: NSImageView, image: NSImage) {
        let renderedImage = (image.copy() as? NSImage) ?? image
        renderedImage.isTemplate = false
        imageView.image = renderedImage
        if #available(macOS 10.14, *) {
            imageView.contentTintColor = nil
        }
    }

    func resetImages() {
        configureToolbarIcon(generalImageView, image: Asset.prefGeneral.image)
        configureToolbarIcon(menuImageView, image: Asset.prefMenu.image)
        configureToolbarIcon(typeImageView, image: Asset.prefType.image)
        configureToolbarIcon(excludeImageView, image: Asset.prefExcluded.image)
        configureToolbarIcon(shortcutsImageView, image: Asset.prefShortcut.image)
        configureToolbarIcon(updatesImageView, image: Asset.prefUpdate.image)
        configureToolbarIcon(betaImageView, image: Asset.prefBeta.image)

        generalTextField.textColor = unselectedLabelColor
        menuTextField.textColor = unselectedLabelColor
        typeTextField.textColor = unselectedLabelColor
        excludeTextField.textColor = unselectedLabelColor
        shortcutsTextField.textColor = unselectedLabelColor
        updatesTextField.textColor = unselectedLabelColor
        betaTextField.textColor = unselectedLabelColor
    }

    func selectedTab(_ index: Int) {
        resetImages()

        switch index {
        case 0:
            configureToolbarIcon(generalImageView, image: Asset.prefGeneralOn.image)
            generalTextField.textColor = selectedLabelColor
        case 1:
            configureToolbarIcon(menuImageView, image: Asset.prefMenuOn.image)
            menuTextField.textColor = selectedLabelColor
        case 2:
            configureToolbarIcon(typeImageView, image: Asset.prefTypeOn.image)
            typeTextField.textColor = selectedLabelColor
        case 3:
            configureToolbarIcon(excludeImageView, image: Asset.prefExcludedOn.image)
            excludeTextField.textColor = selectedLabelColor
        case 4:
            configureToolbarIcon(shortcutsImageView, image: Asset.prefShortcutOn.image)
            shortcutsTextField.textColor = selectedLabelColor
        case 5:
            configureToolbarIcon(updatesImageView, image: Asset.prefUpdateOn.image)
            updatesTextField.textColor = selectedLabelColor
        case 6:
            configureToolbarIcon(betaImageView, image: Asset.prefBetaOn.image)
            betaTextField.textColor = selectedLabelColor
        default: break
        }
    }

    func switchView(_ index: Int) {
        let newView = viewController[index].view
        // Remove current views without toolbar
        window?.contentView?.subviews.forEach { view in
            if view != toolBar {
                view.removeFromSuperview()
            }
        }
        // Resize view
        let frame = window!.frame
        var newFrame = window!.frameRect(forContentRect: newView.frame)
        newFrame.origin = frame.origin
        newFrame.origin.y += frame.height - newFrame.height - toolBar.frame.height
        newFrame.size.height += toolBar.frame.height
        window?.setFrame(newFrame, display: true)
        window?.contentView?.addSubview(newView)
        applyAppearance()
    }

    func applyAppearance() {
        window?.backgroundColor = .windowBackgroundColor
        applyAppearance(to: toolBar)
        if let contentView = window?.contentView {
            for view in contentView.subviews where view != toolBar {
                applyAppearance(to: view)
            }
        }
        selectedTab(selectedTabIndex)
    }

    func applyAppearance(to view: NSView) {
        if let designableView = view as? CPYDesignableView {
            designableView.backgroundColor = .controlBackgroundColor
            if designableView.borderWidth > 0 {
                designableView.borderColor = borderSeparatorColor
            }
        }
        if let textField = view as? NSTextField {
            if textField.isEditable {
                textField.textColor = .textColor
            } else {
                textField.textColor = .labelColor
            }
        }
        view.subviews.forEach { applyAppearance(to: $0) }
    }
}
