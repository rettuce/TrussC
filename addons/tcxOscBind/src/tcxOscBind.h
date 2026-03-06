#pragma once

// =============================================================================
// tcxOscBind - Declarative OSC parameter binding for TrussC
// Simplified port of ofxPubSubOsc on top of tcxOsc
// =============================================================================

#include "tcxOsc.h"
#include "TrussC.h"

#include <map>
#include <string>
#include <memory>
#include <variant>
#include <functional>
#include <vector>
#include <utility>

namespace trussc {

// =============================================================================
// OscBindManager - Singleton managing all OSC bindings
// =============================================================================
class OscBindManager {
public:
    static OscBindManager& instance() {
        static OscBindManager manager;
        return manager;
    }

    // -------------------------------------------------------------------------
    // Subscribe: bind variable to incoming OSC address
    // -------------------------------------------------------------------------

    void subscribe(int port, const std::string& address, float& value) {
        ensureReceiver(port);
        addSubscribeBinding(port, address, SubscribeTarget{&value});
    }

    void subscribe(int port, const std::string& address, int& value) {
        ensureReceiver(port);
        addSubscribeBinding(port, address, SubscribeTarget{&value});
    }

    void subscribe(int port, const std::string& address, bool& value) {
        ensureReceiver(port);
        addSubscribeBinding(port, address, SubscribeTarget{&value});
    }

    void subscribe(int port, const std::string& address, std::string& value) {
        ensureReceiver(port);
        addSubscribeBinding(port, address, SubscribeTarget{&value});
    }

    void subscribe(int port, const std::string& address, Vec2& value) {
        ensureReceiver(port);
        addSubscribeBinding(port, address, SubscribeTarget{&value});
    }

    void subscribe(int port, const std::string& address, Vec3& value) {
        ensureReceiver(port);
        addSubscribeBinding(port, address, SubscribeTarget{&value});
    }

    void subscribe(int port, const std::string& address,
                   std::function<void(const OscMessage&)> callback) {
        ensureReceiver(port);
        addSubscribeBinding(port, address, SubscribeTarget{std::move(callback)});
    }

    void unsubscribe(int port, const std::string& address) {
        auto it = subscribeBindings_.find(port);
        if (it != subscribeBindings_.end()) {
            it->second.erase(address);
        }
    }

    void unsubscribeAll() {
        subscribeBindings_.clear();
        receivers_.clear();
    }

    // -------------------------------------------------------------------------
    // Publish: bind variable to outgoing OSC address (sends on change)
    // -------------------------------------------------------------------------

    void publish(const std::string& host, int port,
                 const std::string& address, const float& value) {
        ensureSender(host, port);
        auto dest = makeDestKey(host, port);
        publishBindings_[dest].emplace_back(
            PublishBinding{address, PublishSource{&value}, PublishOldValue{value}});
    }

    void publish(const std::string& host, int port,
                 const std::string& address, const int& value) {
        ensureSender(host, port);
        auto dest = makeDestKey(host, port);
        publishBindings_[dest].emplace_back(
            PublishBinding{address, PublishSource{&value}, PublishOldValue{value}});
    }

    void publish(const std::string& host, int port,
                 const std::string& address, const std::string& value) {
        ensureSender(host, port);
        auto dest = makeDestKey(host, port);
        publishBindings_[dest].emplace_back(
            PublishBinding{address, PublishSource{&value}, PublishOldValue{value}});
    }

    void unpublish(const std::string& host, int port,
                   const std::string& address) {
        auto dest = makeDestKey(host, port);
        auto it = publishBindings_.find(dest);
        if (it == publishBindings_.end()) return;

        auto& bindings = it->second;
        bindings.erase(
            std::remove_if(bindings.begin(), bindings.end(),
                           [&](const PublishBinding& b) {
                               return b.address == address;
                           }),
            bindings.end());
    }

    void unpublishAll() {
        publishBindings_.clear();
        senders_.clear();
    }

    // -------------------------------------------------------------------------
    // Update: call once per frame
    // -------------------------------------------------------------------------

    void update() {
        processIncoming();
        processOutgoing();
    }

private:
    OscBindManager() = default;
    ~OscBindManager() = default;
    OscBindManager(const OscBindManager&) = delete;
    OscBindManager& operator=(const OscBindManager&) = delete;

    // -------------------------------------------------------------------------
    // Subscribe types
    // -------------------------------------------------------------------------

    using SubscribeTarget = std::variant<
        float*,
        int*,
        bool*,
        std::string*,
        Vec2*,
        Vec3*,
        std::function<void(const OscMessage&)>
    >;

    // -------------------------------------------------------------------------
    // Publish types
    // -------------------------------------------------------------------------

    using PublishSource = std::variant<const float*, const int*, const std::string*>;
    using PublishOldValue = std::variant<float, int, std::string>;
    using DestKey = std::pair<std::string, int>;

    struct PublishBinding {
        std::string address;
        PublishSource source;
        PublishOldValue oldValue;
    };

    // -------------------------------------------------------------------------
    // Internal helpers
    // -------------------------------------------------------------------------

    static DestKey makeDestKey(const std::string& host, int port) {
        return {host, port};
    }

    void ensureReceiver(int port) {
        if (receivers_.count(port)) return;
        auto receiver = std::make_unique<OscReceiver>();
        receiver->setup(port);
        receivers_[port] = std::move(receiver);
    }

    void ensureSender(const std::string& host, int port) {
        auto key = makeDestKey(host, port);
        if (senders_.count(key)) return;
        auto sender = std::make_unique<OscSender>();
        sender->setup(host, port);
        senders_[key] = std::move(sender);
    }

    void addSubscribeBinding(int port, const std::string& address,
                             SubscribeTarget target) {
        subscribeBindings_[port].emplace(address, std::move(target));
    }

    // -------------------------------------------------------------------------
    // Incoming message dispatch
    // -------------------------------------------------------------------------

    void processIncoming() {
        for (auto& [port, receiver] : receivers_) {
            OscMessage msg;
            while (receiver->getNextMessage(msg)) {
                dispatchMessage(port, msg);
            }
        }
    }

    void dispatchMessage(int port, const OscMessage& msg) {
        auto portIt = subscribeBindings_.find(port);
        if (portIt == subscribeBindings_.end()) return;

        const auto& address = msg.getAddress();
        auto range = portIt->second.equal_range(address);

        for (auto it = range.first; it != range.second; ++it) {
            applyMessage(it->second, msg);
        }
    }

    void applyMessage(SubscribeTarget& target, const OscMessage& msg) {
        std::visit([&msg](auto&& dest) {
            using T = std::decay_t<decltype(dest)>;

            if constexpr (std::is_same_v<T, float*>) {
                if (msg.getArgCount() >= 1) {
                    *dest = msg.getArgAsFloat(0);
                }
            }
            else if constexpr (std::is_same_v<T, int*>) {
                if (msg.getArgCount() >= 1) {
                    *dest = msg.getArgAsInt(0);
                }
            }
            else if constexpr (std::is_same_v<T, bool*>) {
                if (msg.getArgCount() >= 1) {
                    // Support both bool type tags and int-as-bool
                    char tag = msg.getArgType(0);
                    if (tag == 'T' || tag == 'F') {
                        *dest = msg.getArgAsBool(0);
                    } else {
                        *dest = msg.getArgAsInt(0) != 0;
                    }
                }
            }
            else if constexpr (std::is_same_v<T, std::string*>) {
                if (msg.getArgCount() >= 1) {
                    *dest = msg.getArgAsString(0);
                }
            }
            else if constexpr (std::is_same_v<T, Vec2*>) {
                if (msg.getArgCount() >= 2) {
                    dest->x = msg.getArgAsFloat(0);
                    dest->y = msg.getArgAsFloat(1);
                }
            }
            else if constexpr (std::is_same_v<T, Vec3*>) {
                if (msg.getArgCount() >= 3) {
                    dest->x = msg.getArgAsFloat(0);
                    dest->y = msg.getArgAsFloat(1);
                    dest->z = msg.getArgAsFloat(2);
                }
            }
            else if constexpr (std::is_same_v<T, std::function<void(const OscMessage&)>>) {
                if (dest) {
                    dest(msg);
                }
            }
        }, target);
    }

    // -------------------------------------------------------------------------
    // Outgoing change detection & send
    // -------------------------------------------------------------------------

    void processOutgoing() {
        for (auto& [dest, bindings] : publishBindings_) {
            auto senderIt = senders_.find(dest);
            if (senderIt == senders_.end()) continue;
            auto& sender = *senderIt->second;

            for (auto& binding : bindings) {
                sendIfChanged(sender, binding);
            }
        }
    }

    void sendIfChanged(OscSender& sender, PublishBinding& binding) {
        std::visit([&](auto&& src) {
            using T = std::decay_t<decltype(src)>;

            if constexpr (std::is_same_v<T, const float*>) {
                float current = *src;
                auto* old = std::get_if<float>(&binding.oldValue);
                if (!old || *old != current) {
                    OscMessage msg(binding.address);
                    msg.addFloat(current);
                    sender.send(msg);
                    binding.oldValue = current;
                }
            }
            else if constexpr (std::is_same_v<T, const int*>) {
                int current = *src;
                auto* old = std::get_if<int>(&binding.oldValue);
                if (!old || *old != current) {
                    OscMessage msg(binding.address);
                    msg.addInt(current);
                    sender.send(msg);
                    binding.oldValue = current;
                }
            }
            else if constexpr (std::is_same_v<T, const std::string*>) {
                const std::string& current = *src;
                auto* old = std::get_if<std::string>(&binding.oldValue);
                if (!old || *old != current) {
                    OscMessage msg(binding.address);
                    msg.addString(current);
                    sender.send(msg);
                    binding.oldValue = current;
                }
            }
        }, binding.source);
    }

    // -------------------------------------------------------------------------
    // Storage
    // -------------------------------------------------------------------------

    // One receiver per listen port
    std::map<int, std::unique_ptr<OscReceiver>> receivers_;

    // One sender per destination (host, port)
    std::map<DestKey, std::unique_ptr<OscSender>> senders_;

    // Subscribe bindings: port -> multimap<address, target>
    std::map<int, std::multimap<std::string, SubscribeTarget>> subscribeBindings_;

    // Publish bindings: destination -> vector<binding>
    std::map<DestKey, std::vector<PublishBinding>> publishBindings_;
};

// =============================================================================
// Free-function API (delegates to singleton)
// =============================================================================

// Subscribe -------------------------------------------------------------------

inline void oscSubscribe(int port, const std::string& address, float& value) {
    OscBindManager::instance().subscribe(port, address, value);
}

inline void oscSubscribe(int port, const std::string& address, int& value) {
    OscBindManager::instance().subscribe(port, address, value);
}

inline void oscSubscribe(int port, const std::string& address, bool& value) {
    OscBindManager::instance().subscribe(port, address, value);
}

inline void oscSubscribe(int port, const std::string& address, std::string& value) {
    OscBindManager::instance().subscribe(port, address, value);
}

inline void oscSubscribe(int port, const std::string& address, Vec2& value) {
    OscBindManager::instance().subscribe(port, address, value);
}

inline void oscSubscribe(int port, const std::string& address, Vec3& value) {
    OscBindManager::instance().subscribe(port, address, value);
}

inline void oscSubscribe(int port, const std::string& address,
                          std::function<void(const OscMessage&)> callback) {
    OscBindManager::instance().subscribe(port, address, std::move(callback));
}

inline void oscUnsubscribe(int port, const std::string& address) {
    OscBindManager::instance().unsubscribe(port, address);
}

inline void oscUnsubscribeAll() {
    OscBindManager::instance().unsubscribeAll();
}

// Publish ---------------------------------------------------------------------

inline void oscPublish(const std::string& host, int port,
                        const std::string& address, const float& value) {
    OscBindManager::instance().publish(host, port, address, value);
}

inline void oscPublish(const std::string& host, int port,
                        const std::string& address, const int& value) {
    OscBindManager::instance().publish(host, port, address, value);
}

inline void oscPublish(const std::string& host, int port,
                        const std::string& address, const std::string& value) {
    OscBindManager::instance().publish(host, port, address, value);
}

inline void oscUnpublish(const std::string& host, int port,
                          const std::string& address) {
    OscBindManager::instance().unpublish(host, port, address);
}

inline void oscUnpublishAll() {
    OscBindManager::instance().unpublishAll();
}

// Update ----------------------------------------------------------------------

inline void oscBindUpdate() {
    OscBindManager::instance().update();
}

}  // namespace trussc
