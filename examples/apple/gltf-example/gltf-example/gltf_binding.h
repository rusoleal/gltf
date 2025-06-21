//
//  gltf_binding.h
//  gltf-example
//
//  Created by Ruben Leal on 24/5/25.
//

#import <Foundation/Foundation.h>

//NS_ASSUME_NONNULL_BEGIN

@interface gltf_binding : NSObject

-(id) initWithString:(NSString *)data;
-(id) initWithNSData:(NSData *)data;

-(NSString *) toString;

+(NSString *) getVersion;

@end

//NS_ASSUME_NONNULL_END
