import re
import json
from typing import List, Dict, Any

def analyze_self_modifying_calculus(requirement: str, learned_rules: int) -> Dict[str, Any]:
    """
    Analyze a requirement for a self-modifying topological calculus.
    
    Args:
        requirement: The requirement string to analyze.
        learned_rules: The number of learned rules to consider.
        
    Returns:
        A dictionary containing the analysis results.
    """
    # Extract key concepts from the requirement
    key_concepts = [
        "self-modifying",
        "topological calculus",
        "dynamically reconfigures",
        "state-transition rules",
        "real-time entropy differentials",
        "autonomously discover",
        "computational geometries",
        "semantic clarity",
        "external intervention"
    ]
    
    # Check which concepts are present in the requirement
    present_concepts = [concept for concept in key_concepts if concept.lower() in requirement.lower()]
    
    # Analyze complexity based on number of concepts and learned rules
    complexity_score = len(present_concepts) * 10 + learned_rules * 0.1
    
    # Determine risk level
    if complexity_score > 100:
        risk_level = "high"
    elif complexity_score > 50:
        risk_level = "medium"
    else:
        risk_level = "low"
    
    # Generate recommendations
    recommendations = []
    if "self-modifying" in present_concepts:
        recommendations.append("Implement version control for state-transition rules")
    if "topological calculus" in present_concepts:
        recommendations.append("Define topological invariants to maintain system stability")
    if "entropy differentials" in present_concepts:
        recommendations.append("Establish entropy monitoring and threshold mechanisms")
    if "autonomously discover" in present_concepts:
        recommendations.append("Create sandboxed environment for testing new computational geometries")
    if "semantic clarity" in present_concepts:
        recommendations.append("Develop semantic validation framework")
    
    # Calculate confidence score based on learned rules
    confidence_score = min(1.0, learned_rules / 500.0)
    
    return {
        "requirement": requirement,
        "learned_rules": learned_rules,
        "present_concepts": present_concepts,
        "complexity_score": round(complexity_score, 2),
        "risk_level": risk_level,
        "recommendations": recommendations,
        "confidence_score": round(confidence_score, 2)
    }