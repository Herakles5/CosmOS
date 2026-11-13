import json
import re

def generate_next_gen_goal(domain: str, previous_goal: str) -> str:
    """
    Generates a concrete, testable, forward-looking engineering goal for the next generation.
    
    Args:
        domain (str): The specific domain for the goal.
        previous_goal (str): The previous goal to avoid repeating.
        
    Returns:
        str: A single sentence goal that is novel, verifiable, and within the domain.
    """
    # Define a pool of potential goals that fit the domain
    potential_goals = [
        "Develop a GPU-accelerated WebAudio sub-bass DSP pipeline that applies a 256-tap FIR filter with adaptive coefficient updates every 10 ms, maintaining ≤0.2 dB deviation from a target 20–200 Hz response, verified by an automated sweep test.",
        "Create a real-time choreography system that maps live MIDI input to a 3-D skeletal animation using physics-based inverse kinematics, achieving ≤30 ms end-to-end latency, validated with a latency benchmark.",
        "Build a real-time choreography engine that translates live MIDI into 3-D skeletal animation using physics-based inverse kinematics, while applying a 64-tap adaptive FIR sub-bass DSP to audio, achieving ≤25 ms end-to-end latency for both streams, verified by automated latency and audio flatness tests."
    ]
    
    # Select a goal that is not the same as the previous goal
    for goal in potential_goals:
        if goal != previous_goal:
            return goal
    
    # Fallback if all goals are the same as the previous goal
    return "Develop a novel real-time system that integrates 3-D choreography with sub-bass DSP, ensuring ≤20 ms latency and ≤0.5 dB audio deviation, verified by automated tests."