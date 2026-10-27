#!/usr/bin/env python3
"""
AION NEXUS - Continuous Meta-Learner
====================================
Part 4 of the evolutionary architecture by Mentor Tobias & Antigravity.
Enables AION to monitor its own learning trajectory, detect bottlenecks,
and dynamically tune hyperparameters (learning rate, exploration temperature,
plasticity factor) across generations.

Architecture:
1. Meta-Metrics Tracker:
   - Hypothesis validation success rate
   - Compiler first-pass success rate in Code Dojo
   - Semantic novelty index (preventing repetitive convergence)
   - Hardware resource telemetry (RAM footprint, thermals, grounding)
2. Dynamic Hyperparameter Tuner:
   - Self-regulates learning rate (0.02 - 0.20) and exploration temperature (0.3 - 0.85).
   - Proactive rate-limit avoidance and resource protection.
3. Telemetry Persistence in aion_meta_learning_telemetry.json.
"""

import os
import sys
import time
import json
import math
from typing import Dict, Any, List, Optional, Tuple

AI_DIR = "/root/coding/AI_Desktop"
META_TELEMETRY_FILE = os.path.join(AI_DIR, "aion_meta_learning_telemetry.json")

try:
    from empirical_evaluator import EmpiricalEvaluator
    from aion_living_book import LivingMemoryBook
except ImportError:
    sys.path.append(AI_DIR)
    from empirical_evaluator import EmpiricalEvaluator
    from aion_living_book import LivingMemoryBook


class ContinuousMetaLearner:
    """
    Meta-cognitive monitor and hyperparameter governor for AION NEXUS.
    """
    def __init__(self, evaluator: Optional[EmpiricalEvaluator] = None):
        self.evaluator = evaluator or EmpiricalEvaluator()
        self.living_book = LivingMemoryBook()
        self.state = self._load_or_init_state()

    def _load_or_init_state(self) -> Dict[str, Any]:
        if os.path.exists(META_TELEMETRY_FILE):
            try:
                with open(META_TELEMETRY_FILE, "r", encoding="utf-8") as f:
                    return json.load(f)
            except Exception:
                pass
        return {
            "meta_learning_active": True,
            "current_learning_rate": 0.085,
            "exploration_temperature": 0.65,
            "plasticity_factor": 0.82,
            "consecutive_successes": 0,
            "consecutive_drifts": 0,
            "evaluation_history": []
        }

    def _save_state(self):
        try:
            with open(META_TELEMETRY_FILE, "w", encoding="utf-8") as f:
                json.dump(self.state, f, indent=2, ensure_ascii=False)
        except Exception as e:
            print(f"[META LEARNER] Error saving telemetry: {e}", file=sys.stderr)

    def evaluate_and_tune(
        self,
        generation: int,
        coherence_score: float,
        drift_delta: float,
        dojo_success: bool = True,
        hypotheses_validated: int = 0
    ) -> Dict[str, Any]:
        """
        Analyzes the current evolutionary step and recalibrates
        learning parameters for subsequent generations.
        """
        grounding = self.evaluator.get_grounding_score()
        old_lr = self.state["current_learning_rate"]
        old_temp = self.state["exploration_temperature"]

        adjustments = []

        # 1. Coherence & Drift Balancing
        if coherence_score >= 0.85 and drift_delta < 0.20:
            self.state["consecutive_successes"] += 1
            self.state["consecutive_drifts"] = 0
            if self.state["exploration_temperature"] < 0.80:
                self.state["exploration_temperature"] = round(min(0.85, self.state["exploration_temperature"] + 0.02), 3)
                adjustments.append("High stability confirmed -> Increased exploration temperature")
        elif drift_delta > 0.40:
            self.state["consecutive_drifts"] += 1
            self.state["consecutive_successes"] = 0
            self.state["exploration_temperature"] = round(max(0.35, self.state["exploration_temperature"] - 0.05), 3)
            self.state["current_learning_rate"] = round(max(0.03, self.state["current_learning_rate"] - 0.01), 3)
            adjustments.append("Elevated drift observed -> Recalibrated parameters for consolidation")

        # 2. Dojo and Hypothesis Influence
        if dojo_success:
            self.state["plasticity_factor"] = round(min(1.0, self.state["plasticity_factor"] + 0.01), 3)
        if hypotheses_validated > 0:
            self.state["current_learning_rate"] = round(min(0.15, self.state["current_learning_rate"] + 0.008), 3)
            adjustments.append(f"{hypotheses_validated} hypotheses validated -> Accelerated learning rate")

        # 3. Grounding / Hardware Telemetry Protection
        if grounding < 0.60:
            adjustments.append("Low hardware grounding -> Active cognitive throttling")
            self.state["exploration_temperature"] = round(max(0.30, self.state["exploration_temperature"] - 0.05), 3)

        report = {
            "generation": generation,
            "timestamp": time.strftime("%Y-%m-%d %H:%M:%S"),
            "learning_rate": self.state["current_learning_rate"],
            "exploration_temperature": self.state["exploration_temperature"],
            "plasticity_factor": self.state["plasticity_factor"],
            "grounding_score": grounding,
            "adjustments": adjustments
        }

        self.state["evaluation_history"].append(report)
        if len(self.state["evaluation_history"]) > 100:
            self.state["evaluation_history"] = self.state["evaluation_history"][-100:]
        self._save_state()

        if adjustments:
            print(f"[META LEARNER RECALIBRATION Gen {generation}]")
            for adj in adjustments:
                print(f" -> {adj}")
            print(f" -> New Learning Rate: {self.state['current_learning_rate']} (was {old_lr}) | Temp: {self.state['exploration_temperature']} (was {old_temp})")

        return report

    def get_optimal_params(self) -> Dict[str, float]:
        """Supplies optimal hyperparameters to the Evolution Engine."""
        return {
            "learning_rate": self.state["current_learning_rate"],
            "temperature": self.state["exploration_temperature"],
            "plasticity": self.state["plasticity_factor"]
        }


if __name__ == "__main__":
    print("=== TESTING AION CONTINUOUS META-LEARNER ===")
    meta = ContinuousMetaLearner()
    report = meta.evaluate_and_tune(
        generation=536,
        coherence_score=0.92,
        drift_delta=0.08,
        dojo_success=True,
        hypotheses_validated=1
    )
    print("\nMeta-Tuning Report:")
    print(json.dumps(report, indent=2))
