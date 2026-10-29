import re
import json

def analyze_requirements(requirement_text: str, learned_rules: int = 0) -> dict:
    """
    Analyze a requirement text and return a structured analysis.
    
    Args:
        requirement_text: The requirement text to analyze.
        learned_rules: Number of learned rules to consider.
        
    Returns:
        A dictionary containing the analysis results.
    """
    # Basic analysis
    words = requirement_text.split()
    word_count = len(words)
    
    # Extract key terms
    key_terms = []
    for word in words:
        clean_word = re.sub(r'[^a-zA-Z]', '', word).lower()
        if clean_word and len(clean_word) > 3:
            key_terms.append(clean_word)
    
    # Count unique key terms
    unique_terms = list(set(key_terms))
    
    # Determine complexity based on word count and unique terms
    if word_count < 20:
        complexity = "low"
    elif word_count < 50:
        complexity = "medium"
    else:
        complexity = "high"
    
    # Check for specific patterns
    has_self_evolution = any(term in requirement_text.lower() for term in ['self-evolving', 'self evolving', 'evolving'])
    has_meta_cognitive = any(term in requirement_text.lower() for term in ['meta-cognitive', 'meta cognitive', 'metacognitive'])
    has_simulation = any(term in requirement_text.lower() for term in ['simulation', 'simulations'])
    has_validation = any(term in requirement_text.lower() for term in ['validation', 'validate', 'validated'])
    
    # Generate recommendations based on patterns
    recommendations = []
    if has_self_evolution:
        recommendations.append("Implement self-modification mechanisms with safety constraints")
    if has_meta_cognitive:
        recommendations.append("Design meta-cognitive monitoring and reflection systems")
    if has_simulation:
        recommendations.append("Create sandboxed simulation environments for testing")
    if has_validation:
        recommendations.append("Establish external validation protocols and feedback loops")
    
    # Incorporate learned rules
    if learned_rules > 0:
        recommendations.append(f"Leverage {learned_rules} learned rules to guide design decisions")
    
    return {
        "word_count": word_count,
        "unique_terms": unique_terms,
        "complexity": complexity,
        "has_self_evolution": has_self_evolution,
        "has_meta_cognitive": has_meta_cognitive,
        "has_simulation": has_simulation,
        "has_validation": has_validation,
        "recommendations": recommendations,
        "learned_rules_considered": learned_rules
    }