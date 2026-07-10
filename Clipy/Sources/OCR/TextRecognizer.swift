//
//  TextRecognizer.swift
//
//  Clipy
//  GitHub: https://github.com/clipy
//  HP: https://clipy-app.com
//
//  Created by Shunsuke Furubayashi on 2026/07/10.
//
//  Copyright © 2015-2026 Clipy Project.
//

import Dependencies
import Foundation
import Vision

protocol TextRecognizerProtocol {
    func recognizeTextIfNeeded(id: PasteboardHistory.ID)
}

final class TextRecognizer: TextRecognizerProtocol {
    @Dependency(\.pasteboardHistoryRepository)
    private var pasteboardHistoryRepository

    private let queue = DispatchQueue(label: "com.clipy-app.Clipy.TextRecognizer", qos: .utility)

    func recognizeTextIfNeeded(id: PasteboardHistory.ID) {
        queue.async { [weak self] in
            guard let self else { return }
            guard let history = self.pasteboardHistoryRepository.fetchHistory(id: id), history.ocrText == nil else { return }
            guard let imageData = self.pasteboardHistoryRepository.fetchContent(id: id)?.imageData else { return }
            let text = recognizedText(in: imageData) ?? ""
            pasteboardHistoryRepository.updateOCRText(id: id, ocrText: String(text.prefix(10000)))
        }
    }
}

private extension TextRecognizer {
    func recognizedText(in imageData: Data) -> String? {
        let request = VNRecognizeTextRequest()
        request.recognitionLevel = .accurate
        request.usesLanguageCorrection = true
        request.automaticallyDetectsLanguage = true
        let handler = VNImageRequestHandler(data: imageData)
        try? handler.perform([request])
        return request.results?
            .compactMap { $0.topCandidates(1).first?.string }
            .joined(separator: "\n")
    }
}

private enum TextRecognizerKey: DependencyKey {
    static let liveValue: any TextRecognizerProtocol = TextRecognizer()
}

extension DependencyValues {
    var textRecognizer: any TextRecognizerProtocol {
        get { self[TextRecognizerKey.self] }
        set { self[TextRecognizerKey.self] = newValue }
    }
}
