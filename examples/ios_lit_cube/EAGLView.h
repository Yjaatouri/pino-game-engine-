#if defined(__OBJC__)
#import <UIKit/UIKit.h>
#import <OpenGLES/EAGL.h>
#import <OpenGLES/ES3/gl.h>
#import <OpenGLES/ES3/glext.h>

@interface EAGLView : UIView

@property (nonatomic, readonly) EAGLContext* context;
@property (nonatomic, readonly) GLuint framebuffer;
@property (nonatomic, readonly) GLuint colorRenderbuffer;

- (BOOL)createFramebuffer;
- (void)deleteFramebuffer;

@end
#endif
