#ifndef YARR_LOGIC_KNOB_HPP
#define YARR_LOGIC_KNOB_HPP


#include "utils/rng.hpp"

#include <imgui.h>

#include <cmath>
#include <algorithm>
#include <cstddef>
#include <span>
#include <type_traits>


// Whether a knob takes part in "Randomise". Off for anything that picks a mode rather than
// shapes a look: rolling the dice on "Update" or on a particle budget does not produce a
// different scene, it produces a broken one.
enum class Randomise : bool
{
    No,
    Yes,
};


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
            static_assert(false, "no ImGuiDataType for this knob type"); // fine in C++26
        }
    } ();


// A per-instance widget identity. ImGui derives a widget's identity from its label, so two
// knobs sharing a label — the "Strength" of two attractors — would otherwise be the same
// widget, and dragging one would move the other. Copies take a fresh id rather than the
// source's, so duplicating a force copies its tuning without copying its widget identity.
class WidgetId
{
public:
    WidgetId() noexcept
        : value_{++counter_} {}

    WidgetId([[maybe_unused]] WidgetId const& other) noexcept
        : value_{++counter_} {}
    WidgetId([[maybe_unused]] WidgetId&& other) noexcept
        : value_{++counter_} {}

    // Identity is a property of the instance, not of the value assigned into it, so an
    // assigned-to knob keeps the id it was born with.
    WidgetId& operator =([[maybe_unused]] WidgetId const& other) noexcept { return *this; }
    WidgetId& operator =([[maybe_unused]] WidgetId&& other) noexcept      { return *this; }

    ~WidgetId() = default;

    [[nodiscard]] int get() const noexcept { return value_; }

private:
    inline static int counter_ {0};

    int value_ {0};
};


// The type-erased face of a knob, so a panel can be drawn, randomised or saved by walking a
// list without knowing what each entry holds. Everything the simulation reads goes through
// the non-virtual get() on the derived type instead, which inlines to a load.
class KnobBase
{
public:
    KnobBase()                            = default;
    KnobBase(KnobBase const&)             = default;
    KnobBase(KnobBase&&)                  = default;
    KnobBase& operator =(KnobBase const&) = default;
    KnobBase& operator =(KnobBase&&)      = default;
    virtual ~KnobBase()                   = default;

    virtual bool                      render()              = 0;
    virtual void                      randomise(RNG& rng)   = 0;
    [[nodiscard]] virtual char const* name() const noexcept = 0;
};


template <typename T>
class Knob final : public KnobBase
{
public:
    // Not constexpr: the id counter below is mutable state, so a constant-evaluated knob
    // could not exist anyway.
    Knob(char const*     name,
        T                value,
        T                low,
        T                high,
        char const*      format,
        ImGuiSliderFlags flags     = ImGuiSliderFlags_AlwaysClamp,
        Randomise        randomise = Randomise::Yes) noexcept
        : name_{name},
          format_{format},
          value_{value},
          default_{value},
          low_{low},
          high_{high},
          flags_{flags},
          randomise_{randomise}
    {}

    bool render() override
    {
        ImGui::PushID(id_.get());

        bool changed = ImGui::SliderScalar(name_, kDataType<T>, &value_, &low_, &high_, format_, flags_);

        // Right-click to put a knob back where it started. This is what makes a wide
        // exploratory range safe to drag through — there is always a way back.
        if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
            value_  = default_;
            changed = true;
        }

        ImGui::PopID();

        return changed;
    }

    void randomise(RNG& rng) override
    {
        if (randomise_ == Randomise::No) {
            return;
        }

        if constexpr (std::is_floating_point_v<T>) {
            // Logarithmic knobs are drawn in log space, or every roll lands in the top
            // decade and the button only ever finds one look. Guarded on a positive floor,
            // since log(0) is not a bound.
            if (((flags_ & ImGuiSliderFlags_Logarithmic) != 0) && (low_ > T {0})) {
                value_ = std::exp(rng.range(std::log(low_), std::log(high_)));
            }
            else {
                value_ = rng.range(low_, high_);
            }
        }
        else {
            value_ = static_cast<T>(rng.range(static_cast<int>(low_), static_cast<int>(high_)));
        }
    }

    [[nodiscard]] char const* name() const noexcept override { return name_; }

    [[nodiscard]] T   get() const noexcept { return value_; }
    [[nodiscard]] int id() const noexcept  { return id_.get(); }

    // Clamped, because the bounds are the knob's promise to everything downstream and a
    // value arriving from a preset file has not been through a slider.
    void set(T value) noexcept { value_ = std::clamp(value, low_, high_); }

private:
    WidgetId id_;

    char const* name_;
    char const* format_;

    T value_ {};
    T default_ {};

    T low_ {};
    T high_ {};

    ImGuiSliderFlags flags_;
    Randomise        randomise_;
};


// A checkbox is a knob in every way that this abstraction cares about: it is named, it has a
// default, it belongs in a preset, and a panel wants to draw it in the same pass as the
// sliders. It is only the *widget* that differs, so it shares the base and specialises the
// parts that would be meaningless — there are no bounds to clamp to, no format to print
// with, and no log scale to draw in.
template <>
class Knob<bool> final : public KnobBase
{
public:
    Knob(char const* name, bool value, Randomise randomise = Randomise::No) noexcept
        : name_{name},
          value_{value},
          default_{value},
          randomise_{randomise}
    {}

    bool render() override
    {
        ImGui::PushID(id_.get());

        bool changed = ImGui::Checkbox(name_, &value_);

        if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
            value_  = default_;
            changed = true;
        }

        ImGui::PopID();

        return changed;
    }

    // Defaults to opting out: most flags in this project select a mode. A toggle that is
    // genuinely part of a look — an inversion, a wrap — can opt back in at the call site.
    void randomise(RNG& rng) override
    {
        if (randomise_ == Randomise::Yes) {
            value_ = rng.chance();
        }
    }

    [[nodiscard]] char const* name() const noexcept override { return name_; }

    [[nodiscard]] bool get() const noexcept { return value_; }
    [[nodiscard]] int  id() const noexcept  { return id_.get(); }

    void set(bool value) noexcept { value_ = value; }

private:
    WidgetId id_;

    char const* name_;

    bool value_ {};
    bool default_ {};

    Randomise randomise_;
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

inline void randomiseKnobs(std::span<KnobBase* const> knobs, RNG& rng)
{
    for (KnobBase* knob : knobs) {
        knob->randomise(rng);
    }
}

#endif // YARR_LOGIC_KNOB_HPP
