#pragma once

// =============================================================================
// tcxSceneManagerImpl.h - SceneManager and VJSceneManager
// Manages multiple Scene layers, composites them into an FBO
// =============================================================================

#include "tcxScene.h"
#include "TrussC.h"
#include <vector>
#include <memory>
#include <algorithm>
#include <string>

namespace trussc {

// =============================================================================
// SceneManager - Base scene manager with single-pass FBO rendering
// =============================================================================

class SceneManager {
public:
    void setup(int w, int h, int sampleCount = 1) {
        width_ = w;
        height_ = h;
        fbo_.allocate(w, h, sampleCount);
    }

    // -------------------------------------------------------------------------
    // Scene registration
    // -------------------------------------------------------------------------

    template<typename T, typename... Args>
    std::shared_ptr<T> addScene(Args&&... args) {
        auto scene = std::make_shared<T>(std::forward<Args>(args)...);
        scene->setup();
        scenes_.push_back(scene);
        return scene;
    }

    // -------------------------------------------------------------------------
    // Update / Draw
    // -------------------------------------------------------------------------

    void update() {
        for (auto& scene : scenes_) {
            scene->callUpdate();
        }
        updateFbo();
    }

    void draw() {
        fbo_.draw(0, 0);
    }

    void draw(float x, float y) {
        fbo_.draw(x, y);
    }

    void draw(float x, float y, float w, float h) {
        fbo_.draw(x, y, w, h);
    }

    // -------------------------------------------------------------------------
    // Scene access
    // -------------------------------------------------------------------------

    Scene* getScene(const std::string& name) {
        for (auto& scene : scenes_) {
            if (scene->getName() == name) {
                return scene.get();
            }
        }
        return nullptr;
    }

    template<typename T>
    T* getScene() {
        for (auto& scene : scenes_) {
            T* casted = dynamic_cast<T*>(scene.get());
            if (casted) return casted;
        }
        return nullptr;
    }

    // -------------------------------------------------------------------------
    // Alpha control
    // -------------------------------------------------------------------------

    void setAlpha(const std::string& name, float alpha) {
        Scene* scene = getScene(name);
        if (scene) {
            scene->setAlpha(alpha);
        }
    }

    float getAlpha(const std::string& name) {
        Scene* scene = getScene(name);
        return scene ? scene->getAlpha() : 0.0f;
    }

    // -------------------------------------------------------------------------
    // Draw order
    // -------------------------------------------------------------------------

    void moveToFront(const std::string& name) {
        for (auto it = scenes_.begin(); it != scenes_.end(); ++it) {
            if ((*it)->getName() == name) {
                auto scene = *it;
                scenes_.erase(it);
                scenes_.push_back(scene);
                return;
            }
        }
    }

    template<typename T>
    void moveToFront() {
        for (auto it = scenes_.begin(); it != scenes_.end(); ++it) {
            if (dynamic_cast<T*>(it->get())) {
                auto scene = *it;
                scenes_.erase(it);
                scenes_.push_back(scene);
                return;
            }
        }
    }

    // -------------------------------------------------------------------------
    // FBO access
    // -------------------------------------------------------------------------

    Fbo& getFbo() { return fbo_; }
    int getWidth() const { return width_; }
    int getHeight() const { return height_; }

protected:
    // Override for custom render passes (e.g. VJSceneManager)
    virtual void updateFbo() {
        fbo_.begin();  // clears with transparent black

        for (auto& scene : scenes_) {
            scene->callDraw2dBack();
            scene->callDraw();
            scene->callDraw2dFront();
        }

        fbo_.end();
    }

    std::vector<std::shared_ptr<Scene>> scenes_;
    Fbo fbo_;
    int width_ = 0;
    int height_ = 0;
};

// =============================================================================
// VJSceneManager - Extended manager with 3-pass camera rendering
// =============================================================================

class VJSceneManager : public SceneManager {
public:
    // -------------------------------------------------------------------------
    // Camera management
    // -------------------------------------------------------------------------

    void attachCamera(EasyCam* cam) {
        camera_ = cam;
    }

    void detachCamera() {
        camera_ = nullptr;
    }

    EasyCam* getCamera() const {
        return camera_;
    }

protected:
    void updateFbo() override {
        fbo_.begin();  // clears with transparent black

        // Pass 1: 2D background (before camera)
        for (auto& scene : scenes_) {
            scene->callDraw2dBack();
        }

        // Pass 2: 3D content (inside camera)
        if (camera_) {
            camera_->begin();
        }

        for (auto& scene : scenes_) {
            scene->callDraw();
        }

        if (camera_) {
            camera_->end();
        }

        // Pass 3: 2D overlay (after camera)
        for (auto& scene : scenes_) {
            scene->callDraw2dFront();
        }

        fbo_.end();
    }

private:
    EasyCam* camera_ = nullptr;
};

} // namespace trussc
