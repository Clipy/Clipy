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
    static let sharedController = CPYPreferencesWindowController(windowNibName: NSNib.Name(rawValue: "CPYPreferencesWindowController"))
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
    fileprivate let viewController = [NSViewController(nibName: NSNib.Name(rawValue: "CPYGeneralPreferenceViewController"), bundle: nil),
                                  NSViewController(nibName: NSNib.Name(rawValue: "CPYMenuPreferenceViewController"), bundle: nil),
                                  CPYTypePreferenceViewController(nibName: NSNib.Name(rawValue: "CPYTypePreferenceViewController"), bundle: nil),
                                  CPYExcludeAppPreferenceViewController(nibName: NSNib.Name(rawValue: "CPYExcludeAppPreferenceViewController"), bundle: nil),
                                  CPYShortcutsPreferenceViewController(nibName: NSNib.Name(rawValue: "CPYShortcutsPreferenceViewController"), bundle: nil),
                                  CPYUpdatesPreferenceViewController(nibName: NSNib.Name(rawValue: "CPYUpdatesPreferenceViewController"), bundle: nil),
                                  CPYBetaPreferenceViewController(nibName: NSNib.Name(rawValue: "CPYBetaPreferenceViewController"), bundle: nil)]

    // MARK: - Window Life Cycle
    override func windowDidLoad() {
        super.windowDidLoad()
        self.window?.collectionBehavior = .canJoinAllSpaces
        self.window?.backgroundColor = NSColor(white: 0.99, alpha: 1)
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
    }

    override func showWindow(_ sender: Any?) {
        super.showWindow(sender)
        window?.makeKeyAndOrderFront(self)
    }
}

// MARK: - IBActions
extension CPYPreferencesWindowController {
    @IBAction private func toolBarItemTapped(_ sender: NSButton) {
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
fileprivate extension CPYPreferencesWindowController {
    private func resetImages() {
        generalImageView.image = NSImage(assetIdentifier: .generalOff)
        menuImageView.image = NSImage(assetIdentifier: .menuOff)
        typeImageView.image = NSImage(assetIdentifier: .typeOff)
        excludeImageView.image = NSImage(assetIdentifier: .excludedOff)
        shortcutsImageView.image = NSImage(assetIdentifier: .shortcutsOff)
        updatesImageView.image = NSImage(assetIdentifier: .updatesOff)
        betaImageView.image = NSImage(assetIdentifier: .betaOff)

        generalTextField.textColor = NSColor.tabTitleColor()
        menuTextField.textColor = NSColor.tabTitleColor()
        typeTextField.textColor = NSColor.tabTitleColor()
        excludeTextField.textColor = NSColor.tabTitleColor()
        shortcutsTextField.textColor = NSColor.tabTitleColor()
        updatesTextField.textColor = NSColor.tabTitleColor()
        betaTextField.textColor = NSColor.tabTitleColor()
    }

    func selectedTab(_ index: Int) {
        resetImages()

        switch index {
        case 0:
            generalImageView.image = NSImage(assetIdentifier: .generalOn)
            generalTextField.textColor = NSColor.clipyColor()
        case 1:
            menuImageView.image = NSImage(assetIdentifier: .menuOn)
            menuTextField.textColor = NSColor.clipyColor()
        case 2:
            typeImageView.image = NSImage(assetIdentifier: .typeOn)
            typeTextField.textColor = NSColor.clipyColor()
        case 3:
            excludeImageView.image = NSImage(assetIdentifier: .excludedOn)
            excludeTextField.textColor = NSColor.clipyColor()
        case 4:
            shortcutsImageView.image = NSImage(assetIdentifier: .shortcutsOn)
            shortcutsTextField.textColor = NSColor.clipyColor()
        case 5:
            updatesImageView.image = NSImage(assetIdentifier: .updatesOn)
            updatesTextField.textColor = NSColor.clipyColor()
        case 6:
            betaImageView.image = NSImage(assetIdentifier: .betaOn)
            betaTextField.textColor = NSColor.clipyColor()
        default: break
        }
    }

    fileprivate func switchView(_ index: Int) {
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
    }
}
