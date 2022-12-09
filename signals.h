#pragma once
#include "intrusive_list.h"
#include <functional>

// Чтобы не было коллизий с UNIX-сигналами реализация вынесена в неймспейс, по
// той же причине изменено и название файла

namespace signals {

template <typename T>
struct signal;

template <typename... Args>
struct signal<void(Args...)> {

    using slot_t = std::function<void(Args...)>;

    struct connection : intrusive::list_element<struct connection_tag> {
        connection() = default;

        connection(signal* sig, slot_t slot) : sig(sig), slot(std::move(slot)) {
            sig->connections.push_back(*this);
        }

        connection(connection const& other) = delete;
        connection& operator=(connection const& other) = delete;

        connection(connection&& other) : slot(std::move(other.slot)) {
            sig = other.sig;
            if (other.sig != nullptr) {
                replace(other);
            }
        }

        connection& operator=(connection&& other) {
            if (this == &other) {
                return *this;
            }
            disconnect();
            this->sig = other.sig;
            this->slot = std::move(other.slot);
            if (other.sig != nullptr) {
                replace(other);
            }
            return *this;
        }

        void disconnect() {
            if (is_linked()) {
                for (auto it = sig->top; it != nullptr; it = it->next) {
                    if (it->current != sig->connections.end() && &(*it->current) == this) {
                        it->current++;
                    }
                }
            }
            unlink();
            sig = nullptr;
        }

        slot_t slot;
        signal* sig{ nullptr };

    private:
        void replace(connection& other) {
            other.sig->connections.insert(sig->connections.get_iterator(other), *this);
            other.disconnect();
        }
    };

    signal() = default;

    signal(signal const&) = delete;
    signal& operator=(signal const&) = delete;

    ~signal() {
        for (auto it = top; it != nullptr; it = it->next) {
            if (it->current != connections.end()) {
                it->sig = nullptr;
            }
        }

        while (!connections.empty()) {
            connections.back().sig = nullptr;
            connections.pop_back();
        }
    }

    connection connect(std::function<void(Args...)> slot) noexcept {
        return connection(this, std::move(slot));
    }

    void operator()(Args... args) const {
        iterator_holder holder(this);
        while (holder.current != connections.end()) {
            auto copy = holder.current;
            holder.current++;
            copy->slot(std::forward<Args>(args)...);
            if (holder.sig == nullptr) {
                return;
            }
        }
    }

private:
    struct iterator_holder {

        ~iterator_holder() {
            if (sig != nullptr) {
                sig->top = next;
            }
        }

        explicit iterator_holder(signal const* sig) : current(sig->connections.begin()), next(sig->top), sig(sig) {
            sig->top = this;
        }

        typename intrusive::list<connection, connection_tag>::const_iterator current;
        iterator_holder* next;
        signal const* sig;
    };

    intrusive::list<connection, connection_tag> connections;
    mutable iterator_holder* top{ nullptr };
};

} // namespace signals