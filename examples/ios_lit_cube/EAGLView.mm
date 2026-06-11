#import "EAGLView.h"
#import <QuartzCore/CAEAGLLayer.h>

@implementation EAGLView {
    GLuint _depthRenderbuffer;
}

+ (Class)layerClass {
    return [CAEAGLLayer class];
}

- (instancetype)initWithFrame:(CGRect)frame {
    self = [super initWithFrame:frame];
    if (self) {
        CAEAGLLayer* layer = (CAEAGLLayer*)self.layer;
        layer.opaque = YES;
        layer.drawableProperties = @{
            kEAGLDrawablePropertyColorFormat: kEAGLColorFormatRGBA8,
            kEAGLDrawablePropertyRetinaBacking: @(YES),
        };

        _context = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES3];
        if (!_context) {
            NSLog(@"Failed to create ES3 context");
            return nil;
        }
        [EAGLContext setCurrentContext:_context];
        [self createFramebuffer];
    }
    return self;
}

- (void)layoutSubviews {
    [EAGLContext setCurrentContext:_context];
    [self deleteFramebuffer];
    [self createFramebuffer];
}

- (BOOL)createFramebuffer {
    glGenFramebuffers(1, &_framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, _framebuffer);

    glGenRenderbuffers(1, &_colorRenderbuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, _colorRenderbuffer);
    [_context renderbufferStorage:GL_RENDERBUFFER fromDrawable:(CAEAGLLayer*)self.layer];
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, _colorRenderbuffer);

    GLint width = 0, height = 0;
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, &width);
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_HEIGHT, &height);

    glGenRenderbuffers(1, &_depthRenderbuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, _depthRenderbuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, _depthRenderbuffer);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        NSLog(@"Framebuffer not complete: 0x%x", status);
        return NO;
    }

    glBindRenderbuffer(GL_RENDERBUFFER, _colorRenderbuffer);
    return YES;
}

- (void)deleteFramebuffer {
    if (_framebuffer) {
        glDeleteFramebuffers(1, &_framebuffer);
        _framebuffer = 0;
    }
    if (_colorRenderbuffer) {
        glDeleteRenderbuffers(1, &_colorRenderbuffer);
        _colorRenderbuffer = 0;
    }
    if (_depthRenderbuffer) {
        glDeleteRenderbuffers(1, &_depthRenderbuffer);
        _depthRenderbuffer = 0;
    }
}

// ─── Touch handling with safe-area-aware coordinates ─────────────────────────

- (CGPoint)convertTouchToPixel:(UITouch*)touch {
    CGPoint pt = [touch locationInView:self];
    // UIKit points → pixels
    CGFloat scale = self.contentScaleFactor;
    // Account for safe area offset in landscape
    if (@available(iOS 11.0, *)) {
        UIEdgeInsets insets = self.safeAreaInsets;
        pt.x -= insets.left;
    }
    return CGPointMake(pt.x * scale, pt.y * scale);
}

- (void)touchesBegan:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
    for (UITouch* t in touches) {
        CGPoint pt = [self convertTouchToPixel:t];
        [[NSNotificationCenter defaultCenter]
            postNotificationName:@"PinoTouchBegan"
            object:nil
            userInfo:@{
                @"x": @(pt.x),
                @"y": @(pt.y),
                @"ptr": @((uintptr_t)t),
            }];
    }
}

- (void)touchesMoved:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
    for (UITouch* t in touches) {
        CGPoint pt = [self convertTouchToPixel:t];
        [[NSNotificationCenter defaultCenter]
            postNotificationName:@"PinoTouchMoved"
            object:nil
            userInfo:@{
                @"x": @(pt.x),
                @"y": @(pt.y),
                @"ptr": @((uintptr_t)t),
            }];
    }
}

- (void)touchesEnded:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
    for (UITouch* t in touches) {
        CGPoint pt = [self convertTouchToPixel:t];
        [[NSNotificationCenter defaultCenter]
            postNotificationName:@"PinoTouchEnded"
            object:nil
            userInfo:@{
                @"x": @(pt.x),
                @"y": @(pt.y),
                @"ptr": @((uintptr_t)t),
            }];
    }
}

- (void)touchesCancelled:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
    [[NSNotificationCenter defaultCenter]
        postNotificationName:@"PinoTouchCancelled"
        object:nil];
}

@end
