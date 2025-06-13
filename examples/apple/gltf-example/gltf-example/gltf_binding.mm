//
//  gltf_binding.m
//  gltf-example
//
//  Created by Ruben Leal on 24/5/25.
//

#import <future>
#import "gltf_binding.h"
#import "gltf/gltf.hpp"

NS_ASSUME_NONNULL_BEGIN

using namespace systems::leal::gltf;

@interface gltf_binding()

@property std::shared_ptr<systems::leal::gltf::GLTF> native;

@end

@implementation gltf_binding

-(id) init:(NSString *)data
{
    if (self=[super init]) {
        
        self.native = GLTF::loadGLTF(data.UTF8String, [](const std::string &uri) {
            std::future<std::vector<uint8_t>> toReturn = std::async(std::launch::async, []() {
                return std::vector<uint8_t>();
            });
            return toReturn;
        } );
    }
    return self;
}

@end

NS_ASSUME_NONNULL_END
