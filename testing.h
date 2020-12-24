#include <iostream>
#include <entt/entt.hpp>
#include <functional>
#include <limits>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <memory>





class ECS_runtime_view {
    template<class ScriptingObj>
    friend class ECS_ManagerProxy;

    entt::runtime_view rv;
    using sparse_set_handle = std::function<entt::sparse_set*(void)>;
    std::vector<sparse_set_handle> scripting_view;
    std::vector<sparse_set_handle> scripting_filter;

    using underlying_iterator = typename entt::runtime_view::iterator;
    
    class view_iterator {
        entt::runtime_view& rv;
        underlying_iterator it;
        std::vector<sparse_set_handle>& scripting_view;
        std::vector<sparse_set_handle>& scripting_filter;
        
        bool scripting_view_checks(const entt::entity entt) const {
            bool ok = true;
            for (auto vsp : scripting_view) {
                ok = ok && vsp()->contains(entt);
            }
            for (auto fsp : scripting_filter) {
                ok = ok && !fsp()->contains(entt);
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
            std::vector<sparse_set_handle>& cpool, std::vector<sparse_set_handle>& fpool)
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

    ECS_runtime_view(entt::runtime_view rv, std::vector<sparse_set_handle> cpool = {},
        std::vector<sparse_set_handle> cfilter = {}) : rv(rv), scripting_view(cpool),
        scripting_filter(cfilter) {}

public:

    view_iterator begin() {
        return view_iterator(rv, rv.begin(), scripting_view, scripting_filter);
    }
    view_iterator end() {
        return view_iterator(rv, rv.end(), scripting_view, scripting_filter);
    }
};





/**
 * @brief Manages an ECS Context.
 * 
 */
class ECS_Manager {
    template<class T>
    friend class ECS_ManagerProxy;

    entt::registry reg;
    std::uint32_t num_proxies = 0;

    struct runtime_index {
        std::uint32_t owner_proxy_id;
        std::uint32_t component_id;
    };

    std::unordered_map<std::string, runtime_index> runtime;
    std::unordered_map<std::string, entt::id_type> internal;

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

    template<class C, class... Args>
    C& emplace(const entt::entity entt, Args&&... args) {
        return reg.emplace<C>(entt, args...);
    }

    template<class C, class... Args>
    C& emplace_or_replace(const entt::entity entt, Args&&... args) {
        return reg.emplace_or_replace<C>(entt, args...);
    }

    template<class C>
    void remove(entt::entity entt) {

    }

    template<class ...F, class... C>
    entt::view<entt::exclude_t<F...>, C...> view(const entt::exclude_t<F...> exclude = {}) const {
        return reg.view<C...>(exclude);
    }
};





/**
 * @brief Proxy to ECS_Manager, to be used from a scripting language.
 * 
 * @tparam ScriptingObj Data type or handler of the scripting language
 * component type.
 */
template<class ScriptingObj>
class ECS_ManagerProxy {
    std::shared_ptr<ECS_Manager> manager;
    std::uint32_t proxy_id;
    using storage = entt::storage<ScriptingObj>;
    std::vector<storage> runtime_components;
    

    void remove_all_runtime(const entt::entity entt) {
        for (auto& ss : runtime_components) {
            if (ss.contains(entt))
                ss.remove(entt);
        }
    }

public:
    ECS_ManagerProxy(std::shared_ptr<ECS_Manager> manager) : manager{manager} {
        if (!manager) {
            throw std::invalid_argument("ECS Manager pointer shall be dereferencable");
        }
        proxy_id = manager->num_proxies++;
    }

    entt::entity create() {
        return manager->reg.create();
    }

    // WIP
    void destroy(const entt::entity entt) {
        remove_all_runtime(entt);
        manager->reg.destroy(entt);
    }

    ScriptingObj& set(const entt::entity entt, const std::string& component_name,
        const ScriptingObj& component_handle) {
        std::uint32_t component_id = runtime_components.size();
        auto r = manager->runtime.emplace(component_name, ECS_Manager::runtime_index{proxy_id, component_id});
        component_id = r.first->second.component_id;
        
        if (r.second) {
            auto& s = runtime_components.emplace_back();
            return s.emplace(entt, component_handle);
        }
        // storage emplace its UB if entity already has the component
        // and get its UB if entity doesn't have the entity, so check required.
        auto& components = runtime_components[component_id];
        if (components.contains(entt)) {
            auto& tmp = components.get(entt);
            tmp = component_handle;
            return tmp;
        }
        return components.emplace(entt, component_handle);
    }

    void remove(entt::entity entt, const std::string& component) {

    }
    //allocation to runtime_components may invaldate view! find solution
    ECS_runtime_view view(const std::vector<std::string>& components,
            const std::vector<std::string>& filters = {}) {
        std::vector<entt::id_type> internal_c;
        internal_c.reserve(components.size());
        std::vector<entt::id_type> internal_f;
        internal_f.reserve(filters.size());
        std::vector<ECS_runtime_view::sparse_set_handle> runtime_c;
        runtime_c.reserve(components.size());
        std::vector<ECS_runtime_view::sparse_set_handle> runtime_f;
        runtime_f.reserve(filters.size());
        for (const auto& comp : components) {
            if (auto it = manager->runtime.find(comp); it != manager->runtime.end()
                && it->second.owner_proxy_id == proxy_id) {
                    auto index = it->second.component_id;
                    runtime_c.push_back([this, index](){return &runtime_components[index];});
                    //runtime_c.push_back(&runtime_components[it->second.component_id]);
            }
            else if (auto it = manager->internal.find(comp); it != manager->internal.end()) {
                internal_c.push_back(it->second);
            }
            else {
                // workaround to return an empty view
                auto endit = internal_c.end();
                return ECS_runtime_view(manager->reg.runtime_view(endit, endit));
            }
        }
        for (const auto& filt : filters) {
            if (auto it = manager->runtime.find(filt); it != manager->runtime.end()
                && it->second.owner_proxy_id == proxy_id) {
                    auto index = it->second.component_id;
                    runtime_f.push_back([this, index](){return &runtime_components[index];});
                    //runtime_f.push_back(&runtime_components[it->second.component_id]);
            }
            else if (auto it = manager->internal.find(filt); it != manager->internal.end()) {
                internal_f.push_back(it->second);
            }
        }
        auto rv = manager->reg.runtime_view(internal_c.begin(), internal_c.end(), internal_f.begin(), internal_f.end());
        return ECS_runtime_view{rv, std::move(runtime_c), std::move(runtime_f)};
    }
};


