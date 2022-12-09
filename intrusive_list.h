#pragma once

#include <iterator>

namespace intrusive {

struct default_tag;

template <typename T, typename Tag>
struct list;

struct list_element_base {
    list_element_base() {
        prev = next = this;
    }

    list_element_base(list_element_base const& other) = delete;

    list_element_base& operator=(list_element_base const& other) = delete;

    list_element_base(list_element_base&& other) {
        swap(other);
    }

    list_element_base& operator=(list_element_base&& other) {
        if (this != &other) {
            swap(other);
        }
        return *this;
    }

    void swap(list_element_base& other) {
        std::swap(prev, other.prev);
        std::swap(prev->next, other.prev->next);
        std::swap(next, other.next);
        std::swap(next->prev, other.next->prev);
    }

    ~list_element_base() {
        unlink();
    }

    template <typename T, typename Tag>
    friend struct list;

private:
    list_element_base* next;
    list_element_base* prev;

protected:
    bool is_linked() const {
        return prev != this && next != this;
    }
    void unlink() {
        next->prev = prev;
        prev->next = next;

        next = this;
        prev = this;
    }
};

template <typename Tag = default_tag>
struct list_element : list_element_base {};

template <typename T, typename Tag = default_tag>
struct list {
    template <typename DATA_TYPE>
    struct list_iterator;

    // bidirectional iterator
    using iterator = list_iterator<T>;
    // bidirectional iterator
    using const_iterator = list_iterator<T const>;

    friend class list_iterator<T>;
    friend class list_iterator<T const>;

    list() noexcept {
        end_.prev = &end_;
        end_.next = &end_;
    }

    list(list const& other) = delete;
    list& operator=(list const&) = delete;

    list(list&& other) {
        end_ = std::move(other.end_);
        other.clear();
    }

    list& operator=(list&& other) noexcept {
        if (this != &other) {
            end_ = std::move(other.end_);
            other.clear();
        }
        return *this;
    }

    ~list() {
        clear();
    }

    iterator begin() noexcept {
        return iterator(static_cast<list_element<Tag>*>(end_.next));
    }

    const_iterator begin() const noexcept {
        return const_iterator(static_cast<list_element<Tag>*>(end_.next));
    }

    iterator end() noexcept {
        return iterator(&end_);
    }

    const_iterator end() const noexcept {
        return const_iterator(const_cast<list_element<Tag>*>(&end_));
    }

    iterator get_iterator(T& element) noexcept {
        return iterator(static_cast<list_element<Tag>*>(&element));
    }

    const_iterator get_iterator(T& element) const noexcept {
        return const_iterator(static_cast<list_element<Tag>*>(&element));
    }

    T& front() {
        return *begin();
    }

    T const& front() const noexcept {
        return *begin();
    }

    T& back() {
        return *std::prev(end());
    }

    T const& back() const noexcept {
        return *std::prev(end());
    }

    void push_back(T& element) {
        insert(end(), element);
    }

    void push_front(T& element) {
        insert(begin(), element);
    }

    iterator insert(const_iterator pos, T& element) {
        list_element<Tag>* pos_ptr = pos.ptr_;
        list_element<Tag>* ptr = get_list(element);
        if (pos_ptr == ptr) {
            return iterator(ptr);
        }
        if (ptr->next != ptr) {
            ptr->unlink();
        }
        link(ptr, ptr, pos_ptr);
        return iterator(ptr);
    }

    void clear() {
        erase(begin(), end());
    }

    void pop_front() {
        erase(begin());
    }

    void pop_back() {
        erase(std::prev(end()));
    }

    iterator erase(const_iterator pos) noexcept {
        return erase(pos, std::next(pos));
    }

    iterator erase(const_iterator first, const_iterator last) noexcept {
        if (first != last) {
            auto* start_ptr = static_cast<list_element<Tag>*>(first.ptr_);
            auto* end_ptr = static_cast<list_element<Tag>*>(last.ptr_->prev);
            cut(start_ptr, end_ptr);
            auto* t = start_ptr;
            while (t != end_ptr) {
                auto* tmp = t->next;
                t->unlink();
                t = static_cast<list_element<Tag>*>(tmp);
            }
            t->unlink();
        }
        return iterator(last.ptr_);
    }

    bool empty() const noexcept {
        return &end_ == end_.next;
    }

    void splice(const_iterator pos, list<T>& other, const_iterator first, const_iterator last) noexcept {
        if (first == last) {
            return;
        }
        auto* start_ptr = static_cast<list_element<Tag>*>(first.ptr_);
        auto* end_ptr = static_cast<list_element<Tag>*>(last.ptr_->prev);
        auto* tmp_pos = static_cast<list_element<Tag>*>(pos.ptr_);
        cut(start_ptr, end_ptr);
        link(start_ptr, end_ptr, tmp_pos);
    }

    template <typename E>
    struct list_iterator {
        using iterator_category = std::bidirectional_iterator_tag;
        using value_type = std::remove_const_t<T>;
        using difference_type = std::ptrdiff_t;
        using pointer = value_type*;
        using reference = value_type&;

        list_iterator() = default;

        template <typename P>
        list_iterator(list_iterator<P> other, std::enable_if_t<std::is_same_v<E, P> || std::is_const_v<E>>* = nullptr) {
            ptr_ = other.ptr_;
        }

        E& operator*() const noexcept {
            return *static_cast<E*>(ptr_);
        }
        E* operator->() const noexcept {
            return static_cast<E*>(ptr_);
        }

        list_iterator<E>& operator++() & noexcept {
            ptr_ = static_cast<list_element<Tag>*>(ptr_->next);
            return *this;
        }
        list_iterator<E>& operator--() & noexcept {
            ptr_ = static_cast<list_element<Tag>*>(ptr_->prev);
            return *this;
        }

        list_iterator<E> operator++(int) &noexcept {
            ptr_ = static_cast<list_element<Tag>*>(ptr_->next);
            return list_iterator<E>(static_cast<list_element<Tag>*>(ptr_->prev));
        }
        list_iterator<E> operator--(int) &noexcept {
            ptr_ = static_cast<list_element<Tag>*>(ptr_->prev);
            return list_iterator<E>(static_cast<list_element<Tag>*>(ptr_->next));
        }

        template <typename other_t>
        bool operator==(list_iterator<other_t> const& other) const {
            return ptr_ == other.ptr_;
        }

        template <typename other_t>
        bool operator!=(list_iterator<other_t> const& other) const {
            return ptr_ != other.ptr_;
        }

    private:
        explicit list_iterator(list_element<Tag>* cur) noexcept : ptr_(cur) {}
        friend list;
        friend const_iterator;
        friend list_element_base;

        list_element<Tag>* ptr_;
    };

private:
    list_element<Tag> end_;

    list_element<Tag>* get_list(T& elem) {
        return static_cast<list_element<Tag>*>(&elem);
    }

    static void link(list_element<Tag>* start_ptr, list_element<Tag>* end_ptr, list_element<Tag>* insert_pos) {
        end_ptr->next = insert_pos;
        start_ptr->prev = insert_pos->prev;

        insert_pos->prev->next = start_ptr;
        insert_pos->prev = end_ptr;
    }

    static void cut(list_element<Tag>* start_ptr, list_element<Tag>* end_ptr) {
        end_ptr->next->prev = start_ptr->prev;
        start_ptr->prev->next = end_ptr->next;
    }
};

} // namespace intrusive