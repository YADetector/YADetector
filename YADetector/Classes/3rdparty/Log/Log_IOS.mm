#ifdef __APPLE__

//
//  Log_IOS.mm
//  YAD
//

#include "Log_IOS.h"
#import <Foundation/Foundation.h>

namespace YAD {

void logImpl(const char *message)
{
    NSLog(@"%s", message);
}

}  // namespace YAD

#endif
