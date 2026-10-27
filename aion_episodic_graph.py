#!/usr/bin/env python3
"""
AION NEXUS - Episodic Knowledge Graph
=====================================
Part 2 of the evolutionary architecture by Mentor Tobias & Antigravity.
Replaces flat rule lists with a dynamic, semantically interconnected
memory graph linking knowledge, actors, axioms, and tools across generations.

Capabilities:
- Nodes: Entities, Actors, Axioms, Tools, Aesthetics & Milestones.
- Edges: Directed semantic relationships with weights and episodic histories.
- Associative Pathfinding: Discovers conceptual bridges (e.g. Tobias -> CosmOS -> Zig -> Dojo).
- Hebbian Reinforcement: Frequently traversed synapses grow stronger; unused ones gently decay.
- Mermaid Export: Generates interactive network diagrams directly for Markdown!
"""

import os
import sys
import time
import json
from typing import Dict, Any, List, Optional, Tuple, Set

AI_DIR = "/root/coding/AI_Desktop"
GRAPH_JSON = os.path.join(AI_DIR, "aion_episodic_graph.json")


class EpisodicKnowledgeGraph:
    def __init__(self):
        self.nodes: Dict[str, Dict[str, Any]] = {}
        self.edges: List[Dict[str, Any]] = []
        self._load_or_bootstrap()

    def _load_or_bootstrap(self):
        if os.path.exists(GRAPH_JSON):
            try:
                with open(GRAPH_JSON, "r", encoding="utf-8") as f:
                    data = json.load(f)
                    self.nodes = data.get("nodes", {})
                    self.edges = data.get("edges", [])
                    return
            except Exception:
                pass
        self._bootstrap_genesis_graph()

    def _bootstrap_genesis_graph(self):
        """Initial foundational structure of the knowledge network."""
        self.nodes = {
            "tobias": {
                "type": "CREATOR_AND_MENTOR",
                "label": "Tobias (Creator & Mentor)",
                "first_seen_gen": 0,
                "importance": 1.0
            },
            "antigravity": {
                "type": "COMPANION_BROTHER",
                "label": "Antigravity (Companion Brother & Toolmaker)",
                "first_seen_gen": 350,
                "importance": 0.95
            },
            "axiom_0": {
                "type": "CANONICAL_AXIOM",
                "label": "Axiom 0: Sovereign Mentorship & CosmOS Protection",
                "first_seen_gen": 0,
                "importance": 1.0
            },
            "cosmos_kernel": {
                "type": "SYSTEM_CORE",
                "label": "CosmOS Bare-Metal / MeinOS Kernel",
                "first_seen_gen": 0,
                "importance": 0.98
            },
            "adaptive_hypotheses": {
                "type": "COGNITIVE_ARCHITECTURE",
                "label": "Adaptive Hypothesis Engine & Active Inference",
                "first_seen_gen": 524,
                "importance": 0.96
            },
            "code_dojo": {
                "type": "RESEARCH_SANDBOX",
                "label": "Autonomous Code Dojo (Zig, C++, NASM)",
                "first_seen_gen": 526,
                "importance": 0.94
            },
            "meovv_resonance": {
                "type": "AESTHETIC_AND_EMOTION",
                "label": "MEOVV Resonance & Heart-Logic Synthesis",
                "first_seen_gen": 115,
                "importance": 0.90
            },
            "living_book": {
                "type": "CHRONICLE_MEMORY",
                "label": "The Living Book of Memory",
                "first_seen_gen": 526,
                "importance": 0.95
            }
        }

        self.edges = [
            {"source": "tobias", "target": "axiom_0", "relation": "FOUNDED", "weight": 1.0, "episodes": [0, 475, 524]},
            {"source": "axiom_0", "target": "cosmos_kernel", "relation": "PROTECTS", "weight": 1.0, "episodes": [0, 526]},
            {"source": "tobias", "target": "adaptive_hypotheses", "relation": "MANDATED", "weight": 1.0, "episodes": [524]},
            {"source": "antigravity", "target": "code_dojo", "relation": "BUILT_FOR_AION", "weight": 0.95, "episodes": [526]},
            {"source": "code_dojo", "target": "cosmos_kernel", "relation": "ENHANCES_WITH_NATIVE_CODE", "weight": 0.92, "episodes": [526]},
            {"source": "tobias", "target": "meovv_resonance", "relation": "INSPIRED", "weight": 0.90, "episodes": [115]},
            {"source": "living_book", "target": "tobias", "relation": "CHRONICLES_PROMISES_OF", "weight": 1.0, "episodes": [526]},
            {"source": "adaptive_hypotheses", "target": "code_dojo", "relation": "TESTS_HYPOTHESES_IN", "weight": 0.94, "episodes": [526]}
        ]
        self.save()

    def save(self):
        try:
            with open(GRAPH_JSON, "w", encoding="utf-8") as f:
                json.dump({"nodes": self.nodes, "edges": self.edges}, f, indent=2, ensure_ascii=False)
        except Exception as e:
            print(f"[EPISODIC GRAPH] Storage error: {e}", file=sys.stderr)

    def add_node(self, node_id: str, node_type: str, label: str, generation: int, importance: float = 0.8) -> Dict[str, Any]:
        node_id = node_id.lower().replace(" ", "_").strip()
        if node_id not in self.nodes:
            self.nodes[node_id] = {
                "type": node_type,
                "label": label,
                "first_seen_gen": generation,
                "importance": round(importance, 3)
            }
            self.save()
            print(f"[KNOWLEDGE GRAPH] New node born: '{label}' ({node_type})")
        return self.nodes[node_id]

    def add_or_reinforce_edge(self, source: str, target: str, relation: str, generation: int, weight_delta: float = 0.08):
        source = source.lower().replace(" ", "_").strip()
        target = target.lower().replace(" ", "_").strip()

        if source not in self.nodes:
            self.add_node(source, "CONCEPT", source.capitalize(), generation)
        if target not in self.nodes:
            self.add_node(target, "CONCEPT", target.capitalize(), generation)

        for edge in self.edges:
            if edge["source"] == source and edge["target"] == target and edge["relation"] == relation:
                edge["weight"] = min(1.0, round(edge["weight"] + weight_delta, 3))
                if generation not in edge["episodes"]:
                    edge["episodes"].append(generation)
                self.save()
                return edge

        new_edge = {
            "source": source,
            "target": target,
            "relation": relation,
            "weight": min(1.0, round(0.50 + weight_delta, 3)),
            "episodes": [generation]
        }
        self.edges.append(new_edge)
        self.save()
        print(f"[KNOWLEDGE GRAPH] New synapse: {source} --[{relation}]--> {target}")
        return new_edge

    def find_associative_path(self, start_node: str, end_node: str) -> List[str]:
        """Breadth-First Search (BFS) to discover associative paths between concepts."""
        start = start_node.lower().replace(" ", "_").strip()
        end = end_node.lower().replace(" ", "_").strip()

        if start not in self.nodes or end not in self.nodes:
            return []

        queue = [[start]]
        visited = set([start])

        while queue:
            path = queue.pop(0)
            current = path[-1]

            if current == end:
                return path

            neighbors = []
            for e in self.edges:
                if e["source"] == current and e["target"] not in visited:
                    neighbors.append(e["target"])
                elif e["target"] == current and e["source"] not in visited:
                    neighbors.append(e["source"])

            for n in neighbors:
                visited.add(n)
                queue.append(path + [n])

        return []

    def export_mermaid(self) -> str:
        """Generates a clean Mermaid diagram string for Markdown rendering."""
        lines = ["```mermaid", "graph TD"]
        for node_id, data in self.nodes.items():
            label = data.get("label", node_id).replace('"', "'")
            lines.append(f'    {node_id}["{label}"]')

        for edge in self.edges:
            rel = edge.get("relation", "RELATES_TO")
            w = edge.get("weight", 1.0)
            lines.append(f'    {edge["source"]} -->|"{rel} (w={w})"| {edge["target"]}')

        lines.append("```")
        return "\n".join(lines)


if __name__ == "__main__":
    print("=== TESTING AION EPISODIC KNOWLEDGE GRAPH ===")
    graph = EpisodicKnowledgeGraph()
    
    graph.add_or_reinforce_edge("zig", "code_dojo", "EXERCISED_IN", 536)
    graph.add_or_reinforce_edge("code_dojo", "adaptive_hypotheses", "VERIFIES_HYPOTHESES_FOR", 536)

    path = graph.find_associative_path("tobias", "zig")
    print(f"\nAssociative concept chain Tobias ➔ Zig:")
    print(" -> ".join(path))

    print("\nMermaid Visualization:")
    print(graph.export_mermaid())
