#ifndef YARR_APP_SETTINGS_HPP
#define YARR_APP_SETTINGS_HPP

#include "exceptions.hpp"
#include "utils/bases.hpp"
#include "utils/knob.hpp"

#include <memory>
#include <string_view>
#include <utility>
#include <vector>


///@brief App menu bar
class Settings : public Singleton<Settings>
{
    friend class Singleton<Settings>;

public:
    ///@brief creates or returns knobs
    template <typename T, typename ... Args>
    Knob<T>& option(char const* menu, char const* name, Args&&... args)
    {
        if (KnobBase* existing = findKnob(menu, name)) {
            auto* const typed = dynamic_cast<Knob<T>*>(existing);

            if (typed == nullptr) {
                throw InitializationError("setting \"{}/{}\" is already registered as another type", menu, name);
            }

            return *typed;
        }

        auto     knob = std::make_unique<Knob<T>>(name, std::forward<Args>(args)...);
        Knob<T>& out  = *knob;

        for (Menu& candidate : menus_) {
            if (std::string_view {candidate.name} == menu) {
                candidate.knobs.push_back(std::move(knob));
                return out;
            }
        }

        menus_.push_back(Menu {.name = menu, .knobs = {}});
        menus_.back().knobs.push_back(std::move(knob));

        return out;
    }

    template <typename T>
    [[nodiscard]] Knob<T>* peek(char const* menu, char const* name)
    {
        KnobBase* knob = findKnob(menu, name);
        return (knob == nullptr)? nullptr : dynamic_cast<Knob<T>*>(knob);
    }

    ///@brief Draws the bar. Belongs at the top of the frame, before anything that reads an option.
    void render();

private:
    Settings() = default;

    struct Menu
    {
        char const* name;
        std::vector<std::unique_ptr<KnobBase>> knobs;
    };

    std::vector<Menu> menus_;

    [[nodiscard]] KnobBase* findKnob(char const* menu, char const* name);
};

#endif // YARR_APP_SETTINGS_HPP
