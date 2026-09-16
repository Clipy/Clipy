//
//  CPYShortcutsPreferenceViewController.swift
//
//  Clipy
//  GitHub: https://github.com/clipy
//  HP: https://clipy-app.com
//
//  Created by Econa77 on 2016/02/26.
//
//  Copyright © 2015-2018 Clipy Project.
//

import Cocoa
import Dependencies
import KeyHolder
import Magnet
import Sharing

class CPYShortcutsPreferenceViewController: NSViewController {

    // MARK: - Properties
    @IBOutlet private weak var mainShortcutRecordView: RecordView!
    @IBOutlet private weak var historyShortcutRecordView: RecordView!
    @IBOutlet private weak var snippetShortcutRecordView: RecordView!
    @IBOutlet private weak var editSnippetsShortcutRecordView: RecordView!
    @IBOutlet private weak var clearHistoryShortcutRecordView: RecordView!

    @Dependency(\.hotKeyService)
    private var hotKeyService
    @Shared(.mainKeyCombo)
    private var mainKeyCombo
    @Shared(.historyKeyCombo)
    private var historyKeyCombo
    @Shared(.snippetKeyCombo)
    private var snippetKeyCombo
    @Shared(.editSnippetsKeyCombo)
    private var editSnippetsKeyCombo
    @Shared(.clearHistoryKeyCombo)
    private var clearHistoryKeyCombo

    // MARK: - Initialize
    override func loadView() {
        super.loadView()
        mainShortcutRecordView.delegate = self
        historyShortcutRecordView.delegate = self
        snippetShortcutRecordView.delegate = self
        editSnippetsShortcutRecordView.delegate = self
        clearHistoryShortcutRecordView.delegate = self
        prepareHotKeys()
    }

}

// MARK: - Shortcut
private extension CPYShortcutsPreferenceViewController {
    func prepareHotKeys() {
        mainShortcutRecordView.keyCombo = mainKeyCombo
        historyShortcutRecordView.keyCombo = historyKeyCombo
        snippetShortcutRecordView.keyCombo = snippetKeyCombo
        editSnippetsShortcutRecordView.keyCombo = editSnippetsKeyCombo
        clearHistoryShortcutRecordView.keyCombo = clearHistoryKeyCombo
    }
}

// MARK: - RecordView Delegate
extension CPYShortcutsPreferenceViewController: RecordViewDelegate {
    func recordViewShouldBeginRecording(_ recordView: RecordView) -> Bool {
        return true
    }

    func recordView(_ recordView: RecordView, canRecordKeyCombo keyCombo: KeyCombo) -> Bool {
        return true
    }

    func recordView(_ recordView: RecordView, didChangeKeyCombo keyCombo: KeyCombo?) {
        switch recordView {
        case mainShortcutRecordView:
            hotKeyService.change(with: .main, keyCombo: keyCombo)
        case historyShortcutRecordView:
            hotKeyService.change(with: .history, keyCombo: keyCombo)
        case snippetShortcutRecordView:
            hotKeyService.change(with: .snippet, keyCombo: keyCombo)
        case editSnippetsShortcutRecordView:
            hotKeyService.changeEditSnippetsKeyCombo(keyCombo)
        case clearHistoryShortcutRecordView:
            hotKeyService.changeClearHistoryKeyCombo(keyCombo)
        default: break
        }
    }

    func recordViewDidEndRecording(_ recordView: RecordView) {}
}
