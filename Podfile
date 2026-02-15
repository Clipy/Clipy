require 'shellwords'

platform :osx, '10.13'
use_frameworks!

inhibit_all_warnings!

target 'Clipy' do

  # Application
  pod 'PINCache'
  pod 'Sauce'
  pod 'Sparkle'
  pod 'RealmSwift', '~> 10.0'
  pod 'RxCocoa'
  pod 'RxSwift'
  pod 'LoginServiceKit', :git => 'https://github.com/Clipy/LoginServiceKit.git'
  pod 'KeyHolder'
  pod 'Magnet'
  pod 'RxScreeen'
  pod 'AEXML'
  pod 'LetsMove'
  pod 'SwiftHEXColors'
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

post_install do |installer|
  installer.pods_project.targets.each do |target|
    target.build_configurations.each do |config|
      config.build_settings['MACOSX_DEPLOYMENT_TARGET'] = '10.13'

      ldflags = config.build_settings['OTHER_LDFLAGS']
      next if ldflags.nil?

      values = ldflags.is_a?(Array) ? ldflags : Shellwords.split(ldflags.to_s)
      config.build_settings['OTHER_LDFLAGS'] = values.uniq
    end

    target.shell_script_build_phases.each do |phase|
      next unless phase.name == 'Create Symlinks to Header Folders'

      # Keep this script explicitly out-of-date to avoid Xcode warning noise
      # about missing output files.
      phase.always_out_of_date = '1'
    end
  end
end
