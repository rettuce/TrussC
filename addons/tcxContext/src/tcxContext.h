#pragma once

// =============================================================================
// tcxContext - TrussC Context addon
// Type-keyed global singleton registry for sharing context objects across app
// =============================================================================

#include <memory>
#include <type_traits>
#include <typeindex>
#include <unordered_map>
#include <utility>

namespace trussc {

// -----------------------------------------------------------------------------
// Context base
// -----------------------------------------------------------------------------
struct Context {
    using Ref = std::shared_ptr<Context>;

    virtual ~Context() = default;
    virtual void update() {}
};

// -----------------------------------------------------------------------------
// ContextManager — Meyer's singleton, type-keyed registry
// -----------------------------------------------------------------------------
class ContextManager {
public:
    static ContextManager& instance() {
        static ContextManager mgr;
        return mgr;
    }

    template <typename T, typename... Args>
    std::shared_ptr<T> create(Args&&... args) {
        static_assert(std::is_base_of<Context, T>::value,
                      "T must derive from trussc::Context");
        auto ptr = std::make_shared<T>(std::forward<Args>(args)...);
        contexts_[std::type_index(typeid(T))] = ptr;
        return ptr;
    }

    template <typename T>
    T* get() const {
        auto it = contexts_.find(std::type_index(typeid(T)));
        if (it == contexts_.end()) return nullptr;
        return static_cast<T*>(it->second.get());
    }

    void update() {
        for (auto& pair : contexts_) {
            pair.second->update();
        }
    }

    void clear() { contexts_.clear(); }

    ContextManager(const ContextManager&) = delete;
    ContextManager& operator=(const ContextManager&) = delete;
    ContextManager(ContextManager&&) = delete;
    ContextManager& operator=(ContextManager&&) = delete;

private:
    ContextManager() = default;

    std::unordered_map<std::type_index, Context::Ref> contexts_;
};

// -----------------------------------------------------------------------------
// Free functions
// -----------------------------------------------------------------------------
template <typename T, typename... Args>
inline std::shared_ptr<T> createContext(Args&&... args) {
    return ContextManager::instance().create<T>(std::forward<Args>(args)...);
}

template <typename T>
inline T* getContext() {
    return ContextManager::instance().get<T>();
}

inline void updateContexts() {
    ContextManager::instance().update();
}

} // namespace trussc

// -----------------------------------------------------------------------------
// Convenience macro
// -----------------------------------------------------------------------------
#define tcxGetContext(T) trussc::ContextManager::instance().get<T>()
