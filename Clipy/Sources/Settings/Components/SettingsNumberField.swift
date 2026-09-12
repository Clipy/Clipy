import SwiftUI

struct SettingsNumberField: View {
    @Binding
    private var value: Int
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
                        value: $value,
                        formatter: formatter,
                        label: EmptyView.init
                    )
                    .lineLimit(1)
                    .multilineTextAlignment(layoutDirection == .rightToLeft ? .leading : .trailing)
                    .monospacedDigit()
                    .frame(width: 64)

                    Stepper(
                        value: $value,
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
    }
}
