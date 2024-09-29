// MyObject.mm
#include "MetalInterface.h"
#include <QuartzCore/CAMetalDisplayLink.h>

// Define an Objective-C class
@interface MyObject : NSObject

- (int)performActionWithInt:(int)i;
@end

@implementation MyObject
- (int)performActionWithInt:(int)i {
    // Example implementation: doubling the input value
    return i + 5;
}
@end

// Implement the C function declared in the header
int pObjectC(int i) {
    // Create an instance of MyObject
    MyObject *obj = [[MyObject alloc] init];
    // Call the Objective-C method and return its result
    return [obj performActionWithInt:i];
}

void RegisterVerticalBlankingCallback(void (*callback)()) {

}

}
