
#import "gltf_binding.h"
#import "gltf/gltf.hpp"
#include <future>
#include <vector>

NS_ASSUME_NONNULL_BEGIN

using namespace systems::leal::gltf;


@interface gltf_binding()

@property std::shared_ptr<systems::leal::gltf::GLTF> native;

@end

@implementation gltf_binding

-(id) initWithString:(NSString *)data
{
    // Use no-op resource loader for self-contained glTF
    return [self initWithString:data resourceLoader:nil];
}

-(id) initWithString:(NSString *)data resourceLoader:(GLTFResourceLoader _Nullable)loader
{
    if (self=[super init]) {

        try {
            if (loader) {
                // Use callback-based loader
                GLTFResourceLoader loaderCopy = [loader copy];
                self.native = GLTF::loadGLTF(data.UTF8String, [loaderCopy](const std::string &uri) {
                    return std::async(std::launch::deferred, [loaderCopy, uri]() {
                        NSString *nsUri = [NSString stringWithUTF8String:uri.c_str()];
                        NSData *loaded = loaderCopy(nsUri);
                        std::vector<uint8_t> result;
                        if (loaded) {
                            const uint8_t *bytes = (const uint8_t *)loaded.bytes;
                            result.assign(bytes, bytes + loaded.length);
                        }
                        return result;
                    });
                });
            } else {
                // No resource loader - use no-op callback for inline/embedded glTF
                self.native = GLTF::loadGLTF(data.UTF8String, [](const std::string &) {
                    return std::async(std::launch::deferred, []() {
                        return std::vector<uint8_t>{};
                    });
                });
            }
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
