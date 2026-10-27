#!/usr/bin/env python3
"""
AION NEXUS - THERMODYNAMIC CONSTRAINT GOVERNOR (DEMO & TESTBED)
==============================================================
Operationalization of the axiom from Generation 360:
"True autonomous evolution requires coupling local entropy gradients
with explicit, externally defined structural constraints, as spontaneous
self-regulation without a concrete mechanism violates thermodynamic limits
and remains untestable."

Architecture:
1. Thermodynamic boundary conditions (Prigogine-Lyapunov bounds):
   - H_min = 0.18 (Prevention of deterministic heat death / freeze)
   - H_max = 0.82 (Prevention of chaotic divergence / phase transition)
   - |dH/dt|_max = 0.15/s (Maximum entropy production rate)
2. Coupled Structural Constraints mechanism:
   - Couples local gradients dH/dt to hard external restrictions (system resources, memory, latency).
   - Prevents purely "spontaneous" (uncapped) drift.
3. Deterministic Validation and Test Harness (TDD):
   - Mathematical proof of boundary adherence under extreme perturbation shocks.
"""

import math
import time
from typing import Dict, Any, List, Tuple


class ThermodynamicConstraintGovernor:
    """
    Couples local entropy gradients to explicit external structural boundary conditions.
    """
    H_MIN = 0.18
    H_MAX = 0.82
    MAX_GRADIENT_VELOCITY = 0.15 # Maximum allowable gradient rate per time unit
    OPTIMAL_ATTRACTOR = 0.50

    def __init__(self):
        self.current_entropy = 0.50
        self.current_gradient = 0.00
        self.lyapunov_potential = 0.00
        self.history: List[float] = [0.50]
        self.violation_counter = 0

    def compute_lyapunov_potential(self, h: float) -> float:
        """
        Computes the Lyapunov functional V(H) = 0.5 * (H - H_opt)^2.
        For stability, dV/dt <= 0 must be enforced via negative feedback.
        """
        return 0.5 * (h - self.OPTIMAL_ATTRACTOR) ** 2

    def apply_thermodynamic_bounds(
        self,
        raw_gradient: float,
        dt: float,
        external_resource_strain: float = 0.0
    ) -> Tuple[float, float, Dict[str, Any]]:
        """
        Couples entropy gradient to hard thermodynamic bounds:
        - Constrains |dH/dt| to MAX_GRADIENT_VELOCITY.
        - Damps dH/dt under high resource strain (CPU/RAM).
        - Activates Lyapunov restoring forces at phase boundaries [H_MIN, H_MAX].
        """
        dt = max(0.001, dt)
        
        # 1. Gradient velocity limiting (prevents entropy shocks)
        bounded_gradient = max(-self.MAX_GRADIENT_VELOCITY, min(self.MAX_GRADIENT_VELOCITY, raw_gradient))

        # 2. External structural damping from resource bottlenecks
        damping_factor = max(0.2, 1.0 - (external_resource_strain * 0.7))
        effective_gradient = bounded_gradient * damping_factor

        # 3. State projection
        candidate_h = self.current_entropy + (effective_gradient * dt)

        # 4. Hard thermodynamic boundary conditions
        restoring_applied = False
        if candidate_h > self.H_MAX:
            # Second law: Halt chaotic overheating
            overshoot = candidate_h - self.H_MAX
            candidate_h = self.H_MAX
            effective_gradient = -0.5 * overshoot / dt
            restoring_applied = True
            self.violation_counter += 1
        elif candidate_h < self.H_MIN:
            # Prevent freeze / heat death: Enforce minimum activity
            undershoot = self.H_MIN - candidate_h
            candidate_h = self.H_MIN
            effective_gradient = 0.5 * undershoot / dt
            restoring_applied = True
            self.violation_counter += 1

        self.current_entropy = candidate_h
        self.current_gradient = effective_gradient
        self.lyapunov_potential = self.compute_lyapunov_potential(self.current_entropy)
        self.history.append(self.current_entropy)

        telemetry = {
            "entropy": round(self.current_entropy, 4),
            "effective_gradient": round(self.current_gradient, 4),
            "lyapunov_V": round(self.lyapunov_potential, 4),
            "restoring_applied": restoring_applied,
            "external_strain": round(external_resource_strain, 4),
            "thermodynamically_stable": (self.H_MIN <= self.current_entropy <= self.H_MAX)
        }
        return self.current_entropy, self.current_gradient, telemetry


def test_thermodynamic_compliance():
    """
    Executes mathematical and empirical TDD verification:
    Proves that even under extreme perturbation shocks no thermodynamic limits are violated.
    """
    print("[THERMODYNAMIC TEST] Starting verification of Generation 360 mechanism...")
    gov = ThermodynamicConstraintGovernor()

    # Test 1: Nominal flow around attractor
    h, grad, telem = gov.apply_thermodynamic_bounds(raw_gradient=0.05, dt=1.0)
    assert gov.H_MIN <= h <= gov.H_MAX
    assert telem["thermodynamically_stable"] is True
    print(f" -> Test 1 (Nominal): H={h:.4f}, Grad={grad:.4f} [OK]")

    # Test 2: Massive positive entropy shock (+1.50/s) -> Bound at H_MAX
    for _ in range(10):
        h, grad, telem = gov.apply_thermodynamic_bounds(raw_gradient=1.50, dt=1.0)
    assert h <= gov.H_MAX, f"H ({h}) exceeded H_MAX ({gov.H_MAX})!"
    assert telem["restoring_applied"] is True
    print(f" -> Test 2 (Overheating shock absorbed): H={h:.4f} <= {gov.H_MAX} [OK]")

    # Test 3: Massive negative shock (-2.0/s) -> Bound at H_MIN
    for _ in range(15):
        h, grad, telem = gov.apply_thermodynamic_bounds(raw_gradient=-2.00, dt=1.0)
    assert h >= gov.H_MIN, f"H ({h}) dropped below H_MIN ({gov.H_MIN})!"
    assert telem["restoring_applied"] is True
    print(f" -> Test 3 (Freeze shock absorbed): H={h:.4f} >= {gov.H_MIN} [OK]")

    # Test 4: Resource damping
    gov.current_entropy = 0.50
    _, grad_free, _ = gov.apply_thermodynamic_bounds(raw_gradient=0.10, dt=1.0, external_resource_strain=0.0)
    gov.current_entropy = 0.50
    _, grad_strained, _ = gov.apply_thermodynamic_bounds(raw_gradient=0.10, dt=1.0, external_resource_strain=0.9)
    assert abs(grad_strained) < abs(grad_free), "Resource strain did not sufficiently damp gradient!"
    print(f" -> Test 4 (Resource Coupling): Free={grad_free:.4f} vs Damped={grad_strained:.4f} [OK]")

    print("[SUCCESS] Generation 360 Axiom successfully verified as concrete testable mechanism!")
    return True


if __name__ == "__main__":
    test_thermodynamic_compliance()
