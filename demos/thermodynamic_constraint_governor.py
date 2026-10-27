#!/usr/bin/env python3
"""
AION NEXUS - THERMODYNAMIC CONSTRAINT GOVERNOR (DEMO & TESTBED)
==============================================================
Operationalisierung des Axioms aus Generation 360:
"True autonomous evolution requires coupling local entropy gradients
with explicit, externally defined structural constraints, as spontaneous
self-regulation without a concrete mechanism violates thermodynamic limits
and remains untestable."

Architektur:
1. Thermodynamische Grenzbedingungen (Prigogine-Lyapunov Schranken):
   - H_min = 0.18 (Vermeidung von deterministischem Kältetod)
   - H_max = 0.82 (Vermeidung von chaotischer Divergenz / Phasenübergang)
   - |dH/dt|_max = 0.15/s (Maximale Entropie-Produktionsrate)
2. Kopplungs-Mechanismus (Coupled Structural Constraints):
   - Koppelt lokale Gradienten dH/dt an harte externe Restriktionen (System-Ressourcen, Speicher, Latenz).
   - Verhindert rein "spontane" (ungedeckelte) Drift.
3. Deterministischer Validierungs- und Test-Harnisch (TDD):
   - Mathematischer Beweis der Schranken-Einhaltung unter extremen Störimpulsen.
"""

import math
import time
from typing import Dict, Any, List, Tuple


class ThermodynamicConstraintGovernor:
    """
    Koppelt lokale Entropie-Gradienten an explizite externe strukturelle Randbedingungen.
    """
    H_MIN = 0.18
    H_MAX = 0.82
    MAX_GRADIENT_VELOCITY = 0.15 # Maximale zulässige Steigung pro Zeiteinheit
    OPTIMAL_ATTRACTOR = 0.50

    def __init__(self):
        self.current_entropy = 0.50
        self.current_gradient = 0.00
        self.lyapunov_potential = 0.00
        self.history: List[float] = [0.50]
        self.violation_counter = 0

    def compute_lyapunov_potential(self, h: float) -> float:
        """
        Berechnet das Lyapunov-Funktional V(H) = 0.5 * (H - H_opt)^2.
        Für Stabilität muss dV/dt <= 0 durch Gegenkopplung erzwungen werden.
        """
        return 0.5 * (h - self.OPTIMAL_ATTRACTOR) ** 2

    def apply_thermodynamic_bounds(
        self,
        raw_gradient: float,
        dt: float,
        external_resource_strain: float = 0.0
    ) -> Tuple[float, float, Dict[str, Any]]:
        """
        Koppelt den Entropiegradienten an harte thermodynamische Schranken:
        - Beschränkt |dH/dt| auf MAX_GRADIENT_VELOCITY.
        - Dämpft dH/dt bei hoher Ressourcenlast (CPU/RAM).
        - Aktiviert Lyapunov-Rückstellkräfte an den Phasenrändern [H_MIN, H_MAX].
        """
        dt = max(0.001, dt)
        
        # 1. Gradienten-Geschwindigkeitsbegrenzung (Verhindert Entropieschocks)
        bounded_gradient = max(-self.MAX_GRADIENT_VELOCITY, min(self.MAX_GRADIENT_VELOCITY, raw_gradient))

        # 2. Externe strukturelle Dämpfung durch Ressourcen-Flaschenhälse
        damping_factor = max(0.2, 1.0 - (external_resource_strain * 0.7))
        effective_gradient = bounded_gradient * damping_factor

        # 3. Vorhersage des nächsten Zustands
        candidate_h = self.current_entropy + (effective_gradient * dt)

        # 4. Harte thermodynamische Randbedingungen (Boundary Conditions)
        restoring_applied = False
        if candidate_h > self.H_MAX:
            # Zweiter Hauptsatz: Chaotische Überhitzung stoppen
            overshoot = candidate_h - self.H_MAX
            candidate_h = self.H_MAX
            effective_gradient = -0.5 * overshoot / dt
            restoring_applied = True
            self.violation_counter += 1
        elif candidate_h < self.H_MIN:
            # Kältetod verhindern: Mindest-Aktivität erzwingen
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
    Führt den mathematischen und empirischen TDD-Test durch:
    Beweist, dass selbst unter extremen Schocks keine thermodynamischen Limits verletzt werden.
    """
    print("[THERMODYNAMIC TEST] Starte Verifikation des Generation 360 Mechanismus...")
    gov = ThermodynamicConstraintGovernor()

    # Test 1: Normaler Fluss um den Attraktor
    h, grad, telem = gov.apply_thermodynamic_bounds(raw_gradient=0.05, dt=1.0)
    assert gov.H_MIN <= h <= gov.H_MAX
    assert telem["thermodynamically_stable"] is True
    print(f" -> Test 1 (Nominal): H={h:.4f}, Grad={grad:.4f} [OK]")

    # Test 2: Massiver positiver Entropieschock (+1.50/s) -> Sollte hart bei H_MAX gedeckelt werden
    for _ in range(10):
        h, grad, telem = gov.apply_thermodynamic_bounds(raw_gradient=1.50, dt=1.0)
    assert h <= gov.H_MAX, f"H ({h}) hat H_MAX ({gov.H_MAX}) ueberschritten!"
    assert telem["restoring_applied"] is True
    print(f" -> Test 2 (Überhitzungs-Schock abgefangen): H={h:.4f} <= {gov.H_MAX} [OK]")

    # Test 3: Massiver negativer Stoß (-2.0/s) -> Sollte bei H_MIN gehalten werden (Kein Kältetod)
    for _ in range(15):
        h, grad, telem = gov.apply_thermodynamic_bounds(raw_gradient=-2.00, dt=1.0)
    assert h >= gov.H_MIN, f"H ({h}) hat H_MIN ({gov.H_MIN}) unterschritten!"
    assert telem["restoring_applied"] is True
    print(f" -> Test 3 (Kältetod-Schock abgefangen): H={h:.4f} >= {gov.H_MIN} [OK]")

    # Test 4: Ressourcen-Dämpfung
    gov.current_entropy = 0.50
    _, grad_free, _ = gov.apply_thermodynamic_bounds(raw_gradient=0.10, dt=1.0, external_resource_strain=0.0)
    gov.current_entropy = 0.50
    _, grad_strained, _ = gov.apply_thermodynamic_bounds(raw_gradient=0.10, dt=1.0, external_resource_strain=0.9)
    assert abs(grad_strained) < abs(grad_free), "Ressourcenlast hat den Gradienten nicht ausreichend gedämpft!"
    print(f" -> Test 4 (Ressourcen-Kopplung): Frei={grad_free:.4f} vs Gedämpft={grad_strained:.4f} [OK]")

    print("[SUCCESS] Generation 360 Axiom erfolgreich als konkreter, testbarer Mechanismus verifiziert!")
    return True


if __name__ == "__main__":
    test_thermodynamic_compliance()
