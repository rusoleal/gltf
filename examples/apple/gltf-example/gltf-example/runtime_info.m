
#import "runtime_info.h"

@implementation RuntimeInfo

-(id) init:(NSArray<NSNumber *> *)buffers :(NSArray<NSNumber *> *)images
{
    if (self=[super init]) {
        self.buffers = buffers;
        self.images = images;
    }
    return self;
}

@end
