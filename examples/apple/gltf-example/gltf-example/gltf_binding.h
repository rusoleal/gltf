#import <Foundation/Foundation.h>
#import "runtime_info.h"

/**
 * Callback block for loading external glTF resources.
 * @param uri The URI of the resource to load
 * @return The loaded resource data, or nil if loading failed
 */
typedef NSData * _Nullable (^GLTFResourceLoader)(NSString *uri);

@interface gltf_binding : NSObject

/**
 * Initialize with glTF JSON string.
 * External resources are NOT loaded. Use initWithString:resourceLoader: instead.
 */
-(id) initWithString:(NSString *)data;

/**
 * Initialize with glTF JSON string and resource loader.
 * The resource loader block is called for each external URI.
 */
-(id) initWithString:(NSString *)data resourceLoader:(GLTFResourceLoader)loader;

-(id) initWithNSData:(NSData *)data;

-(RuntimeInfo *) getRuntimeInfo:(uint64_t)sceneIndex;

-(NSString *) toString;

+(NSString *) getVersion;


@end

