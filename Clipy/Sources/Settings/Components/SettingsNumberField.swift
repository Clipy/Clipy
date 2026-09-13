import SwiftUI

struct SettingsNumberField: View {
    @Binding
    private var value: Int
    @State
    private var draftValue: Int
    @State
    private var hasChanges = false
    @FocusState
    private var isFocused: Bool
    private let bounds: ClosedRange<Int>
    private let step: Int.Stride
    private let unit: LocalizedStringResource?
    private let caption: LocalizedStringResource?

    @Environment(\.layoutDirection)
    private var layoutDirection

    private var formatter: NumberFormatter {
        let formatter = NumberFormatter()
        formatter.numberStyle = .decimal
        formatter.allowsFloats = false
        formatter.usesGroupingSeparator = false
        formatter.minimum = NSNumber(value: bounds.lowerBound)
        formatter.maximum = NSNumber(value: bounds.upperBound)
        return formatter
    }

    init(
        value: Binding<Int>,
        in bounds: ClosedRange<Int>,
        step: Int.Stride = 1,
        unit: LocalizedStringResource? = .Settings.items,
        caption: LocalizedStringResource? = nil
    ) {
        self._value = value
        self._draftValue = State(initialValue: value.wrappedValue)
        self.bounds = bounds
        self.step = step
        self.unit = unit
        self.caption = caption
    }

    var body: some View {
        HStack(alignment: .firstTextBaseline) {
            VStack(alignment: .leading, spacing: 2) {
                HStack(spacing: 4) {
                    TextField(
                        value: Binding(
                            get: { draftValue },
                            set: {
                                guard draftValue != $0 else { return }
                                draftValue = $0
                                hasChanges = true
                            }
                        ),
                        formatter: formatter,
                        label: EmptyView.init
                    )
                    .focused($isFocused)
                    .onSubmit(commit)
                    .lineLimit(1)
                    .multilineTextAlignment(layoutDirection == .rightToLeft ? .leading : .trailing)
                    .monospacedDigit()
                    .frame(width: 64)

                    Stepper(
                        value: Binding(
                            get: { hasChanges ? draftValue : value },
                            set: {
                                value = $0
                                resetDraft()
                            }
                        ),
                        in: bounds,
                        step: step,
                        label: EmptyView.init
                    )
                }
                if let caption {
                    Text(caption)
                        .frame(width: 64)
                        .font(.caption)
                }
            }
            if let unit {
                Text(unit)
            }
        }
        .labelsHidden()
        .onAppear(perform: resetDraft)
        .onDisappear(perform: commit)
        .onChange(of: isFocused) { isFocused in
            if !isFocused {
                commit()
            }
        }
        .onChange(of: value) { _ in
            if !hasChanges {
                resetDraft()
            }
        }
    }

    private func commit() {
        guard hasChanges else { return }
        if bounds.contains(draftValue), draftValue != value {
            value = draftValue
        }
        resetDraft()
    }

    private func resetDraft() {
        draftValue = value
        hasChanges = false
    }
}
