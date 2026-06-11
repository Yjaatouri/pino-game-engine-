#import "ViewController.h"
#import "EAGLView.h"

#include "engine/engine.h"
#include "engine/renderer/gl_es3.h"
#include "engine/renderer/shader.h"
#include "engine/renderer/mesh.h"
#include "engine/renderer/texture.h"
#include "engine/renderer/camera.h"
#include "engine/renderer/light.h"
#include "engine/assets/asset_manager.h"

#include <glm/gtc/matrix_transform.hpp>

// ─── LitCubeGame: C++ game class ─────────────────────────────────────────────
class LitCubeGame : public pino::IGame {
public:
    pino::Engine*        engine  = nullptr;
    pino::AssetManager*  assets  = nullptr;
    pino::Shader*        shader  = nullptr;
    pino::Mesh*          cube    = nullptr;
    pino::Texture*       texture = nullptr;
    pino::Camera         camera;
    pino::Material       material;
    float                angle = 0;

    bool init() override {
        auto& fs = engine->filesystem();

        shader = assets->load_shader("shaders/lit.vert", "shaders/lit.frag");
        if (!shader) return false;

        cube = assets->load_mesh("models/cube.obj");
        if (!cube) return false;

        texture = assets->load_texture("textures/checker.ppm");
        if (!texture) return false;

        camera.perspective(pino::radians(60.0f), engine->window().aspect(), 0.1f, 100.0f);
        camera.look_at({3, 2, 4}, {0, 0, 0}, {0, 1, 0});

        material.ambient   = {0.3f, 0.3f, 0.3f};
        material.diffuse   = {0.8f, 0.8f, 0.8f};
        material.specular  = {1.0f, 1.0f, 1.0f};
        material.shininess = 32.0f;

        glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
        glEnable(GL_DEPTH_TEST);
        return true;
    }

    void update(pino::f32 dt) override {
        angle += dt * 0.5f;
    }

    void render(pino::f32) override {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        shader->bind();
        shader->set_mat4("u_view_proj", camera.view_proj());

        auto model = glm::rotate(glm::mat4(1.0f), angle, glm::vec3(0, 1, 0));
        shader->set_mat4("u_model", model);
        shader->set_mat3("u_normal_matrix", glm::mat3(glm::transpose(glm::inverse(model))));
        shader->set_vec3("u_camera_pos", camera.position());

        pino::upload_ambient(*shader, {1, 1, 1}, 0.3f);
        pino::upload_directional(*shader, {0, -1, -1}, {0.8f, 0.8f, 1.0f});
        shader->set_int("u_num_point_lights", 0);

        pino::upload_material(*shader, material);

        texture->bind(0);
        shader->set_int("u_diffuse_tex", 0);
        shader->set_int("u_has_diffuse_tex", 1);

        cube->draw();
    }

    void shutdown() override {}

    void on_context_lost() override {
        PINO_INFO("Context lost — invalidating GPU resources");
        assets->invalidate_all();
        shader  = nullptr;
        cube    = nullptr;
        texture = nullptr;
    }

    void on_context_restored() override {
        PINO_INFO("Context restored — re-uploading GPU resources");
        shader  = assets->load_shader("shaders/lit.vert", "shaders/lit.frag");
        cube    = assets->load_mesh("models/cube.obj");
        texture = assets->load_texture("textures/checker.ppm");

        camera.perspective(pino::radians(60.0f), engine->window().aspect(), 0.1f, 100.0f);

        glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
        glEnable(GL_DEPTH_TEST);
    }
};

// ─── ViewController ──────────────────────────────────────────────────────────
@interface ViewController () {
    EAGLView* _glView;
    CADisplayLink* _displayLink;
    pino::Engine _engine;
    LitCubeGame _game;
    BOOL _initialized;
    BOOL _app_active;
}
@end

@implementation ViewController

- (void)viewDidLoad {
    [super viewDidLoad];

    _app_active = YES;

    _glView = [[EAGLView alloc] initWithFrame:self.view.bounds];
    _glView.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    [self.view addSubview:_glView];

    [self setupLifecycleNotifications];
    [self setupTouchNotifications];

    _displayLink = [CADisplayLink displayLinkWithTarget:self selector:@selector(renderFrame)];
    _displayLink.preferredFramesPerSecond = 60;
    [_displayLink addToRunLoop:[NSRunLoop mainRunLoop] forMode:NSRunLoopCommonModes];
}

- (void)viewDidLayoutSubviews {
    [super viewDidLayoutSubviews];
    // EAGLView recreates its framebuffer in layoutSubviews automatically.
    // Notify the window so it can check for context validity.
    if (_initialized) {
        _engine.window().on_framebuffer_recreated();
    }
}

// ─── Lifecycle notifications ─────────────────────────────────────────────────

- (void)setupLifecycleNotifications {
    NSNotificationCenter* nc = [NSNotificationCenter defaultCenter];

    // App will resign active (e.g. phone call, app switcher)
    [nc addObserverForName:UIApplicationWillResignActiveNotification
                    object:nil queue:[NSOperationQueue mainQueue]
                usingBlock:^(NSNotification*) {
        [self appWillResignActive];
    }];

    // App became active (after foreground or initial launch)
    [nc addObserverForName:UIApplicationDidBecomeActiveNotification
                    object:nil queue:[NSOperationQueue mainQueue]
                usingBlock:^(NSNotification*) {
        [self appDidBecomeActive];
    }];

    // App entered background (about to be suspended)
    [nc addObserverForName:UIApplicationDidEnterBackgroundNotification
                    object:nil queue:[NSOperationQueue mainQueue]
                usingBlock:^(NSNotification*) {
        [self appDidEnterBackground];
    }];

    // App will enter foreground (about to be displayed)
    [nc addObserverForName:UIApplicationWillEnterForegroundNotification
                    object:nil queue:[NSOperationQueue mainQueue]
                usingBlock:^(NSNotification*) {
        [self appWillEnterForeground];
    }];

    // Memory warning
    [nc addObserverForName:UIApplicationDidReceiveMemoryWarningNotification
                    object:nil queue:[NSOperationQueue mainQueue]
                usingBlock:^(NSNotification*) {
        [self didReceiveMemoryWarning];
    }];
}

- (void)appWillResignActive {
    _app_active = NO;
    if (_initialized) {
        // Pause update loop — render stops naturally from CADisplayLink pause
        _engine.pause();

        // Ensure all GL commands complete before snapshot / suspension
        glFinish();
    }
}

- (void)appDidBecomeActive {
    _app_active = YES;
    if (_initialized) {
        // Re-create framebuffer (may have been invalidated during background)
        [EAGLContext setCurrentContext:_glView.context];
        [_glView deleteFramebuffer];
        [_glView createFramebuffer];

        // Notify the window that framebuffer was recreated
        _engine.window().on_framebuffer_recreated();

        // Resume update loop
        _engine.resume();
    }
}

- (void)appDidEnterBackground {
    if (_initialized) {
        glFinish();
    }
}

- (void)appWillEnterForeground {
    // Framebuffer recreation happens in appDidBecomeActive
}

- (void)didReceiveMemoryWarning {
    PINO_WARN("iOS memory warning — unloading non-critical assets");
    if (_initialized && _game.assets) {
        // Unload cached assets not actively referenced by game code
        _game.assets->unload_unused();
    }
}

// ─── Touch notifications (from EAGLView) ─────────────────────────────────────

- (void)setupTouchNotifications {
    NSNotificationCenter* nc = [NSNotificationCenter defaultCenter];

    [nc addObserverForName:@"PinoTouchBegan" object:nil queue:nil
                usingBlock:^(NSNotification* note) {
        NSDictionary* u = note.userInfo;
        _game.engine->input().touch_began(
            [u[@"x"] floatValue], [u[@"y"] floatValue],
            static_cast<u64>([u[@"ptr"] unsignedLongLongValue])
        );
    }];

    [nc addObserverForName:@"PinoTouchMoved" object:nil queue:nil
                usingBlock:^(NSNotification* note) {
        NSDictionary* u = note.userInfo;
        _game.engine->input().touch_moved(
            [u[@"x"] floatValue], [u[@"y"] floatValue],
            static_cast<u64>([u[@"ptr"] unsignedLongLongValue])
        );
    }];

    [nc addObserverForName:@"PinoTouchEnded" object:nil queue:nil
                usingBlock:^(NSNotification* note) {
        NSDictionary* u = note.userInfo;
        _game.engine->input().touch_ended(
            [u[@"x"] floatValue], [u[@"y"] floatValue],
            static_cast<u64>([u[@"ptr"] unsignedLongLongValue])
        );
    }];

    [nc addObserverForName:@"PinoTouchCancelled" object:nil queue:nil
                usingBlock:^(NSNotification*) {
        _game.engine->input().touch_cancelled();
    }];
}

// ─── Engine init ─────────────────────────────────────────────────────────────

- (void)initEngine {
    pino::EngineConfig cfg;
    cfg.app_title       = "Pino Lit Cube";
    cfg.window_width    = static_cast<u32>(_glView.bounds.size.width * _glView.contentScaleFactor);
    cfg.window_height   = static_cast<u32>(_glView.bounds.size.height * _glView.contentScaleFactor);
    cfg.native_window   = (__bridge void*)_glView;
    cfg.fixed_update_rate = 60;

    if (!_engine.init(cfg)) {
        NSLog(@"Engine init failed");
        return;
    }

    _game.engine = &_engine;
    _game.assets = new pino::AssetManager(_engine.filesystem());

    if (!_game.init()) {
        NSLog(@"Game init failed");
        return;
    }

    _initialized = YES;
    [EAGLContext setCurrentContext:_glView.context];
    glBindFramebuffer(GL_FRAMEBUFFER, _glView.framebuffer);
}

// ─── Render loop (CADisplayLink callback) ────────────────────────────────────

- (void)renderFrame {
    if (!_initialized) {
        [self initEngine];
        return;
    }

    if (!_engine.is_running()) return;

    // Check for context restore (needed after framebuffer recreation)
    if (_engine.window().needs_context_restore()) {
        _game.on_context_lost();
        // Re-bind the freshly created framebuffer
        [EAGLContext setCurrentContext:_glView.context];
        glBindFramebuffer(GL_FRAMEBUFFER, _glView.framebuffer);
        _game.on_context_restored();
    }

    // Set up the OpenGL ES framebuffer for this frame
    [EAGLContext setCurrentContext:_glView.context];
    glBindFramebuffer(GL_FRAMEBUFFER, _glView.framebuffer);
    GLint w = 0, h = 0;
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, &w);
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_HEIGHT, &h);
    glViewport(0, 0, w, h);

    // Run one game loop iteration
    _engine.step_game(_game);
    // step_game calls end_frame → IOSWindow::swap_buffers → presentRenderbuffer
}

- (void)dealloc {
    [_displayLink invalidate];
    if (_initialized) {
        _game.shutdown();
        delete _game.assets;
        _engine.shutdown();
    }
}

@end
