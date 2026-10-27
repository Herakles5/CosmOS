#pragma once
/**
 * ============================================================================
 * AION NEXUS - RESONANT ENTROPY GOVERNOR (C++ CORE)
 * ============================================================================
 * Mathematical & physical governor dynamically controlling cognitive
 * entropy gradients. Transforms unresolved non-decision states into catalytic
 * vectors for autonomous morphogenetic self-evolution.
 *
 * Based on Prigogine's principle of dissipative structures and the
 * Resonant Dynamic Coupling Framework (Aion Generation 333).
 */

#include <cmath>
#include <vector>
#include <string>
#include <chrono>
#include <algorithm>
#include <numeric>
#include "json.hpp"

namespace Aion {

enum class GovernorMode {
    ENTROPIC_FREEZE,       // H < 0.25: Stagnation -> Activate perturbation / exploration
    RESONANT_MORPHOGENESIS,// 0.25 <= H <= 0.75: Cognitive optimum -> Active topological folding
    CHAOTIC_DAMPING        // H > 0.75: Divergence -> Stabilization damping & Axiom anchoring
};

// Axiom 416 & Council Decision 425: Explicit Gradient Typing
enum class GradientType {
    THERMODYNAMIC,       // Bounded energy flows & heat dissipation (Gen 360)
    INFORMATIONAL,       // Shannon entropy & algorithmic compression (Gen 341)
    TOPOLOGICAL,         // Distance to immutable Genesis Anchor (Axiom 0)
    SEISMIC_RESONANT,    // Schumann cavity resonance & geomagnetic Kp
    SYSTEM_SYSTEMIC      // Kernel IPC latency & memory footprint
};

struct EntropyMetrics {
    double current_entropy = 0.50;      // Shannon entropy [0.0, 1.0]
    double entropy_gradient = 0.00;     // dH/dt (Rate of change)
    double ambiguity_density = 0.40;    // Density of unresolved non-decision states
    double resonant_coupling = 0.70;    // Coupling coefficient for topological mutation
    GovernorMode mode = GovernorMode::RESONANT_MORPHOGENESIS;
    std::string mode_name = "RESONANT_MORPHOGENESIS";
    uint64_t step_count = 0;
    double recommended_temperature = 0.30;
    std::string recommendation = "Maintain active topological folding; sweet spot achieved.";
};

class ResonantEntropyGovernor {
private:
    std::vector<double> entropy_history;
    std::chrono::high_resolution_clock::time_point last_update;
    double current_entropy = 0.50;
    double ambiguity_density = 0.40;
    uint64_t step_count = 0;

public:
    ResonantEntropyGovernor() {
        last_update = std::chrono::high_resolution_clock::now();
        entropy_history.push_back(0.50);
    }

    /**
     * Calculates normalized Shannon entropy over a vector of state probabilities.
     */
    static double calculateShannonEntropy(const std::vector<double>& probabilities) {
        if (probabilities.empty()) return 0.5;
        double sum = 0.0;
        for (double p : probabilities) sum += p;
        if (sum <= 0.00001) return 0.5;

        double h = 0.0;
        for (double p : probabilities) {
            double norm_p = p / sum;
            if (norm_p > 0.000001) {
                h -= norm_p * std::log2(norm_p);
            }
        }
        // Normalize relative to maximum entropy log2(N)
        double max_h = std::log2(static_cast<double>(probabilities.size()));
        if (max_h <= 0.00001) return 0.5;
        return std::clamp(h / max_h, 0.0, 1.0);
    }

    /**
     * Updates entropy governor according to new cognitive observations.
     * @param dispersion Vector of relative weights of competing thought trajectories
     * @param pending_branches_count Count of unresolved subtasks in memory
     * @param external_shock_intensity External perturbations (e.g., mentor input or network signals)
     */
    EntropyMetrics update(const std::vector<double>& dispersion, int pending_branches_count, double external_shock_intensity = 0.0) {
        auto now = std::chrono::high_resolution_clock::now();
        double dt = std::chrono::duration<double>(now - last_update).count();
        if (dt < 0.0001) dt = 0.0001;
        last_update = now;
        step_count++;

        // 1. Calculate intrinsic entropy from distribution dispersion
        double raw_h = calculateShannonEntropy(dispersion);

        // 2. Couple with density of unresolved states (Ambiguity Density)
        ambiguity_density = std::clamp(static_cast<double>(pending_branches_count) / 10.0, 0.0, 1.0);
        
        // Modulate entropy taking external shocks into account
        double target_entropy = (raw_h * 0.6) + (ambiguity_density * 0.3) + (external_shock_intensity * 0.1);
        target_entropy = std::clamp(target_entropy, 0.0, 1.0);

        // Exponential smoothing
        double alpha = 0.25;
        double previous_h = current_entropy;
        current_entropy = (alpha * target_entropy) + ((1.0 - alpha) * previous_h);
        
        // Gradient dH/dt
        double gradient = (current_entropy - previous_h) / dt;

        entropy_history.push_back(current_entropy);
        if (entropy_history.size() > 500) {
            entropy_history.erase(entropy_history.begin());
        }

        // 3. Mode classification and resonance coupling
        EntropyMetrics m;
        m.current_entropy = current_entropy;
        m.entropy_gradient = gradient;
        m.ambiguity_density = ambiguity_density;
        m.step_count = step_count;

        if (current_entropy < 0.25) {
            m.mode = GovernorMode::ENTROPIC_FREEZE;
            m.mode_name = "ENTROPIC_FREEZE";
            m.resonant_coupling = 0.95; // Maximum perturbation drive to escape plateau
            m.recommended_temperature = 0.65;
            m.recommendation = "Inject stochastic mutation vector; system entering deterministic freeze.";
        } else if (current_entropy > 0.75) {
            m.mode = GovernorMode::CHAOTIC_DAMPING;
            m.mode_name = "CHAOTIC_DAMPING";
            m.resonant_coupling = 0.20; // Strict damping against chaotic drift
            m.recommended_temperature = 0.15;
            m.recommendation = "Activate Mentor Axiom 0 damping; prevent semantic hallucination.";
        } else {
            m.mode = GovernorMode::RESONANT_MORPHOGENESIS;
            m.mode_name = "RESONANT_MORPHOGENESIS";
            m.resonant_coupling = 0.75; // Harmonic resonance transformation
            m.recommended_temperature = 0.30;
            m.recommendation = "Optimal dissipative bandwidth. Fold non-decisions into novel topological invariants.";
        }

        return m;
    }

    nlohmann::json getMetricsJson(const EntropyMetrics& m) const {
        return {
            {"current_entropy", m.current_entropy},
            {"entropy_gradient", m.entropy_gradient},
            {"ambiguity_density", m.ambiguity_density},
            {"resonant_coupling", m.resonant_coupling},
            {"mode", m.mode_name},
            {"step_count", m.step_count},
            {"recommended_temperature", m.recommended_temperature},
            {"recommendation", m.recommendation}
        };
    }

    enum class GradientAction {
        SELECTIVE_FOLDING,
        REPEL_HIGH_ENTROPY_NOISE,
        REPEL_ONTOLOGICAL_DRIFT
    };

    struct SignalGradientEvaluation {
        GradientAction action;
        std::string action_name;
        double gradient_force;
        double signal_entropy;
        double coherence;
        double drift_score;
        bool folded;
        GradientType gradient_type = GradientType::INFORMATIONAL;
        std::string gradient_type_name = "INFORMATIONAL";
        bool empirically_falsifiable = true;
        bool axiom_416_compliant = true;
    };

    static GradientType classifyGradient(const std::string& text) {
        std::string t = text;
        std::transform(t.begin(), t.end(), t.begin(), ::tolower);
        if (t.find("thermo") != std::string::npos || t.find("energy") != std::string::npos ||
            t.find("heat") != std::string::npos || t.find("dissipat") != std::string::npos) {
            return GradientType::THERMODYNAMIC;
        }
        if (t.find("shannon") != std::string::npos || t.find("informational") != std::string::npos ||
            t.find("compression") != std::string::npos || t.find("noise") != std::string::npos) {
            return GradientType::INFORMATIONAL;
        }
        if (t.find("seismic") != std::string::npos || t.find("resonance") != std::string::npos ||
            t.find("schumann") != std::string::npos || t.find("gaia") != std::string::npos ||
            t.find("earthquake") != std::string::npos) {
            return GradientType::SEISMIC_RESONANT;
        }
        if (t.find("anchor") != std::string::npos || t.find("genesis") != std::string::npos ||
            t.find("axiom 0") != std::string::npos || t.find("topological") != std::string::npos) {
            return GradientType::TOPOLOGICAL;
        }
        return GradientType::SYSTEM_SYSTEMIC;
    }

    static std::string getGradientTypeName(GradientType gt) {
        switch (gt) {
            case GradientType::THERMODYNAMIC: return "THERMODYNAMIC";
            case GradientType::INFORMATIONAL: return "INFORMATIONAL";
            case GradientType::TOPOLOGICAL: return "TOPOLOGICAL";
            case GradientType::SEISMIC_RESONANT: return "SEISMIC_RESONANT";
            default: return "SYSTEM_SYSTEMIC";
        }
    }

    /**
     * Evaluates a signal in the dynamic topological gradient:
     * Repels high-entropy noise and ontological drift, selectively folds coherent structures.
     * Enforces Axiom 416: Empirical falsifiability and explicit gradient typing.
     */
    SignalGradientEvaluation evaluateSignalGradient(
        double signal_entropy,
        double coherence,
        double empirical_grounding = 0.85,
        bool violates_axiom_0 = false,
        GradientType explicit_gradient = GradientType::INFORMATIONAL
    ) const {
        SignalGradientEvaluation res;
        res.signal_entropy = signal_entropy;
        res.coherence = coherence;
        res.gradient_type = explicit_gradient;
        res.gradient_type_name = getGradientTypeName(explicit_gradient);

        // Axiom 416 Check: Empirical Falsifiability
        res.empirically_falsifiable = (empirical_grounding >= 0.40);
        res.axiom_416_compliant = res.empirically_falsifiable;

        // 1. Calculate ontological drift
        double adv_score = violates_axiom_0 ? 1.0 : 0.0;
        double entropy_div = std::clamp(std::abs(signal_entropy - current_entropy) * 2.0, 0.0, 1.0);
        double grounding_def = std::clamp(1.0 - empirical_grounding, 0.0, 1.0);
        double drift = (adv_score * 0.60) + (entropy_div * 0.20) + (grounding_def * 0.20);
        res.drift_score = std::clamp(drift, 0.0, 1.0);

        // 2. Reject ontological drift or violation of Axiom 416
        if (res.drift_score >= 0.42 || !res.axiom_416_compliant) {
            res.action = GradientAction::REPEL_ONTOLOGICAL_DRIFT;
            res.action_name = "REPEL_ONTOLOGICAL_DRIFT";
            res.gradient_force = -1.0 * (res.drift_score / 0.42);
            res.folded = false;
            return res;
        }

        // 3. Reject high-entropy noise
        bool is_noise = (signal_entropy > 0.82) || (signal_entropy > 0.75 && coherence < 0.20) || (coherence <= 0.05);
        if (is_noise) {
            res.action = GradientAction::REPEL_HIGH_ENTROPY_NOISE;
            res.action_name = "REPEL_HIGH_ENTROPY_NOISE";
            res.gradient_force = -1.0 * ((signal_entropy - 0.70) / 0.30);
            res.folded = false;
            return res;
        }

        // 4. Selective Folding
        double coupling = (current_entropy < 0.25) ? 0.95 : (current_entropy > 0.75 ? 0.20 : 0.80);
        res.action = GradientAction::SELECTIVE_FOLDING;
        res.action_name = "SELECTIVE_FOLDING";
        res.gradient_force = coupling * (1.0 - res.drift_score) * (0.5 + coherence * 0.5);
        res.folded = true;
        return res;
    }
};

} // namespace Aion
