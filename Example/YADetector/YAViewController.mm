//
//  YAViewController.m
//  YADetector
//
//  Created by yadetector on 03/05/2021.
//  Copyright (c) 2021 yadetector. All rights reserved.
//

#import "YAViewController.h"
#import <YADetector/YADetector.h>

@interface YAViewController ()

@end

@implementation YAViewController

- (void)viewDidLoad
{
    [super viewDidLoad];
	// Do any additional setup after loading the view, typically from a nib.
    
    YADConfig config;
    config[kYADMaxFaceCount] = std::to_string(3);
    config[kYADPixFormat] = std::to_string(YAD_PIX_FMT_BGRA8888);
    config[kYADDataType] = std::to_string(YAD_DATA_TYPE_IOS_PIXEL_BUFFER);
    YAD::Detector *detector = YAD::Detector::Create(config);
    NSLog(@"detector: %p", detector);
    if (detector) {
        NSLog(@"detector initCheck: %d", detector->initCheck());
        delete detector;
    }
}

- (void)didReceiveMemoryWarning
{
    [super didReceiveMemoryWarning];
    // Dispose of any resources that can be recreated.
}

@end
