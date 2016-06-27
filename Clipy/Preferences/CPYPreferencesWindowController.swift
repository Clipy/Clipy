//
//  CPYPreferencesWindowController.swift
//  Clipy
//
//  Created by 古林俊佑 on 2016/02/25.
//  Copyright © 2016年 Shunsuke Furubayashi. All rights reserved.
//

import Cocoa

final class CPYPreferencesWindowController: NSWindowController {

    // MARK: - Properties
    static let sharedController = CPYPreferencesWindowController(windowNibName: "CPYPreferencesWindowController")
    @IBOutlet weak var toolBar: NSView!
    // ImageViews
    @IBOutlet weak var generalImageView: NSImageView!
    @IBOutlet weak var menuImageView: NSImageView!
    @IBOutlet weak var typeImageView: NSImageView!
    @IBOutlet weak var shortcutsImageView: NSImageView!
    @IBOutlet weak var updatesImageView: NSImageView!
    @IBOutlet weak var betaImageView: NSImageView!
    // Labels
    @IBOutlet weak var generalTextField: NSTextField!
    @IBOutlet weak var menuTextField: NSTextField!
    @IBOutlet weak var typeTextField: NSTextField!
    @IBOutlet weak var shortcutsTextField: NSTextField!
    @IBOutlet weak var updatesTextField: NSTextField!
    @IBOutlet weak var betaTextField: NSTextField!
    // Buttons
    @IBOutlet weak var generalButton: NSButton!
    @IBOutlet weak var menuButton: NSButton!
    @IBOutlet weak var typeButton: NSButton!
    @IBOutlet weak var shortcutsButton: NSButton!
    @IBOutlet weak var updatesButton: NSButton!
    @IBOutlet weak var betaButton: NSButton!
    // ViewController
    private let defaults = NSUserDefaults.standardUserDefaults()
    private let viewController = [NSViewController(nibName: "CPYGeneralPreferenceViewController", bundle: nil)!,
                                  NSViewController(nibName: "CPYMenuPreferenceViewController", bundle: nil)!,
                                  CPYTypePreferenceViewController(nibName: "CPYTypePreferenceViewController", bundle: nil)!,
                                  CPYShortcutsPreferenceViewController(nibName: "CPYShortcutsPreferenceViewController", bundle: nil)!,
                                  CPYUpdatesPreferenceViewController(nibName: "CPYUpdatesPreferenceViewController", bundle: nil)!,
                                  CPYBetaPreferenceViewController(nibName: "CPYBetaPreferenceViewController", bundle: nil)!]

    // MARK: - Window Life Cycle
    override func windowDidLoad() {
        super.windowDidLoad()
        self.window?.collectionBehavior = NSWindowCollectionBehavior.CanJoinAllSpaces
        self.window?.backgroundColor = NSColor(white: 0.99, alpha: 1)
        if #available(OSX 10.10, *) {
            self.window?.titlebarAppearsTransparent = true
        }
        toolBarItemTapped(generalButton)
        generalButton.sendActionOn(Int(NSEventMask.LeftMouseDownMask.rawValue))
        menuButton.sendActionOn(Int(NSEventMask.LeftMouseDownMask.rawValue))
        typeButton.sendActionOn(Int(NSEventMask.LeftMouseDownMask.rawValue))
        shortcutsButton.sendActionOn(Int(NSEventMask.LeftMouseDownMask.rawValue))
        updatesButton.sendActionOn(Int(NSEventMask.LeftMouseDownMask.rawValue))
        betaButton.sendActionOn(Int(NSEventMask.LeftMouseDownMask.rawValue))
    }

    override func showWindow(sender: AnyObject?) {
        super.showWindow(sender)
        window?.makeKeyAndOrderFront(self)
    }
}

// MARK: - IBActions
extension CPYPreferencesWindowController {
    @IBAction func toolBarItemTapped(sender: NSButton) {
        selectedTab(sender.tag)
        switchView(sender.tag)
    }
}

// MARK: - NSWindow Delegate
extension CPYPreferencesWindowController: NSWindowDelegate {
    func windowWillClose(notification: NSNotification) {
        if let viewController = viewController[2] as? CPYTypePreferenceViewController {
           defaults.setObject(viewController.storeTypes, forKey: Constants.UserDefaults.storeTypes)
            defaults.synchronize()
        }
        if let window = window where !window.makeFirstResponder(window) {
            window.endEditingFor(nil)
        }
        NSApp.deactivate()
    }
}

// MARK: - Layout
private extension CPYPreferencesWindowController {
    private func resetImages() {
        generalImageView.image      = NSImage(assetIdentifier: .GeneralOff)
        menuImageView.image         = NSImage(assetIdentifier: .MenuOff)
        typeImageView.image         = NSImage(assetIdentifier: .TypeOff)
        shortcutsImageView.image    = NSImage(assetIdentifier: .ShortcutsOff)
        updatesImageView.image      = NSImage(assetIdentifier: .UpdatesOff)
        betaImageView.image         = NSImage(assetIdentifier: .BetaOff)

        generalTextField.textColor      = NSColor.tabTitleColor()
        menuTextField.textColor         = NSColor.tabTitleColor()
        typeTextField.textColor         = NSColor.tabTitleColor()
        shortcutsTextField.textColor    = NSColor.tabTitleColor()
        updatesTextField.textColor      = NSColor.tabTitleColor()
        betaTextField.textColor         = NSColor.tabTitleColor()
    }

    private func selectedTab(index: Int) {
        resetImages()

        switch index {
        case 0:
            generalImageView.image = NSImage(assetIdentifier: .GeneralOn)
            generalTextField.textColor = NSColor.clipyColor()
        case 1:
            menuImageView.image = NSImage(assetIdentifier: .MenuOn)
            menuTextField.textColor = NSColor.clipyColor()
        case 2:
            typeImageView.image = NSImage(assetIdentifier: .TypeOn)
            typeTextField.textColor = NSColor.clipyColor()
        case 3:
            shortcutsImageView.image = NSImage(assetIdentifier: .ShortcutsOn)
            shortcutsTextField.textColor = NSColor.clipyColor()
        case 4:
            updatesImageView.image = NSImage(assetIdentifier: .UpdatesOn)
            updatesTextField.textColor = NSColor.clipyColor()
        case 5:
            betaImageView.image = NSImage(assetIdentifier: .BetaOn)
            betaTextField.textColor = NSColor.clipyColor()
        default: break
        }
    }

    private func switchView(index: Int) {
        let newView = viewController[index].view
        // Remove current views without toolbar
        window?.contentView?.subviews.forEach { view in
            if view != toolBar {
                view.removeFromSuperview()
            }
        }
        // Resize view
        let frame = window!.frame
        var newFrame = window!.frameRectForContentRect(newView.frame)
        newFrame.origin = frame.origin
        newFrame.origin.y +=  frame.height - newFrame.height - toolBar.frame.height
        newFrame.size.height += toolBar.frame.height
        window?.setFrame(newFrame, display: true)
        window?.contentView?.addSubview(newView)
    }
}
