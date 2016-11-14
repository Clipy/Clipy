# AEXML
**Simple and lightweight XML parser written in Swift**

[![Language Swift 3.0](https://img.shields.io/badge/Language-Swift%203.0-orange.svg?style=flat)](https://swift.org)
[![Platforms iOS | watchOS | tvOS | OSX](https://img.shields.io/badge/Platforms-iOS%20%7C%20watchOS%20%7C%20tvOS%20%7C%20OS%20X-lightgray.svg?style=flat)](http://www.apple.com)
[![License MIT](https://img.shields.io/badge/License-MIT-lightgrey.svg?style=flat)](https://github.com/tadija/AEXML/blob/master/LICENSE)

[![CocoaPods Version](https://img.shields.io/cocoapods/v/AEXML.svg?style=flat)](https://cocoapods.org/pods/AEXML)
[![Carthage compatible](https://img.shields.io/badge/Carthage-compatible-brightgreen.svg?style=flat)](https://github.com/Carthage/Carthage)
[![Swift Package Manager compatible](https://img.shields.io/badge/Swift%20Package%20Manager-compatible-brightgreen.svg)](https://github.com/apple/swift-package-manager)

> This is not a robust full featured XML parser, but rather simple,  
lightweight and easy to use utility for casual XML handling.

**AEXML** is a [minion](http://tadija.net/public/minion.png) which consists of these classes:  

Class | Description
------------ | -------------
`AEXMLElement` | Base class
`AEXMLDocument` | Inherited from `AEXMLElement` with a few addons
`AEXMLParser` | Simple (private) wrapper around `Foundation.XMLParser`

## Index
- [Features](#features)
- [Example](#example)
    - [Read XML](#read-xml)
    - [Write XML](#write-xml)
- [Requirements](#requirements)
- [Installation](#installation)
- [License](#license)

## Features
- **Read XML** data
- **Write XML** string
- Covered with [unit tests](https://github.com/tadija/AEXML/blob/master/Tests/AEXMLTests.swift)
- Covered with [docs](http://cocoadocs.org/docsets/AEXML)

## Example

### Read XML
Let's say this is some XML string you picked up somewhere and made a variable `data: Data` from that.

```xml
<?xml version="1.0" encoding="utf-8"?>
<animals>
    <cats>
        <cat breed="Siberian" color="lightgray">Tinna</cat>
        <cat breed="Domestic" color="darkgray">Rose</cat>
        <cat breed="Domestic" color="yellow">Caesar</cat>
        <cat></cat>
    </cats>
    <dogs>
        <dog breed="Bull Terrier" color="white">Villy</dog>
        <dog breed="Bull Terrier" color="white">Spot</dog>
        <dog breed="Golden Retriever" color="yellow">Betty</dog>
        <dog breed="Miniature Schnauzer" color="black">Kika</dog>
    </dogs>
</animals>
```

This is how you can use AEXML for working with this data:  
(for even more examples, look at the unit tests code included in project)

```swift
guard let
    let xmlPath = Bundle.main.path(forResource: "example", ofType: "xml"),
    let data = try? Data(contentsOf: URL(fileURLWithPath: xmlPath))
else { return }

do {
    let xmlDoc = try AEXMLDocument(xml: data, options: options)
        
    // prints the same XML structure as original
    print(xmlDoc.xml)
    
    // prints cats, dogs
    for child in xmlDoc.root.children {
        print(child.name)
    }
    
    // prints Optional("Tinna") (first element)
    print(xmlDoc.root["cats"]["cat"].value)
    
    // prints Tinna (first element)
    print(xmlDoc.root["cats"]["cat"].string)
    
    // prints Optional("Kika") (last element)
    print(xmlDoc.root["dogs"]["dog"].last?.value)
    
    // prints Betty (3rd element)
    print(xmlDoc.root["dogs"].children[2].string)
    
    // prints Tinna, Rose, Caesar
    if let cats = xmlDoc.root["cats"]["cat"].all {
        for cat in cats {
            if let name = cat.value {
                print(name)
            }
        }
    }
    
    // prints Villy, Spot
    for dog in xmlDoc.root["dogs"]["dog"].all! {
        if let color = dog.attributes["color"] {
            if color == "white" {
                print(dog.string)
            }
        }
    }
    
    // prints Tinna
    if let cats = xmlDoc.root["cats"]["cat"].all(withValue: "Tinna") {
        for cat in cats {
            print(cat.string)
        }
    }
    
    // prints Caesar
    if let cats = xmlDoc.root["cats"]["cat"].all(withAttributes: ["breed" : "Domestic", "color" : "yellow"]) {
        for cat in cats {
            print(cat.string)
        }
    }
    
    // prints 4
    print(xmlDoc.root["cats"]["cat"].count)
    
    // prints Siberian
    print(xmlDoc.root["cats"]["cat"].attributes["breed"]!)
    
    // prints <cat breed="Siberian" color="lightgray">Tinna</cat>
    print(xmlDoc.root["cats"]["cat"].xmlCompact)
    
    // prints Optional(AEXML.AEXMLError.elementNotFound)
    print(xmlDoc["NotExistingElement"].error)
}
catch {
    print("\(error)")
}
```

### Write XML
Let's say this is some SOAP XML request you need to generate.  
Well, you could just build ordinary string for that?

```xml
<?xml version="1.0" encoding="utf-8"?>
<soap:Envelope xmlns:xsd="http://www.w3.org/2001/XMLSchema" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance">
  <soap:Header>
    <m:Trans xmlns:m="http://www.w3schools.com/transaction/" soap:mustUnderstand="1">234</m:Trans>
  </soap:Header>
  <soap:Body>
    <m:GetStockPrice>
      <m:StockName>AAPL</m:StockName>
    </m:GetStockPrice>
  </soap:Body>
</soap:Envelope>
```

Yes, but, you can also do it in a more structured and elegant way with AEXML:

```swift
// create XML Document
let soapRequest = AEXMLDocument()
let attributes = ["xmlns:xsi" : "http://www.w3.org/2001/XMLSchema-instance", "xmlns:xsd" : "http://www.w3.org/2001/XMLSchema"]
let envelope = soapRequest.addChild(name: "soap:Envelope", attributes: attributes)
let header = envelope.addChild(name: "soap:Header")
let body = envelope.addChild(name: "soap:Body")
header.addChild(name: "m:Trans", value: "234", attributes: ["xmlns:m" : "http://www.w3schools.com/transaction/", "soap:mustUnderstand" : "1"])
let getStockPrice = body.addChild(name: "m:GetStockPrice")
getStockPrice.addChild(name: "m:StockName", value: "AAPL")

// prints the same XML structure as original
print(soapRequest.xml)
```

## Requirements
- Xcode 8.0+
- AEXML doesn't require any additional libraries for it to work.

## Installation

- [Swift Package Manager](https://swift.org/package-manager/):

	```
	.Package(url: "https://github.com/tadija/AEXML.git", majorVersion: 4)
	```

- [Carthage](https://github.com/Carthage/Carthage):

	```ogdl
	github "tadija/AEXML"
	```

- [CocoaPods](http://cocoapods.org/):

	```ruby
	pod 'AEXML'
	```

## License
AEXML is released under the MIT license. See [LICENSE](LICENSE) for details.
