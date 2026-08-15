#ifndef YARR_LOGIC_KNOB_HPP
#define YARR_LOGIC_KNOB_HPP


#include <imgui.h>
#include <nlohmann/json.hpp>

#include <cmath>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <type_traits>
#include <utility>


template <typename T>
constexpr ImGuiDataType kDataType = [] {
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
    explicit KnobBase(char const* name) noexcept
        : name_{name} {}
    virtual ~KnobBase() = default;

    virtual bool render() = 0;

    [[nodiscard]] virtual nlohmann::json serialize() const                     = 0;
    virtual void                         deserialize(nlohmann::json const& in) = 0;

    [[nodiscard]] char const* name() const noexcept { return name_; }
    [[nodiscard]] int         id() const noexcept   { return id_.get(); }

protected:
    KnobBase(KnobBase const&)             = default;
    KnobBase(KnobBase&&)                  = default;
    KnobBase& operator =(KnobBase const&) = default;
    KnobBase& operator =(KnobBase&&)      = default;

private:
    WidgetId    id_;
    char const* name_;
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
        ImGuiSliderFlags flags = ImGuiSliderFlags_AlwaysClamp) noexcept
        : KnobBase{name},
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

        bool changed = ImGui::SliderScalar(name(), kDataType<T>, &value_, &low_, &high_, format_, flags_);

        if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
            value_  = default_;
            changed = true;
        }

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
    Knob(char const* name, bool value) noexcept
        : KnobBase{name},
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

    void set(bool value) noexcept { value_ = value; }

private:
    bool value_ {};
    bool default_ {};
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

#endif // YARR_LOGIC_KNOB_HPP
