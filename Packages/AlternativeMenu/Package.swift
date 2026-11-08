// swift-tools-version: 6.2

import PackageDescription

let package = Package(
    name: "AlternativeMenu",
    platforms: [.macOS(.v14)],
    products: [
        .library(name: "AlternativeMenu", targets: ["AlternativeMenu"])
    ],
    targets: [
        .target(name: "AlternativeMenu"),
        .testTarget(
            name: "AlternativeMenuTests",
            dependencies: ["AlternativeMenu"]
        )
    ]
)
