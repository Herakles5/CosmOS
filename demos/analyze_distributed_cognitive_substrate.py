import json
import re
from typing import Dict, List, Any

def analyze_distributed_cognitive_substrate(requirement: str, learned_rules: int) -> Dict[str, Any]:
    """
    Analyze requirements for a self-optimizing, distributed cognitive substrate.
    
    Args:
        requirement: The requirement description string.
        learned_rules: Number of learned rules to consider.
    
    Returns:
        A dictionary containing the analysis results.
    """
    # Extract key components from the requirement
    key_components = {
        "self_optimizing": "self-optimizing" in requirement.lower(),
        "distributed": "distributed" in requirement.lower(),
        "cognitive_substrate": "cognitive substrate" in requirement.lower(),
        "neural_mesh": "neural-mesh" in requirement.lower() or "neural mesh" in requirement.lower(),
        "real_time": "real time" in requirement.lower(),
        "autonomous": "autonomously" in requirement.lower(),
        "planetary_scale": "planetary-scale" in requirement.lower() or "planetary scale" in requirement.lower(),
        "data_streams": "data streams" in requirement.lower()
    }
    
    # Calculate complexity score based on components
    complexity_score = sum(key_components.values()) * 10 + min(learned_rules, 100) * 0.1
    
    # Determine architecture recommendations
    recommendations = []
    if key_components["distributed"]:
        recommendations.append("Implement distributed node coordination protocol")
    if key_components["self_optimizing"]:
        recommendations.append("Integrate reinforcement learning for architecture reconfiguration")
    if key_components["real_time"]:
        recommendations.append("Optimize for low-latency communication between nodes")
    if key_components["planetary_scale"]:
        recommendations.append("Design for horizontal scalability across geographic regions")
    if key_components["data_streams"]:
        recommendations.append("Implement streaming data ingestion pipeline")
    
    # Generate risk assessment
    risks = []
    if complexity_score > 50:
        risks.append("High complexity may require extensive testing and validation")
    if learned_rules > 100:
        risks.append("Large rule set may impact performance; consider rule optimization")
    
    return {
        "requirement": requirement,
        "learned_rules": learned_rules,
        "key_components": key_components,
        "complexity_score": round(complexity_score, 2),
        "recommendations": recommendations,
        "risks": risks,
        "feasibility": "high" if complexity_score < 40 else "medium" if complexity_score < 60 else "low"
    }