platform :osx, '10.10'
use_frameworks!
inhibit_all_warnings!

source 'https://github.com/CocoaPods/Specs.git'

target 'Clipy' do

  # Application
  pod 'PINCache'
  pod 'Sauce'
  pod 'Sparkle'
  pod 'RealmSwift'
  pod 'Fabric'
  pod 'Crashlytics'
  pod 'RxCocoa'
  pod 'RxSwift'
  pod 'RxOptional'
  pod 'LoginServiceKit', :git => 'https://github.com/Clipy/LoginServiceKit.git'
  pod 'KeyHolder'
  pod 'RxScreeen'
  pod 'AEXML'
  pod 'LetsMove'
  pod 'SwiftHEXColors'
  pod 'CryptoSwift'
  # Utility
  pod 'BartyCrouch'
  pod 'SwiftLint'
  pod 'SwiftGen'

  target 'ClipyTests' do
    inherit! :search_paths

    pod 'Quick'
    pod 'Nimble'

  end

end


# Workaround for signing
# https://github.com/CocoaPods/CocoaPods/pull/6964#issuecomment-327851704
post_install do |installer|
  installer.pods_project.build_configurations.each do |config|
    config.build_settings[‘PROVISIONING_PROFILE_SPECIFIER’] = ''
  end
end
