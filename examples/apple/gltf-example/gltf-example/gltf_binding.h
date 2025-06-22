#import <Foundation/Foundation.h>
#import "runtime_info.h"

@interface gltf_binding : NSObject

-(id) initWithString:(NSString *)data;

-(id) initWithNSData:(NSData *)data;

-(RuntimeInfo *) getRuntimeInfo:(uint64_t)sceneIndex;

-(NSString *) toString;

+(NSString *) getVersion;


@end

