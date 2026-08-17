#include "utils/bench.hpp"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <format>
#include <fstream>
#include <numeric>

namespace
{

std::filesystem::path const OUTPUT_DIR {"measurements"};

// One row per run, appended, so a session's configurations end up side by side in the order
// they were tried. The per-run sample files sit next to it for anything the summary flattens.
std::filesystem::path const SUMMARY_FILE = OUTPUT_DIR / "summary.csv";

std::string timestamp()
{
    auto const now = std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now());
    return std::format("{:%Y%m%d-%H%M%S}", now);
}

// Anything that would put a run's label somewhere other than where it says it is.
std::string sanitize(std::string text)
{
    if (text.empty()) {
        return "run";
    }

    for (char& character : text) {
        if ((std::isalnum(static_cast<unsigned char>(character)) == 0) && (character != '-')) {
            character = '_';
        }
    }

    return text;
}

double quantile(std::vector<double> const& sorted, double fraction)
{
    if (sorted.empty()) {
        return 0.0;
    }

    auto const index = static_cast<std::size_t>(fraction * static_cast<double>(sorted.size() - 1));
    return sorted[index];
}

} // namespace

void Bench::start(std::string label, RunConfig config)
{
    label_  = std::move(label);
    config_ = config;

    warmupSeen_ = 0;
    micros_.clear();
    alive_.clear();
    micros_.reserve(static_cast<std::size_t>(sampleSteps_));
    alive_.reserve(static_cast<std::size_t>(sampleSteps_));

    state_ = (warmupSteps_ > 0)? State::WARMUP : State::COLLECTING;
}

void Bench::cancel()
{
    state_ = State::IDLE;
}

void Bench::sample(double micros, std::size_t alive)
{
    switch (state_) {
    case State::IDLE:
        return;

    case State::WARMUP:
        if (++warmupSeen_ >= warmupSteps_) {
            state_ = State::COLLECTING;
        }
        return;

    case State::COLLECTING:
        micros_.push_back(micros);
        alive_.push_back(alive);

        if (micros_.size() >= static_cast<std::size_t>(sampleSteps_)) {
            finish();
        }
        return;
    }
}

void Bench::finish()
{
    state_ = State::IDLE;

    if (micros_.empty()) {
        return;
    }

    RunResult result;
    result.label   = label_;
    result.samples = micros_.size();

    double const total = std::accumulate(micros_.begin(), micros_.end(), 0.0);
    result.meanMicros  = total / static_cast<double>(micros_.size());

    double const variance = std::accumulate(
        micros_.begin(),
        micros_.end(),
        0.0,
        [mean = result.meanMicros] (double acc, double value) {
            double const deviation = value - mean;
            return acc + (deviation * deviation);
        }) / static_cast<double>(micros_.size());

    result.stddevMicros = std::sqrt(variance);

    std::vector<double> sorted = micros_;
    std::sort(sorted.begin(), sorted.end());

    result.medianMicros = quantile(sorted, 0.50);
    result.p95Micros    = quantile(sorted, 0.95);
    result.minMicros    = sorted.front();
    result.maxMicros    = sorted.back();

    auto const [min_alive, max_alive] = std::minmax_element(alive_.begin(), alive_.end());
    result.minAlive                   = *min_alive;
    result.maxAlive                   = *max_alive;

    std::error_code error;
    std::filesystem::create_directories(OUTPUT_DIR, error);

    std::filesystem::path const samples_path = OUTPUT_DIR / std::format(
        "{}_{}_chunk{}_{}_{}.csv",
        timestamp(),
        sanitize(result.label),
        config_.chunkParticles,
        config_.parallel? "par" : "seq",
        config_.pinned? "pinned" : "free");

    {
        std::ofstream samples {samples_path};
        samples << "step,alive,fused_us\n";

        for (std::size_t i = 0; i < micros_.size(); ++i) {
            samples << i << ',' << alive_[i] << ',' << std::format("{:.3f}", micros_[i]) << '\n';
        }
    }

    bool const summary_is_new = !std::filesystem::exists(SUMMARY_FILE, error);

    std::ofstream summary {SUMMARY_FILE, std::ios::app};

    if (summary_is_new) {
        summary << "timestamp,label,chunk,parallel,pinned,threads,samples,"
                   "mean_us,median_us,p95_us,min_us,max_us,stddev_us,min_alive,max_alive\n";
    }

    summary << std::format(
        "{},{},{},{},{},{},{},{:.3f},{:.3f},{:.3f},{:.3f},{:.3f},{:.3f},{},{}\n",
        timestamp(),
        result.label,
        config_.chunkParticles,
        config_.parallel? 1 : 0,
        config_.pinned? 1 : 0,
        config_.threads,
        result.samples,
        result.meanMicros,
        result.medianMicros,
        result.p95Micros,
        result.minMicros,
        result.maxMicros,
        result.stddevMicros,
        result.minAlive,
        result.maxAlive);

    result.path = std::filesystem::absolute(samples_path, error).string();

    info(
        "Bench '{}': {:.1f} us mean over {} steps -> {}",
        result.label,
        result.meanMicros,
        result.samples,
        result.path);

    history_.push_back(std::move(result));
}

void Bench::render(RunConfig const& current)
{
    ImGui::SeparatorText("Measurement");

    static std::array<char, 64> label {"baseline"};
    ImGui::InputText("Label", label.data(), label.size());

    ImGui::SliderInt("Warm-up steps", &warmupSteps_, 0, 1000);
    ImGui::SetItemTooltip("Discarded before anything is kept, so a run measures the steady state and not the change into it");

    ImGui::SliderInt("Sample steps", &sampleSteps_, 10, 5000);

    if (state_ == State::IDLE) {
        if (ImGui::Button("Record run")) {
            start(label.data(), current);
        }
    }
    else {
        float const progress = (state_ == State::WARMUP)
            ? static_cast<float>(warmupSeen_) / static_cast<float>(std::max(warmupSteps_, 1))
            : static_cast<float>(micros_.size()) / static_cast<float>(std::max(sampleSteps_, 1));

        ImGui::ProgressBar(progress, ImVec2 {-1.0F, 0.0F}, (state_ == State::WARMUP)? "warming up" : "recording");

        if (ImGui::Button("Cancel")) {
            cancel();
        }
    }

    if (history_.empty()) {
        return;
    }

    ImGui::SeparatorText("Runs this session");

    if (ImGui::BeginTable("runs", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Run");
        ImGui::TableSetupColumn("Mean");
        ImGui::TableSetupColumn("Median");
        ImGui::TableSetupColumn("p95");
        ImGui::TableSetupColumn("vs first");
        ImGui::TableHeadersRow();

        double const baseline = history_.front().meanMicros;

        for (RunResult const& run : history_) {
            ImGui::TableNextRow();

            ImGui::TableNextColumn();
            ImGui::TextUnformatted(run.label.c_str());
            ImGui::SetItemTooltip("%s", run.path.c_str());

            ImGui::TableNextColumn();
            ImGui::Text("%.1f us", run.meanMicros);

            ImGui::TableNextColumn();
            ImGui::Text("%.1f us", run.medianMicros);

            ImGui::TableNextColumn();
            ImGui::Text("%.1f us", run.p95Micros);

            ImGui::TableNextColumn();
            ImGui::Text("%.2fx", baseline / run.meanMicros);
        }

        ImGui::EndTable();
    }
}
