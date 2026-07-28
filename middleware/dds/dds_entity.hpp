#pragma once

#include <dds/dds.h>

#include <stdexcept>
#include <string>

namespace mostrider::dds {

// RAII wrapper around a Cyclone DDS entity handle (dds_entity_t). Cyclone
// DDS ships only a C API on this distro (no cyclonedds-cxx package on
// Ubuntu 24.04) — see ADR-0001. This wrapper is the thin C++ layer over it:
// move-only so a handle is never deleted twice, throws on creation failure
// so callers don't need to check every dds_create_* return value by hand.
class Entity {
public:
    explicit Entity(dds_entity_t handle) : handle_(handle) {
        if (handle_ < 0) {
            throw std::runtime_error(
                "dds entity creation failed: " + std::string(dds_strretcode(-handle_)));
        }
    }

    Entity(const Entity&) = delete;
    Entity& operator=(const Entity&) = delete;

    Entity(Entity&& other) noexcept : handle_(other.handle_) { other.handle_ = 0; }
    Entity& operator=(Entity&& other) noexcept {
        if (this != &other) {
            reset();
            handle_ = other.handle_;
            other.handle_ = 0;
        }
        return *this;
    }

    ~Entity() { reset(); }

    dds_entity_t get() const noexcept { return handle_; }
    operator dds_entity_t() const noexcept { return handle_; } // NOLINT(google-explicit-constructor)

private:
    void reset() noexcept {
        if (handle_ > 0) {
            dds_delete(handle_);
        }
        handle_ = 0;
    }

    dds_entity_t handle_ = 0;
};

} // namespace mostrider::dds
