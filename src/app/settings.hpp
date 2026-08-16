#ifndef YARR_APP_SETTINGS_HPP
#define YARR_APP_SETTINGS_HPP

#include "app/exceptions.hpp"
#include "logic/knob.hpp"
#include "utils/bases.hpp"

#include <memory>
#include <utility>
#include <vector>


// The window's menu bar, and with it the one place an option can be reached from without an
// owner having to pass it around. A component registers what it owns under a menu and a
// name; anything else that needs the value asks for the same pair and is handed the same
// knob, whichever of the two happened to run first.
//
// Menu and option names are borrowed rather than copied, exactly as a knob's name is, so
// they have to be string literals.
class Settings : public Singleton<Settings>
{
    friend class Singleton<Settings>;

public:
    // Get-or-create. The first caller's arguments fix the type, the range and the default;
    // a later caller passing the same ones gets the knob that is already there. The
    // reference stays good for the rest of the run — the knobs are heap nodes, so nothing
    // registered afterwards moves them.
    template <typename T, typename... Args>
    Knob<T>& option(char const* menu, char const* name, Args&&... args)
    {
        if (Entry* const existing = findEntry(menu, name)) {
            auto* const typed = dynamic_cast<Knob<T>*>(existing->knob.get());

            if (typed == nullptr) {
                throw InitializationError("setting \"{}/{}\" is already registered as another type", menu, name);
            }

            return *typed;
        }

        auto     knob = std::make_unique<Knob<T>>(name, std::forward<Args>(args)...);
        Knob<T>& out  = *knob;

        entriesOf(menu).push_back(Entry {std::move(knob), nullptr});

        return out;
    }

    // The knob if that name is registered with that type, null otherwise. For a reader with
    // no business deciding the option's range or default — and which therefore has to cope
    // with running before whoever does.
    template <typename T>
    [[nodiscard]] Knob<T>* peek(char const* menu, char const* name)
    {
        Entry* const entry = findEntry(menu, name);
        return (entry == nullptr)? nullptr : dynamic_cast<Knob<T>*>(entry->knob.get());
    }

    void describe(char const* menu, char const* name, char const* tooltip);

    ///@brief Draws the bar. Belongs at the top of the frame, before anything that reads an option.
    void render();

private:
    Settings() = default;

    struct Entry
    {
        std::unique_ptr<KnobBase> knob;
        char const*               tooltip;
    };

    struct Menu
    {
        char const*        name;
        std::vector<Entry> entries;
    };

    [[nodiscard]] Entry*              findEntry(char const* menu, char const* name);
    [[nodiscard]] std::vector<Entry>& entriesOf(char const* menu);

    // In registration order, which is construction order: the components built earliest sit
    // leftmost in the bar.
    std::vector<Menu> menus_;
};

#endif // YARR_APP_SETTINGS_HPP
