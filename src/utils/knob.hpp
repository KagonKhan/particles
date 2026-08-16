#ifndef YARR_UTILS_KNOB_HPP
#define YARR_UTILS_KNOB_HPP


#include <imgui.h>
#include <nlohmann/json.hpp>

#include <cmath>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>


template <typename T>
constexpr ImGuiDataType DATA_TYPE = [] {
        if constexpr (std::is_same_v<T, float>) {
            return ImGuiDataType_Float;
        }
        else if constexpr (std::is_same_v<T, int>) {
            return ImGuiDataType_S32;
        }
        else if constexpr (std::is_same_v<T, std::size_t>) {
            return ImGuiDataType_U64;
        }
        else {
            static_assert(false, "no ImGuiDataType for this knob type");
        }
    } ();


class WidgetId
{
public:
    WidgetId() noexcept
        : value_{++counter_} {}
    ~WidgetId() = default;


    WidgetId([[maybe_unused]] WidgetId const& other) noexcept
        : value_{++counter_} {}
    WidgetId([[maybe_unused]] WidgetId&& other) noexcept
        : value_{++counter_} {}

    WidgetId& operator =([[maybe_unused]] WidgetId const& other) noexcept { return *this; }
    WidgetId& operator =([[maybe_unused]] WidgetId&& other) noexcept      { return *this; }

    [[nodiscard]] int get() const noexcept { return value_; }

private:
    inline static int counter_ {0};

    int value_ {0};
};


class KnobBase
{
public:
    KnobBase(char const* name, std::string tooltip) noexcept
        : name_{name},
          tooltip_{std::move(tooltip)} {}
    virtual ~KnobBase() = default;

    virtual bool render() = 0;

    [[nodiscard]] virtual nlohmann::json serialize() const                     = 0;
    virtual void                         deserialize(nlohmann::json const& in) = 0;

    [[nodiscard]] char const*        name() const noexcept    { return name_; }
    [[nodiscard]] std::string const& tooltip() const noexcept { return tooltip_; }
    [[nodiscard]] int                id() const noexcept      { return id_.get(); }

protected:
    KnobBase(KnobBase const&)             = default;
    KnobBase(KnobBase&&)                  = default;
    KnobBase& operator =(KnobBase const&) = default;
    KnobBase& operator =(KnobBase&&)      = default;

    // Called right after the widget, while it is still the current item.
    void renderTooltip() const
    {
        if (!tooltip_.empty()) {
            ImGui::SetItemTooltip("%s", tooltip_.c_str());
        }
    }

private:
    WidgetId    id_;
    char const* name_;
    std::string tooltip_;
};


// TODO: possibly KnobSpec for default knob limits
template <typename T>
class Knob final : public KnobBase
{
public:
    Knob(char const*     name,
        T                value,
        T                low,
        T                high,
        char const*      format,
        std::string      tooltip = {},
        ImGuiSliderFlags flags   = ImGuiSliderFlags_AlwaysClamp) noexcept
        : KnobBase{name, std::move(tooltip)},
          format_{format},
          value_{value},
          default_{value},
          low_{low},
          high_{high},
          flags_{flags}
    {}

    bool render() override
    {
        ImGui::PushID(id());

        bool changed = ImGui::SliderScalar(name(), DATA_TYPE<T>, &value_, &low_, &high_, format_, flags_);

        if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
            value_  = default_;
            changed = true;
        }

        renderTooltip();

        ImGui::PopID();

        return changed;
    }

    [[nodiscard]] nlohmann::json serialize() const override { return value_; }

    void deserialize(nlohmann::json const& in) override
    {
        if constexpr (std::is_floating_point_v<T>) {
            if (in.is_number()) {
                set(static_cast<T>(in.get<double>()));
            }
        }
        else {
            if (!in.is_number_integer()) {
                return;
            }

            auto const value = in.get<std::int64_t>();

            if (std::cmp_less(value, low_)) { set(low_); }
            else if (std::cmp_greater(value, high_)) { set(high_); }
            else { set(static_cast<T>(value)); }
        }
    }

    [[nodiscard]] T get() const noexcept { return value_; }

    // Clamped, because the bounds are the knob's promise to everything downstream and a
    // value arriving from a preset file has not been through a slider.
    void set(T value) noexcept { value_ = std::clamp(value, low_, high_); }

private:
    char const* format_;

    T value_ {};
    T default_ {};

    T low_ {};
    T high_ {};

    ImGuiSliderFlags flags_;
};


template <>
class Knob<bool> final : public KnobBase
{
public:
    Knob(char const* name, bool value, std::string tooltip = {}) noexcept
        : KnobBase{name, std::move(tooltip)},
          value_{value},
          default_{value}
    {}

    bool render() override
    {
        ImGui::PushID(id());

        bool changed = ImGui::Checkbox(name(), &value_);

        if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
            value_  = default_;
            changed = true;
        }

        renderTooltip();

        ImGui::PopID();

        return changed;
    }

    [[nodiscard]] nlohmann::json serialize() const override { return value_; }

    void deserialize(nlohmann::json const& in) override
    {
        if (in.is_boolean()) {
            set(in.get<bool>());
        }
    }

    [[nodiscard]] bool get() const noexcept { return value_; }

    // For the widgets that write the value themselves, such as a window's close button.
    [[nodiscard]] bool* address() noexcept { return &value_; }

    void set(bool value) noexcept { value_ = value; }

private:
    bool value_ {};
    bool default_ {};
};


// A choice out of a fixed list of names, rendered as a combo. The options are borrowed —
// they are meant to be a static table, like SHAPE_NAMES, that outlives the knob.
template <>
class Knob<char const*> final : public KnobBase
{
public:
    Knob(char const*                 name,
        std::span<char const* const> options,
        int                          value   = 0,
        std::string                  tooltip = {}) noexcept
        : KnobBase{name, std::move(tooltip)},
          options_{options},
          value_{clampIndex(value)},
          default_{value_}
    {}

    bool render() override
    {
        if (options_.empty()) {
            return false;
        }

        ImGui::PushID(id());

        bool changed = ImGui::Combo(name(), &value_, options_.data(), static_cast<int>(options_.size()));

        if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
            value_  = default_;
            changed = true;
        }

        renderTooltip();

        ImGui::PopID();

        return changed;
    }

    // By name rather than by index, so reordering or extending the list does not silently
    // repoint an existing preset at a different option.
    [[nodiscard]] nlohmann::json serialize() const override { return get(); }

    void deserialize(nlohmann::json const& in) override
    {
        if (in.is_string()) {
            set(in.get_ref<nlohmann::json::string_t const&>());
        }
    }

    [[nodiscard]] char const* get() const noexcept
    {
        return options_.empty()? "" : options_[static_cast<std::size_t>(value_)];
    }

    [[nodiscard]] int index() const noexcept { return value_; }

    [[nodiscard]] std::span<char const* const> options() const noexcept { return options_; }

    // An unknown name leaves the selection alone, as a value of the wrong type would.
    void set(std::string_view value) noexcept
    {
        for (std::size_t i = 0; i < options_.size(); ++i) {
            if (value == options_[i]) {
                value_ = static_cast<int>(i);
                return;
            }
        }
    }

    void setIndex(int value) noexcept { value_ = clampIndex(value); }

private:
    [[nodiscard]] int clampIndex(int value) const noexcept
    {
        return options_.empty()? 0 : std::clamp(value, 0, static_cast<int>(options_.size()) - 1);
    }

    std::span<char const* const> options_;

    int value_ {};
    int default_ {};
};


inline bool renderKnobs(std::span<KnobBase* const> knobs)
{
    bool changed = false;

    for (KnobBase* knob : knobs) {
        if (knob->render()) {
            changed = true;
        }
    }

    return changed;
}

inline nlohmann::json serializeKnobs(std::span<KnobBase* const> knobs)
{
    nlohmann::json serialized = nlohmann::json::object();

    for (KnobBase const* knob : knobs) {
        serialized[knob->name()] = knob->serialize();
    }

    return serialized;
}

// Absent keys and values of the wrong type leave the knob at whatever it already holds.
inline void deserializeKnobs(std::span<KnobBase* const> knobs, nlohmann::json const& serialized)
{
    if (!serialized.is_object()) {
        return;
    }

    for (KnobBase* knob : knobs) {
        if (auto const entry = serialized.find(knob->name()); (entry != serialized.end())) {
            knob->deserialize(*entry);
        }
    }
}

#endif // YARR_UTILS_KNOB_HPP
