#include <assert.h>

#include <bitset>
#include <cstdint>
#include <functional>
#include <iostream>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

template <typename T, typename... Ts>
struct Index;
template <typename T, typename... Ts>
struct Index<T, T, Ts...> : std::integral_constant<std::size_t, 0> {};
template <typename T, typename U, typename... Ts>
struct Index<T, U, Ts...> : std::integral_constant<std::size_t, 1 + Index<T, Ts...>::value> {};

/////////////////////////////////////////////////////////////

class Entity {
public:
    using Id = uint16_t;
    static Entity spawn() { return {counter_++}; }
    static Entity spawn_with(Id id) {
        counter_ = id + 1;
        return {id};
    }

    const Id& id() const { return id_; }
    size_t operator()(const Entity& e) const { return e.id(); }
    bool operator==(const Entity& rhs) const { return id() == rhs.id(); }

private:
    inline static Id counter_ = 0;
    Entity(Id id) : id_(id) {}
    Id id_;
};

/////////////////////////////////////////////////////////////

template <typename... Event>
class EventManager {
private:
    template <typename E>
    using Handler = std::function<void(const E&)>;

public:
    template <typename E>
    void add_handler(Handler<E> handler) {
        std::get<kIndexOf<E>>(handlers_).push_back(std::move(handler));
    }

    template <typename E>
    void trigger(const E& event) {
        for (auto& handler : std::get<kIndexOf<E>>(handlers_)) {
            handler(event);
        }
    }

private:
    template <typename E>
    static constexpr size_t kIndexOf = Index<E, Event...>::value;

private:
    std::tuple<std::vector<Handler<Event>>...> handlers_;
};

/////////////////////////////////////////////////////////////

template <typename... T>
struct Components {};
template <typename... T>
struct Events {};

/////////////////////////////////////////////////////////////
template <typename Components, typename Events>
class Manager;

template <typename... Component, typename... Event>
class Manager<Components<Component...>, Events<Event...>> {
public:
    using ComponentVariant = std::variant<Component...>;
    static constexpr size_t kNumComponents = sizeof...(Component);

    struct Spawn {
        const Entity& entity;
    };
    struct Despawn {
        const Entity& entity;
    };

public:
    template <typename... C>
    Entity spawn_with(C... components) {
        const auto& [entity, active] = entities_.emplace_back(EntitiyHolder{Entity::spawn(), bitset_of<C...>()});

        auto& index = index_[entity.id()];
        ((index[kIndexOf<C>] = std::get<kIndexOf<C>>(components_).size()), ...);
        (std::get<kIndexOf<C>>(components_).emplace_back(components), ...);

        events().trigger(Spawn{entity});

        return entity;
    }

    template <typename C>
    C* get_component(const Entity& entity) {
        const auto& index = index_[entity.id()][kIndexOf<C>];
        return index == kInvalidIndex ? nullptr : &std::get<kIndexOf<C>>(components_).at(index);
    }

    void despawn(const Entity& entity) {
        assert(!entities_.empty());

        auto it = std::find_if(entities_.begin(), entities_.end(), [&entity](const auto& enitiy_holder) {
            return enitiy_holder.entity.id() == entity.id();
        });
        assert(it != entities_.end());

        std::iter_swap(it, std::prev(entities_.end()));
        entities_.pop_back();

        events().trigger(Despawn{entity});

        // Remove components AFTER triggering the event, since handling the event may need component data
        auto index_it = index_.find(entity.id());
        ArrayProxy index = index_it->second;
        index_.erase(index_it);

        (remove_component_at<Component>(index[kIndexOf<Component>]), ...);
    }

    template <typename C0, typename... ReqComponent, typename F>
    void run_system(const F& f) {
        auto target = bitset_of<C0, ReqComponent...>();
        for (const auto& [entity, active] : entities_) {
            if ((target & active) != target) {
                continue;
            }
            run_system_on_entity<C0, ReqComponent...>(f, entity);
        }
    }

    auto& events() { return events_; };

private:
    template <typename... C>
    static constexpr std::bitset<kNumComponents> bitset_of() {
        std::bitset<kNumComponents> result{0};
        (result.set(kIndexOf<C>), ...);
        return result;
    }

    template <typename C>
    static constexpr size_t kIndexOf = Index<C, Component...>::value;

    template <typename... ReqComponent, typename F>
    void run_system_on_entity(const F& f, const Entity& entity) {
        const auto& index = index_[entity.id()];
        std::tuple<ReqComponent&...> components{
            std::get<kIndexOf<ReqComponent>>(components_)[index[kIndexOf<ReqComponent>]]...};

        std::apply(f, std::tuple_cat(std::tuple{entity}, components));
    }

    template <typename C>
    void remove_component_at(size_t component_index) {
        constexpr size_t I = kIndexOf<C>;
        auto& vector = std::get<I>(components_);

        assert(vector.size() != 0);
        const size_t end_index = vector.size() - 1;
        std::swap(vector[component_index], vector[end_index]);
        vector.pop_back();

        // Make sure to fix up any "dangling pointers" after we removed the component index
        for (auto& [_, index] : index_)
            if (index[I] == end_index) index[I] = component_index;
    }

private:
    std::tuple<std::vector<Component>...> components_;

    static constexpr size_t kInvalidIndex = -1;
    struct ArrayProxy : public std::array<size_t, kNumComponents> {
        ArrayProxy() { this->fill(kInvalidIndex); }
    };
    std::unordered_map<Entity::Id, ArrayProxy> index_;

    struct EntitiyHolder {
        Entity entity;
        std::bitset<kNumComponents> active;
    };
    std::vector<EntitiyHolder> entities_;

    EventManager<Spawn, Despawn, Event...> events_;
};
