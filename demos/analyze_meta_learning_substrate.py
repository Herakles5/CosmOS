import json
import re
from typing import List, Dict, Any

def analyze_meta_learning_substrate(requirement: str, learned_rules: int) -> Dict[str, Any]:
    """
    Analyze the given requirement for a self-modifying, entropy-aware meta-learning substrate.
    
    Args:
        requirement: The requirement string to analyze.
        learned_rules: The number of learned rules to consider.
        
    Returns:
        A dictionary containing the analysis results.
    """
    # Extract key components from the requirement
    key_components = [
        "self-modifying",
        "entropy-aware",
        "meta-learning",
        "energy-optimal",
        "cognitive architectures",
        "quantum-classical nodes",
        "free-energy gradients",
        "systemic resilience",
        "information integrity"
    ]
    
    # Check which components are present in the requirement
    present_components = [comp for comp in key_components if comp in requirement.lower()]
    
    # Calculate complexity score based on number of components and learned rules
    complexity_score = len(present_components) * 10 + learned_rules // 10
    
    # Determine risk level based on complexity
    if complexity_score > 100:
        risk_level = "high"
    elif complexity_score > 50:
        risk_level = "medium"
    else:
        risk_level = "low"
    
    # Generate recommendations based on the analysis
    recommendations = []
    if "self-modifying" in present_components:
        recommendations.append("Implement strict version control and rollback mechanisms for self-modification.")
    if "entropy-aware" in present_components:
        recommendations.append("Develop entropy monitoring and mitigation strategies.")
    if "energy-optimal" in present_components:
        recommendations.append("Optimize energy consumption through adaptive resource allocation.")
    if "quantum-classical nodes" in present_components:
        recommendations.append("Ensure compatibility and efficient communication between quantum and classical nodes.")
    if "free-energy gradients" in present_components:
        recommendations.append("Implement continuous calibration of free-energy gradients for stability.")
    if "systemic resilience" in present_components:
        recommendations.append("Design fault-tolerant architectures to maintain systemic resilience.")
    if "information integrity" in present_components:
        recommendations.append("Implement robust data validation and integrity checks.")
    
    return {
        "requirement": requirement,
        "learned_rules": learned_rules,
        "present_components": present_components,
        "complexity_score": complexity_score,
        "risk_level": risk_level,
        "recommendations": recommendations
    }