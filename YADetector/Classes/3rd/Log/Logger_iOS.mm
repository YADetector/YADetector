#ifdef __APPLE__

//
//  Logger_iOS.mm
//  YAD
//

#include "Logger_iOS.h"
#import <Foundation/Foundation.h>

namespace YAD {

void logImpl(const char *message)
{
    NSLog(@"%s", message);
}

}; // namespace YAD

#endif
