//
//  CPYShortcutsPreferenceViewController.swift
//  Clipy
//
//  Created by 古林俊佑 on 2016/02/26.
//  Copyright © 2016年 Shunsuke Furubayashi. All rights reserved.
//

import Cocoa

class CPYShortcutsPreferenceViewController: NSViewController {

    // MARK: - Properties
    @IBOutlet weak var mainShortcutRecorder: SRRecorderControl!
    @IBOutlet weak var historyShortcutRecorder: SRRecorderControl!
    @IBOutlet weak var snippetsShortcutRecorder: SRRecorderControl!
    private var shortcutRecorders = [SRRecorderControl]()
    private let defaults = NSUserDefaults.standardUserDefaults()

    // MARK: - Initialize
    override func loadView() {
        super.loadView()
        prepareHotKeys()
    }

}

// MARK: - Shortcut
// swiftlint:disable force_cast
private extension CPYShortcutsPreferenceViewController {
    private func prepareHotKeys() {
        shortcutRecorders = [mainShortcutRecorder, historyShortcutRecorder, snippetsShortcutRecorder]

        let hotKeyMap = CPYHotKeyManager.sharedManager.hotkeyMap
        let hotKeyCombos = defaults.objectForKey(Constants.UserDefaults.hotKeys) as! [String: AnyObject]
        for identifier in hotKeyCombos.keys {

            let keyComboPlist = hotKeyCombos[identifier] as! [String: AnyObject]
            let keyCode = Int(keyComboPlist["keyCode"]! as! NSNumber)
            let modifiers = UInt(keyComboPlist["modifiers"]! as! NSNumber)

            if let keys = hotKeyMap[identifier] as? [String: AnyObject] {
                let index = keys[Constants.Common.index] as! Int
                let recorder = shortcutRecorders[index]
                let keyCombo = KeyCombo(flags: recorder.carbonToCocoaFlags(modifiers), code: keyCode)
                recorder.keyCombo = keyCombo
                recorder.animates = true
            }
        }
    }

    private func changeHotKeyByShortcutRecorder(aRecorder: SRRecorderControl!, keyCombo: KeyCombo) {
        let newKeyCombo = PTKeyCombo(keyCode: keyCombo.code, modifiers: aRecorder.cocoaToCarbonFlags(keyCombo.flags))

        var identifier = ""
        if aRecorder == mainShortcutRecorder {
            identifier = Constants.Menu.clip
        } else if aRecorder == historyShortcutRecorder {
            identifier = Constants.Menu.history
        } else if aRecorder == snippetsShortcutRecorder {
            identifier = Constants.Menu.snippet
        }

        let hotKeyCenter = PTHotKeyCenter.sharedCenter()
        let oldHotKey = hotKeyCenter.hotKeyWithIdentifier(identifier)
        hotKeyCenter.unregisterHotKey(oldHotKey)

        var hotKeyPrefs = defaults.objectForKey(Constants.UserDefaults.hotKeys) as! [String: AnyObject]
        hotKeyPrefs.updateValue(newKeyCombo.plistRepresentation(), forKey: identifier)
        defaults.setObject(hotKeyPrefs, forKey: Constants.UserDefaults.hotKeys)
        defaults.synchronize()
    }
}
// swiftlint:enable force_cast

// MARK: - SRRecoederControl Delegate
extension CPYShortcutsPreferenceViewController {
    func shortcutRecorder(aRecorder: SRRecorderControl!, keyComboDidChange newKeyCombo: KeyCombo) {
        if shortcutRecorders.contains(aRecorder) {
            changeHotKeyByShortcutRecorder(aRecorder, keyCombo: newKeyCombo)
        }
    }
}
