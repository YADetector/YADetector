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
    
    YAD::Detector *detector = YAD::Detector::Create(3, YAD_PIX_FMT_BGRA8888, YAD_DATA_TYPE_IOS_PIXEL_BUFFER);
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
