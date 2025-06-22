
#import "gltf_binding.h"
#import "gltf/gltf.hpp"

NS_ASSUME_NONNULL_BEGIN

using namespace systems::leal::gltf;


@interface gltf_binding()

@property std::shared_ptr<systems::leal::gltf::GLTF> native;

@end

@implementation gltf_binding

-(id) initWithString:(NSString *)data
{
    if (self=[super init]) {

        try {
            self.native = GLTF::loadGLTF(data.UTF8String);
            if (self.native == nullptr) {
                return nil;
            }
        } catch (std::exception &ex) {
            [NSException raise:@"Internal exception" format:@"%s", ex.what()];
            return nil;
        }
    }
    return self;
}

-(id) initWithNSData:(NSData *)data
{
    if (self=[super init]) {

        try {
            self.native = GLTF::loadGLB((uint8_t *)data.bytes, data.length);
            if (self.native == nullptr) {
                return nil;
            }
        } catch (std::exception &ex) {
            [NSException raise:@"Internal exception" format:@"%s", ex.what()];
            return nil;
        }
    }
    return self;

}

-(::RuntimeInfo *) getRuntimeInfo:(uint64_t)sceneIndex
{
    auto info = self.native->getRuntimeInfo(sceneIndex);
    if (info == nullptr) {
        return nil;
    }
    
    auto buffers = [NSMutableArray array];
    for (int a=0; a<info->buffers.size(); a++) {
        [buffers addObject:[NSNumber numberWithBool:info->buffers[a]]];
    }
    auto images = [NSMutableArray array];
    for (int a=0; a<info->images.size(); a++) {
        [images addObject:[NSNumber numberWithBool:info->images[a]]];
    }

    auto toReturn = [[::RuntimeInfo alloc] init:buffers :images];
    
    return toReturn;
}


-(void)dealloc
{
    NSLog(@"destructor asset pointer: %ld",self.native.use_count());
    self.native = nullptr;
}

-(NSString *) toString
{
    return [NSString stringWithCString:self.native->toString().c_str()
                              encoding:NSUTF8StringEncoding];
}

+(NSString *) getVersion
{
    return [NSString stringWithCString:systems::leal::gltf::getVersion().c_str() encoding:NSUTF8StringEncoding];
}

@end

NS_ASSUME_NONNULL_END
