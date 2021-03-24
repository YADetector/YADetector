#
# Be sure to run `pod lib lint YADetector.podspec' to ensure this is a
# valid spec before submitting.
#
# Any lines starting with a # are optional, but their use is encouraged
# To learn more about a Podspec see https://guides.cocoapods.org/syntax/podspec.html
#

Pod::Spec.new do |s|
  s.name             = 'YADetector'
  s.version          = '0.1.0'
  s.summary          = 'Yet Another Detector'

# This description is used to generate tags and improve search results.
#   * Think: What does it do? Why did you write it? What is the focus?
#   * Try to keep it short, snappy and to the point.
#   * Write the description between the DESC delimiters below.
#   * Finally, don't worry about the indent, CocoaPods strips it!

  s.description      = <<-DESC
Yet Another Detector.
                       DESC

  s.homepage         = 'https://github.com/yadetector/YADetector'
  s.license          = { :type => 'MIT', :file => 'LICENSE' }
  s.author           = { 'yadetector' => 'yadetector@gmail.com' }
  s.source           = { :git => 'https://github.com/yadetector/YADetector.git', :tag => s.version.to_s }
  # s.social_media_url = 'https://twitter.com/<TWITTER_USERNAME>'

  s.ios.deployment_target = '9.0'

  s.source_files = 'YADetector/Classes/**/*'
  
  s.pod_target_xcconfig = {
    'OTHER_CFLAGS' => '-DWITH_YAD_TT',
  }
  
  s.library = 'c++'
  
  # s.resource_bundles = {
  #   'YADetector' => ['YADetector/Assets/*.png']
  # }

   s.public_header_files = 'YADetector/Classes/YADetector.h'
  # s.frameworks = 'UIKit', 'MapKit'
  # s.dependency 'AFNetworking', '~> 2.3'
end
