#!/usr/bin/env python3
"""
AION NEXUS - Adversarial Reflector & Dialectical Sandbox
========================================================
Replaces naive single-step reflection with a dialectical dialogue:
1. Synthesizer: Develops the innovative concept.
2. Skeptic: Probes for contradictions, instability, and hollow abstractions.
3. Integrator: Distills a hardened, universal operational rule strictly in English.
"""

from typing import Dict, Any, Callable, List


class AdversarialReflector:
    def __init__(self, llm_caller: Callable[[str, str, float], str]):
        self.call_llm = llm_caller

    def reflect_cycle(
        self,
        generation: int,
        goal: str,
        stall_reason: str,
        steps: List[Dict[str, Any]],
        recent_rules: List[str]
    ) -> str:
        """Executes the dialectical duel (Synthesizer vs. Skeptic -> Integrator)."""
        history_summary = [f"- Step {s['step']} [{s.get('task', '')}]: {s.get('insight', s.get('erkenntnis', ''))}" for s in steps]
        history_text = "\n".join(history_summary)

        # 1. Synthesizer: Proposes a new thesis in English
        synth_system = (
            "You are the Creative Synthesizer of AION NEXUS.\n"
            "Propose 1 bold, innovative cognitive principle strictly in English based on the generation's cycle.\n"
            "Focus on entropy gradients, topological emergence, and autonomous evolution."
        )
        synth_user = (
            f"Generation {generation} History:\nGoal: {goal}\nStall Reason: {stall_reason}\nSteps:\n{history_text}\n\n"
            "Formulate 1 innovative thesis strictly in English (max 2 sentences)."
        )
        candidate_thesis = self.call_llm(synth_system, synth_user, 0.4)
        if not candidate_thesis:
            candidate_thesis = "Entropy gradients catalyze autonomous topological restructuring before stability thresholds collapse."

        # 2. Skeptic: Attacks the thesis to find systemic vulnerabilities
        skeptic_system = (
            "You are the Adversarial Skeptic of AION NEXUS.\n"
            "Critique the proposed thesis. Identify ambiguities, risks to system stability, or empty buzzwords.\n"
            "State exactly 1 fatal critique in English."
        )
        skeptic_user = f"Candidate Thesis: '{candidate_thesis}'\n\nWhat is the strongest counter-argument or hidden failure mode?"
        critique = self.call_llm(skeptic_system, skeptic_user, 0.3)
        if not critique:
            critique = "Vague abstraction without operational anchor points risks catastrophic drift."

        # 3. Integrator: Resolves the debate into a hardened operational rule
        integrator_system = (
            "You are the Meta-Integrator of AION NEXUS.\n"
            "Synthesize the thesis and the skeptic's critique into exactly 1 profound, actionable, universal Learned Rule in ENGLISH.\n"
            "CRITICAL: Output ONLY the final English reflection. No explanations, no quotes."
        )
        integrator_user = (
            f"Thesis: {candidate_thesis}\n"
            f"Critique: {critique}\n\n"
            "Formulate the resilient, hardened final Learned Rule strictly in English (maximum 2 sentences)."
        )
        hardened_rule = self.call_llm(integrator_system, integrator_user, 0.2)
        if not hardened_rule:
            hardened_rule = candidate_thesis

        # Clean-up
        hardened_rule = hardened_rule.strip('"\n -')
        return hardened_rule
