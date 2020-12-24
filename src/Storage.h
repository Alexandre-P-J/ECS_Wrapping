
template<class Entity, class Type>
class Storage {
    static const constexpr Entity invalid = std::numeric_limits<Entity>::max();
    std::vector<Entity> sparse;
    std::vector<Entity> packed;
    std::vector<Type> storage;

    using const_iterator = typename std::vector<Entity>::const_iterator;

public:
    bool contains(const Entity element) const {
        assert(invalid > element);
        return sparse.size() > element && sparse[element] != invalid; 
    }

    std::size_t size() const {
        return packed.size();
    }

    template<class... Args>
    Type emplace(const Entity element, Args&&... args) {
        assert(!contains(element));
        const std::size_t max = std::max(sparse.size(), std::size_t{element + 1});
        sparse.resize(max, invalid);
        sparse[element] = Entity{packed.size()};
        packed.emplace_back(element);
        return storage.emplace_back(args...);
    }


    void remove(const Entity element) {
        assert(contains(element));
        // copy end data to deleted data
        packed[sparse[element]] = packed.back();
        storage[sparse[element]] = storage.back();
        // update sparse, invalidate deleted
        sparse[packed.back()] = sparse[element];
        sparse[element] = invalid;
        // free
        packed.pop_back();
        storage.pop_back();
    }

    const std::vector<Entity>& entities() {
        return packed;
    }

    void clear() {
        sparse.clear();
        packed.clear();
        storage.clear();
    }

    const_iterator begin() {
        return packed.cbegin();
    }

    const_iterator end() {
        return packed.cend();
    }
    
    // sort
};