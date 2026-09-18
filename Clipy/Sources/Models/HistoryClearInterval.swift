import Foundation

enum HistoryClearInterval: Int, CaseIterable {
    case fiveMinutes = 300
    case thirtyMinutes = 1800
    case oneHour = 3600
    case fourHours = 14400
    case eightHours = 28800
    case oneDay = 86400
    case threeDays = 259200
    case sevenDays = 604800

    var title: LocalizedStringResource {
        switch self {
        case .fiveMinutes: .Settings.every5Minutes
        case .thirtyMinutes: .Settings.every30Minutes
        case .oneHour: .Settings.everyHour
        case .fourHours: .Settings.every4Hours
        case .eightHours: .Settings.every8Hours
        case .oneDay: .Settings.everyDay
        case .threeDays: .Settings.every3Days
        case .sevenDays: .Settings.every7Days
        }
    }
}
