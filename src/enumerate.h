#pragma once

#include <iterator>

template <typename I, typename T>
struct IndexedElement {
    I index;
    T value;
};

template <typename T>
class Indexer {
public:
    T container;

    Indexer(T&& param) : container{std::forward<T>(param)} {}

    template <typename It>
    class IndexedIterator {
    public:
        using IType = decltype(It{} - It{});

        It iterator;
        const It beg;

        IndexedIterator(It&& it) : iterator{std::forward<It>(it)}, beg{std::forward<It>(it)} {}

        auto operator*() {
            return IndexedElement{
                iterator - beg,
                *iterator
            };
        }

        auto operator++() {
            ++iterator;
            return *this;
        }

        auto operator==(const IndexedIterator& rhs) const {
            return iterator == rhs.iterator;
        };

        auto operator<=>(const IndexedIterator& rhs) const {
            return iterator <=> rhs.iterator;
        };
    };

    auto begin() const { return IndexedIterator{std::begin(container)}; }
    auto end() const { return IndexedIterator{std::end(container)}; }
};

template <typename T>
auto enumerate(T&& container) {
    return Indexer<T>{std::forward<T>(container)};
}