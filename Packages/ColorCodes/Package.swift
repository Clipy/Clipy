// swift-tools-version: 6.0

import PackageDescription

let package = Package(
    name: "ColorCodes",
    platforms: [.macOS(.v13)],
    products: [
        .library(name: "ColorCodes", targets: ["ColorCodes"])
    ],
    targets: [
        .target(name: "ColorCodes"),
        .testTarget(
            name: "ColorCodesTests",
            dependencies: ["ColorCodes"]
        )
    ]
)
