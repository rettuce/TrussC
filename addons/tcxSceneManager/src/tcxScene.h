#pragma once

// =============================================================================
// tcxScene.h - Scene base class
// Override draw methods and enter/exit callbacks to build visual layers
// =============================================================================

#include <string>

namespace trussc {

class Scene {
public:
    Scene(const std::string& name, float alpha = 0.0f)
        : name_(name)
        , alpha_(alpha)
        , wasVisible_(alpha > 0.001f)
    {}

    virtual ~Scene() = default;

    // -------------------------------------------------------------------------
    // Override these in subclasses
    // -------------------------------------------------------------------------

    virtual void setup() {}
    virtual void update() {}
    virtual void draw() {}          // 3D content (in camera space if VJSceneManager)
    virtual void draw2dBack() {}    // 2D background (before camera)
    virtual void draw2dFront() {}   // 2D overlay (after camera)
    virtual void enter() {}         // alpha went 0 -> visible
    virtual void exit() {}          // alpha went visible -> 0

    // -------------------------------------------------------------------------
    // Properties
    // -------------------------------------------------------------------------

    const std::string& getName() const { return name_; }

    float getAlpha() const { return alpha_; }

    void setAlpha(float a) {
        alpha_ = (a < 0.0f) ? 0.0f : (a > 1.0f) ? 1.0f : a;
        // enter/exit detection is handled in callUpdate
    }

    bool isVisible() const { return alpha_ > 0.001f; }

    // -------------------------------------------------------------------------
    // Called by SceneManager (handles enter/exit lifecycle)
    // -------------------------------------------------------------------------

    void callUpdate() {
        bool visible = isVisible();

        if (visible && !wasVisible_) {
            enter();
        } else if (!visible && wasVisible_) {
            exit();
        }
        wasVisible_ = visible;

        if (visible) {
            update();
        }
    }

    void callDraw() {
        if (isVisible()) {
            draw();
        }
    }

    void callDraw2dBack() {
        if (isVisible()) {
            draw2dBack();
        }
    }

    void callDraw2dFront() {
        if (isVisible()) {
            draw2dFront();
        }
    }

private:
    std::string name_;
    float alpha_ = 0.0f;
    bool wasVisible_ = false;
};

} // namespace trussc
