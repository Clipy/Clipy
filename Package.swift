// swift-tools-version:5.3
import PackageDescription

let package = Package(
    name: "Clipy",
    dependencies: [
        .package(url: "https://github.com/realm/realm-swift.git", .upToNextMajor(from: "20.0.4")),
        .package(url: "https://github.com/ReactiveX/RxSwift.git", .upToNextMajor(from: "6.0.0")),
        .package(url: "https://github.com/sparkle-project/Sparkle.git", .upToNextMajor(from: "2.0.0")),
        .package(url: "https://github.com/pinterest/PINCache.git", .upToNextMajor(from: "3.0.0")),
        .package(url: "https://github.com/thii/SwiftHEXColors.git", .upToNextMajor(from: "1.3.1")),
        .package(url: "https://github.com/tadija/AEXML.git", .upToNextMajor(from: "4.6.0")),
        .package(url: "https://github.com/Clipy/Magnet.git", .upToNextMajor(from: "3.0.0")),
        .package(url: "https://github.com/Clipy/Sauce.git", .upToNextMajor(from: "2.0.0")),
        .package(url: "https://github.com/Clipy/KeyHolder.git", .upToNextMajor(from: "4.0.0")),
        // .package(url: "https://github.com/Clipy/RxScreeen.git", .upToNextMajor(from: "1.0.0")),
        // .package(url: "https://github.com/Clipy/Screeen.git", .upToNextMajor(from: "1.0.0")),
        .package(url: "https://github.com/Clipy/LoginServiceKit.git", .upToNextMajor(from: "2.0.0")),
    ],
    targets: [
        .target(name: "Dummy", dependencies: [
            .product(name: "RealmSwift", package: "realm-swift"),
            .product(name: "RxSwift", package: "RxSwift"),
            .product(name: "RxCocoa", package: "RxSwift"),
            .product(name: "Sparkle", package: "Sparkle"),
            .product(name: "PINCache", package: "PINCache"),
            .product(name: "SwiftHEXColors", package: "SwiftHEXColors"),
            .product(name: "AEXML", package: "AEXML"),
            .product(name: "Magnet", package: "Magnet"),
            .product(name: "Sauce", package: "Sauce"),
            .product(name: "KeyHolder", package: "KeyHolder"),
            // .product(name: "RxScreeen", package: "RxScreeen"),
            // .product(name: "Screeen", package: "Screeen"),
            .product(name: "LoginServiceKit", package: "LoginServiceKit"),
        ])
    ]
)
