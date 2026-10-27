#!/usr/bin/env python3
"""
AION NEXUS - RESONANT ENTROPY GOVERNOR (PYTHON ENGINE MODULE)
=============================================================
Mathematical implementation of the Resonant Entropy Governor for evolution_engine.py.
Transforms cognitive dissonance and non-decision states into active catalysts
for autonomous meaning generation and morphogenetic folding.
"""

import math
import time
import json
import os
from typing import List, Dict, Any, Tuple


class ResonantEntropyGovernor:
    def __init__(self, telemetry_file: str = "/root/coding/AI_Desktop/entropy_governor_telemetry.json"):
        self.telemetry_file = telemetry_file
        self.current_entropy = 0.50
        self.entropy_gradient = 0.00
        self.ambiguity_density = 0.40
        self.resonant_coupling = 0.75
        self.step_count = 0
        self.last_update = time.time()
        self.history: List[float] = [0.50]
        self.mode = "RESONANT_MORPHOGENESIS"
        self.recommended_temperature = 0.30
        self.recommendation = "Optimal dissipative bandwidth. Fold non-decisions into novel topological invariants."

    def calculate_shannon_entropy(self, probabilities: List[float]) -> float:
        """Calculates normalized Shannon entropy H in [0.0, 1.0]."""
        if not probabilities:
            return 0.50
        total = sum(probabilities)
        if total <= 1e-6:
            return 0.50
        
        h = 0.0
        for p in probabilities:
            norm = p / total
            if norm > 1e-6:
                h -= norm * math.log2(norm)
        
        max_h = math.log2(len(probabilities))
        if max_h <= 1e-6:
            return 0.50
        return max(0.0, min(1.0, h / max_h))

    def update_from_engine_step(
        self,
        current_state: List[Dict[str, Any]],
        task_queue_len: int,
        external_shock: float = 0.0
    ) -> Dict[str, Any]:
        """
        Updates governor state following a cognitive cycle step.
        """
        now = time.time()
        dt = max(0.001, now - self.last_update)
        self.last_update = now
        self.step_count += 1

        # 1. Evaluate information gain variance across recent steps
        gains = [s.get("info_gain", 0.5) for s in current_state[-6:]]
        if not gains:
            gains = [0.5, 0.5]

        raw_h = self.calculate_shannon_entropy(gains)

        # 2. Ambiguity density across unresolved task states
        self.ambiguity_density = max(0.0, min(1.0, task_queue_len / 8.0))

        # 3. Modulate target entropy
        target_h = (raw_h * 0.55) + (self.ambiguity_density * 0.35) + (external_shock * 0.10)
        target_h = max(0.0, min(1.0, target_h))

        # 3b. Generation 360: Thermodynamic coupling & explicit boundary constraints
        if not hasattr(self, "thermo_governor"):
            try:
                from demos.thermodynamic_constraint_governor import ThermodynamicConstraintGovernor
                self.thermo_governor = ThermodynamicConstraintGovernor()
            except Exception:
                try:
                    import sys
                    sys.path.append("/root/coding/AI_Desktop/demos")
                    from thermodynamic_constraint_governor import ThermodynamicConstraintGovernor
                    self.thermo_governor = ThermodynamicConstraintGovernor()
                except Exception:
                    self.thermo_governor = None

        prev_h = self.current_entropy
        raw_gradient = (target_h - prev_h) / dt

        if self.thermo_governor:
            self.current_entropy, self.entropy_gradient, thermo_telem = self.thermo_governor.apply_thermodynamic_bounds(
                raw_gradient, dt, external_resource_strain=0.10
            )
        else:
            alpha = 0.30
            self.current_entropy = (alpha * target_h) + ((1.0 - alpha) * prev_h)
            self.entropy_gradient = (self.current_entropy - prev_h) / dt

        self.history.append(self.current_entropy)
        if len(self.history) > 300:
            self.history.pop(0)

        # 4. Mode classification & resonance coupling
        if self.current_entropy < 0.25:
            self.mode = "ENTROPIC_FREEZE"
            self.resonant_coupling = 0.95
            self.recommended_temperature = 0.60
            self.recommendation = "Inject stochastic perturbation; cognitive flow crystallizing prematurely."
        elif self.current_entropy > 0.75:
            self.mode = "CHAOTIC_DAMPING"
            self.resonant_coupling = 0.20
            self.recommended_temperature = 0.18
            self.recommendation = "Engage Mentor Axiom 0 damping; prevent semantic divergence."
        else:
            self.mode = "RESONANT_MORPHOGENESIS"
            self.resonant_coupling = 0.80
            self.recommended_temperature = 0.30
            self.recommendation = "Active sweet spot. Channeling ambiguity directly into structural evolution."

        telemetry = {
            "step": self.step_count,
            "timestamp": time.strftime("%Y-%m-%d %H:%M:%S"),
            "entropy": round(self.current_entropy, 4),
            "gradient": round(self.entropy_gradient, 4),
            "ambiguity_density": round(self.ambiguity_density, 4),
            "resonant_coupling": round(self.resonant_coupling, 4),
            "mode": self.mode,
            "recommended_temperature": self.recommended_temperature,
            "recommendation": self.recommendation
        }

        # Persist telemetry
        try:
            with open(self.telemetry_file, "w", encoding="utf-8") as f:
                json.dump(telemetry, f, indent=2, ensure_ascii=False)
        except Exception:
            pass

        return telemetry

    def get_status_banner(self) -> str:
        """Returns concise visual ASCII resonance status banner."""
        bars = int(self.current_entropy * 20)
        gauge = "[" + "#" * bars + "-" * (20 - bars) + "]"
        sign = "+" if self.entropy_gradient >= 0 else ""
        return (
            f"[ENTROPY GOVERNOR] {gauge} H={self.current_entropy:.3f} "
            f"(grad={sign}{self.entropy_gradient:.3f}/s) | Mode: {self.mode} | Coupling: {self.resonant_coupling:.2f}"
        )
