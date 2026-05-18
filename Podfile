platform :macos, '13.0'
use_frameworks!
inhibit_all_warnings!

target 'Clipy' do

  # Application
  pod 'Sparkle'
  pod 'RealmSwift'
  pod 'LetsMove'
  # Utility
  pod 'BartyCrouch'
  pod 'SwiftGen'

  target 'ClipyTests' do
    inherit! :search_paths
  end

end

post_install do |installer|
  installer.pods_project.targets.each do |target|
    target.build_configurations.each do |config|
      config.build_settings['MACOSX_DEPLOYMENT_TARGET'] = '13.0'
    end
  end
end
