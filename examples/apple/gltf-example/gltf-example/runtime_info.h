#import <Foundation/Foundation.h>

@interface RuntimeInfo : NSObject

@property NSArray<NSNumber *> *buffers;
@property NSArray<NSNumber *> *images;

-(id) init:(NSArray<NSNumber *> *)buffers :(NSArray<NSNumber *> *)images;

@end

