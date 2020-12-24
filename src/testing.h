#include <iostream>
#include <entt/entt.hpp>
#include <functional>
#include <limits>
#include <vector>
#include <unordered_map>
#include <memory>


namespace JuicyEngine {

/**
 * @brief runtime view of entities mathing both runtime and non-runtime
 * components.
 * To be used from an scripting language.
 */
class runtime_view {
    template<class RuntimeType>
    friend class RegistryProxy;

    entt::runtime_view rv;
    std::vector<entt::sparse_set*> scripting_view;
    std::vector<entt::sparse_set*> scripting_filter;

    using underlying_iterator = typename entt::runtime_view::iterator;
    
    class view_iterator {
        entt::runtime_view& rv;
        underlying_iterator it;
        std::vector<entt::sparse_set*>& scripting_view;
        std::vector<entt::sparse_set*>& scripting_filter;
        
        bool scripting_view_checks(const entt::entity entt) const {
            bool ok = true;
            for (auto vsp : scripting_view) {
                ok = ok && vsp->contains(entt);
            }
            for (auto fsp : scripting_filter) {
                ok = ok && !fsp->contains(entt);
            }
            return ok;
        }
        // increment it until it is valid, does nothing if it is valid
        void advance_until_valid() {
            while(it != rv.end() && !scripting_view_checks(*it)) {
                ++it;
            }
        }

    public:
        using difference_type = typename underlying_iterator::difference_type;
        using value_type = typename underlying_iterator::value_type;
        using pointer = typename underlying_iterator::pointer;
        using reference = typename underlying_iterator::reference;
        //using iterator_category = std::bidirectional_iterator_tag;

        view_iterator(entt::runtime_view& rv, underlying_iterator it,
            std::vector<entt::sparse_set*>& cpool, std::vector<entt::sparse_set*>& fpool)
            : rv(rv), it(it), scripting_view(cpool), scripting_filter(fpool) {
                advance_until_valid();
            }

        view_iterator& operator++() {
            ++it;
            advance_until_valid();
            return *this;
        }

        bool operator==(const view_iterator &other) const {
            return it == other.it;
        }

        bool operator!=(const view_iterator &other) const {
            return !(it == other.it);
        }
        pointer operator->() const {
            return &(*it);
        }
        reference operator*() const {
            return *it;
        }
    };

    runtime_view(entt::runtime_view rv, std::vector<entt::sparse_set*> cpool = {},
        std::vector<entt::sparse_set*> cfilter = {}) : rv(rv), scripting_view(cpool),
        scripting_filter(cfilter) {}

public:

    view_iterator begin() {
        return view_iterator(rv, rv.begin(), scripting_view, scripting_filter);
    }
    view_iterator end() {
        return view_iterator(rv, rv.end(), scripting_view, scripting_filter);
    }
};


class Registry;


/**
 * @brief Base RegistryProxy used for internal transactions from
 * the Registry to its Proxies.
 */
class RegistryBaseProxy {
    friend class Registry;
    virtual void destroy_unchecked(const entt::entity entt) = 0;
};


/**
 * @brief Manages an ECS Registry that allows both C++ components
 * and scripting runtime components.
 * Provides access to all non-runtime ECS operations.
 * Refer to RegistryProxy<ScriptingOBJ> for the api providing both
 * runtime and internal operations.
 */
class Registry {
    template<class T>
    friend class RegistryProxy;

    entt::registry reg;
    std::vector<RegistryBaseProxy*> proxies;
    std::unordered_map<std::string, const entt::id_type> internal;

public:

    template<class C>
    void expose_internal_component(const std::string& name) {
        const auto id = entt::type_hash<C>::value();
        const auto r = internal.try_emplace(name, id);
        assert(r.second); //Non repeated name and single call per exposed type
    }

    entt::entity create() {
        return reg.create();
    }

    bool valid(const entt::entity entt) const {
        return reg.valid(entt);
    }

    // removing an invalid entt is UB. (same as in EnTT)
    void destroy(const entt::entity entt) {
        for (auto proxy : proxies) {
            proxy->destroy_unchecked(entt);
        }
        reg.destroy(entt);
    }

    template<class C, class... Args>
    C& emplace(const entt::entity entt, Args&&... args) {
        return reg.emplace<C>(entt, args...);
    }

    template<class C, class... Args>
    C& emplace_or_replace(const entt::entity entt, Args&&... args) {
        return reg.emplace_or_replace<C>(entt, args...);
    }

    template<class... C>
    void remove(const entt::entity entt) {
        reg.remove<C...>(entt);
    }

    template<class... C, class... F>
    entt::view<entt::exclude_t<F...>, C...> view(const entt::exclude_t<F...> exclude = {}) {
        return reg.view<C...>(exclude);
    }

    template<class C>
    C& get(const entt::entity entt) {
        return reg.get<C>(entt);
    }
};





/**
 * @brief Proxy to Registry, to be used from a scripting language.
 * Allows access to all ECS runtime and non-runtime components and
 * has all static and runtime operations.
 * 
 * @tparam RuntimeType Data type or handler of the scripting language
 * component type.
 */
template<class RuntimeType>
class RegistryProxy : public RegistryBaseProxy {
    std::shared_ptr<Registry> manager;
    using storage = entt::storage<RuntimeType>;
    std::unordered_map<std::string, storage*> runtime;

    void destroy_unchecked(const entt::entity entt) override {
        for (auto& ss : runtime) {
            if (ss.second->contains(entt))
                ss.second->remove(entt);
        }
    }

public:
    RegistryProxy(std::shared_ptr<JuicyEngine::Registry> manager) : manager{manager} {
        if (!manager) {
            throw std::invalid_argument("ECS Manager pointer shall be dereferencable");
        }
        manager->proxies.push_back(this);
    }

    ~RegistryProxy() {
        for (auto& e : runtime) {
            delete e.second;
        }
    }

    entt::entity create() const {
        return manager->create();
    }

    void destroy(const entt::entity entt) {
        if (manager->valid(entt)) {
            destroy_unchecked(entt);
            manager->reg.destroy(entt);
        }
    }

    RuntimeType& set(const entt::entity entt, const std::string& component_name,
        const RuntimeType& component_handle) {
        auto r = runtime.emplace(component_name, nullptr);
        if (r.second) { // nullptr was inserted
            auto ptr = new storage{};
            r.first->second = ptr;
            return ptr->emplace(entt, component_handle);
        }
        // storage emplace its UB if entity already has the component
        // and get its UB if entity doesn't have the entity, so check required.
        storage* components = r.first->second;
        if (components->contains(entt)) {
            auto& tmp = components->get(entt);
            tmp = component_handle;
            return tmp;
        }
        return components->emplace(entt, component_handle);
    }

    template<class C>
    C& set(const entt::entity entt) {
        
    }

    RuntimeType& get(const entt::entity entt, const std::string& component_name) {

    }

    template<class C>
    C& get(const entt::entity entt, const std::string& component_name) {
        
    }

    void remove(const entt::entity entt, const std::string& component) {
        if (auto it = runtime.find(component); it != runtime.end()) {

        }
        else if (auto it = manager->internal.find(component); it != manager->internal.end()) {
            
        }
    }




    //allocation may invaldate view! find solution
    runtime_view view(const std::vector<std::string>& components,
            const std::vector<std::string>& filters = {}) {
        std::vector<entt::id_type> internal_c;
        internal_c.reserve(components.size());
        std::vector<entt::id_type> internal_f;
        internal_f.reserve(filters.size());
        std::vector<entt::sparse_set*> runtime_c;
        runtime_c.reserve(components.size());
        std::vector<entt::sparse_set*> runtime_f;
        runtime_f.reserve(filters.size());
        for (const auto& comp : components) {
            if (auto it = runtime.find(comp); it != runtime.end()) {
                runtime_c.push_back(it->second);
            }
            else if (auto it = manager->internal.find(comp); it != manager->internal.end()) {
                internal_c.push_back(it->second);
            }
            else {
                // workaround to return an empty view
                auto endit = internal_c.end();
                return runtime_view(manager->reg.runtime_view(endit, endit));
            }
        }
        for (const auto& filt : filters) {
            if (auto it = runtime.find(filt); it != runtime.end()) {
                runtime_f.push_back(it->second);
            }
            else if (auto it = manager->internal.find(filt); it != manager->internal.end()) {
                internal_f.push_back(it->second);
            }
        }
        auto rv = manager->reg.runtime_view(internal_c.begin(), internal_c.end(), internal_f.begin(), internal_f.end());
        return runtime_view{rv, std::move(runtime_c), std::move(runtime_f)};
    }
};


}