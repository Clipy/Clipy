import KeyHolder
import Magnet
import SwiftUI

struct ShortcutRecorder: NSViewRepresentable {
    let keyCombo: KeyCombo?
    let didChange: (KeyCombo?) -> Void

    func makeCoordinator() -> Coordinator {
        Coordinator(didChange: didChange)
    }

    func makeNSView(context: Context) -> RecordView {
        let view = RecordView()
        view.delegate = context.coordinator
        view.keyCombo = keyCombo
        view.tintColor = .controlAccentColor
        view.borderColor = .separatorColor
        view.borderWidth = 1
        view.cornerRadius = 8
        view.clearButtonMode = .whenRecorded
        return view
    }

    func updateNSView(_ nsView: RecordView, context: Context) {
        context.coordinator.didChange = didChange
        if !nsView.isRecording, nsView.keyCombo != keyCombo {
            nsView.keyCombo = keyCombo
        }
    }

    static func dismantleNSView(_ nsView: RecordView, coordinator: Coordinator) {
        nsView.endRecording()
        nsView.delegate = nil
    }

    final class Coordinator: NSObject, RecordViewDelegate {
        var didChange: (KeyCombo?) -> Void

        init(didChange: @escaping (KeyCombo?) -> Void) {
            self.didChange = didChange
        }

        func recordViewShouldBeginRecording(_ recordView: RecordView) -> Bool { true }

        func recordView(_ recordView: RecordView, canRecordKeyCombo keyCombo: KeyCombo) -> Bool { true }

        func recordView(_ recordView: RecordView, didChangeKeyCombo keyCombo: KeyCombo?) {
            didChange(keyCombo)
        }

        func recordViewDidEndRecording(_ recordView: RecordView) {}
    }
}
