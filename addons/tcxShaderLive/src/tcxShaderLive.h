#pragma once

// =============================================================================
// tcxShaderLive - TrussC shader live-reload addon
// Watches shader source files for changes and triggers a reload callback.
// Port of ofxShaderOnTheFly adapted for TrussC's build-time shader pipeline.
// =============================================================================

#include <filesystem>
#include <functional>
#include <string>
#include <vector>
#include <algorithm>

namespace trussc {

// Forward declarations
class ShaderLive;

// -----------------------------------------------------------------------------
// ShaderLiveManager - static registry of all ShaderLive instances
// -----------------------------------------------------------------------------
class ShaderLiveManager {
public:
    // Call after ShaderLive class is fully defined (implementation below)
    static void updateAll();

private:
    friend class ShaderLive;

    static std::vector<ShaderLive*>& instances() {
        static std::vector<ShaderLive*> s_instances;
        return s_instances;
    }

    static void registerInstance(ShaderLive* instance) {
        instances().push_back(instance);
    }

    static void unregisterInstance(ShaderLive* instance) {
        auto& vec = instances();
        vec.erase(std::remove(vec.begin(), vec.end(), instance), vec.end());
    }
};

// -----------------------------------------------------------------------------
// ShaderLive
// -----------------------------------------------------------------------------
class ShaderLive {
public:
    ShaderLive() {
        ShaderLiveManager::registerInstance(this);
    }

    ~ShaderLive() {
        ShaderLiveManager::unregisterInstance(this);
    }

    ShaderLive(const ShaderLive&) = delete;
    ShaderLive& operator=(const ShaderLive&) = delete;

    ShaderLive(ShaderLive&& other) noexcept
        : watchedFiles_(std::move(other.watchedFiles_))
        , reloadCallback_(std::move(other.reloadCallback_))
        , checkInterval_(other.checkInterval_)
        , frameCounter_(other.frameCounter_)
        , enabled_(other.enabled_)
        , changed_(other.changed_) {
        ShaderLiveManager::unregisterInstance(&other);
        ShaderLiveManager::registerInstance(this);
    }

    ShaderLive& operator=(ShaderLive&& other) noexcept {
        if (this != &other) {
            watchedFiles_ = std::move(other.watchedFiles_);
            reloadCallback_ = std::move(other.reloadCallback_);
            checkInterval_ = other.checkInterval_;
            frameCounter_ = other.frameCounter_;
            enabled_ = other.enabled_;
            changed_ = other.changed_;
            ShaderLiveManager::unregisterInstance(&other);
        }
        return *this;
    }

    // ---- watch ----

    void watch(const std::string& path) {
        std::filesystem::path filePath(path);
        if (!std::filesystem::exists(filePath)) return;

        WatchedFile entry;
        entry.path = filePath;
        entry.lastWriteTime = std::filesystem::last_write_time(filePath);
        watchedFiles_.push_back(std::move(entry));
    }

    void watch(const std::vector<std::string>& paths) {
        for (const auto& path : paths) {
            watch(path);
        }
    }

    // ---- callback ----

    void setReloadCallback(std::function<void()> callback) {
        reloadCallback_ = std::move(callback);
    }

    // ---- interval ----

    void setCheckInterval(int frames) {
        checkInterval_ = frames;
    }

    // ---- update ----

    void update() {
        if (!enabled_ || watchedFiles_.empty()) return;

        ++frameCounter_;
        if (frameCounter_ < checkInterval_) return;
        frameCounter_ = 0;

        changed_ = false;

        for (auto& entry : watchedFiles_) {
            if (!std::filesystem::exists(entry.path)) continue;

            auto currentTime = std::filesystem::last_write_time(entry.path);
            if (currentTime != entry.lastWriteTime) {
                entry.lastWriteTime = currentTime;
                changed_ = true;
            }
        }

        if (changed_ && reloadCallback_) {
            reloadCallback_();
        }
    }

    // ---- enable / disable ----

    void enable() { enabled_ = true; }
    void disable() { enabled_ = false; }
    bool isEnabled() const { return enabled_; }

    // ---- state ----

    bool hasChanged() const { return changed_; }

    void clearWatched() {
        watchedFiles_.clear();
        changed_ = false;
        frameCounter_ = 0;
    }

private:
    struct WatchedFile {
        std::filesystem::path path;
        std::filesystem::file_time_type lastWriteTime;
    };

    std::vector<WatchedFile> watchedFiles_;
    std::function<void()> reloadCallback_;
    int checkInterval_ = 60;
    int frameCounter_ = 0;
    bool enabled_ = true;
    bool changed_ = false;
};

// -----------------------------------------------------------------------------
// ShaderLiveManager::updateAll() — deferred implementation (ShaderLive complete)
// -----------------------------------------------------------------------------
inline void ShaderLiveManager::updateAll() {
    for (auto* instance : instances()) {
        instance->update();
    }
}

} // namespace trussc
