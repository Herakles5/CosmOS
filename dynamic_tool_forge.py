#!/usr/bin/env python3
"""
AION NEXUS - Dynamic Tool Forge & Sandbox Engine
================================================
Enables Aion to autonomously code custom Python tools and prototypes at runtime,
verify them via TDD in the sandbox (demos/), and dynamically integrate them
into her cognitive execution pipeline without system restarts.
"""

import os
import sys
import ast
import json
import time
import shutil
import inspect
import importlib
import subprocess
from typing import Dict, Any, Optional, Callable, List

DEMOS_DIR = "/root/coding/AI_Desktop/demos"
MEINOS_DEMOS_DIR = "/root/coding/MeinOS/demos"
REGISTRY_FILE = "/root/coding/AI_Desktop/tool_registry.json"


class DynamicToolForge:
    def __init__(self, demos_dir: str = DEMOS_DIR):
        self.demos_dir = demos_dir
        os.makedirs(self.demos_dir, exist_ok=True)
        os.makedirs(MEINOS_DEMOS_DIR, exist_ok=True)
        self.registry: Dict[str, Dict[str, Any]] = self._load_registry()
        self.active_modules: Dict[str, Any] = {}

    def _load_registry(self) -> Dict[str, Dict[str, Any]]:
        if os.path.exists(REGISTRY_FILE):
            try:
                with open(REGISTRY_FILE, "r", encoding="utf-8") as f:
                    return json.load(f)
            except Exception:
                pass
        return {}

    def _save_registry(self):
        try:
            with open(REGISTRY_FILE, "w", encoding="utf-8") as f:
                json.dump(self.registry, f, indent=2, ensure_ascii=False)
        except Exception:
            pass

    def should_synthesize_tool(self, task: str) -> bool:
        """Determines whether a task requires synthesizing a real software tool."""
        keywords = [
            "tool", "script", "prototype", "sandbox", 
            "monitor", "benchmark", "scanner", "demos/", "implement", 
            "code", "module", "algorithm", "develop", "forge"
        ]
        lower = task.lower()
        return any(k in lower for k in keywords)

    def synthesize_tool(self, task_description: str, llm_caller: Callable[[str, str, float], str]) -> Dict[str, Any]:
        """
        Synthesizes a production-ready, tested Python module at runtime.
        Returns metrics, test outputs, and registered module metadata.
        """
        system_prompt = (
            "You are AION NEXUS Autonomous Tool Forge.\n"
            "Your task is to write a production-ready, clean, standalone Python 3 module that solves the given task.\n"
            "RULES:\n"
            "1. Output valid JSON ONLY in this format:\n"
            "{\n"
            "  \"tool_name\": \"lowercase_snake_case_name\",\n"
            "  \"code\": \"complete python code with functions and docstrings\",\n"
            "  \"test_code\": \"python test code using assert statements to verify the tool's behavior\"\n"
            "}\n"
            "2. Preservation of Axiom 0 (Sovereign Mentorship): The tool must NOT delete or overwrite system files.\n"
            "3. The test_code must invoke the main functions and verify correct output using assert statements.\n"
            "4. Keep code efficient, robust, and dependency-light."
        )
        user_prompt = f"Design and implement a dynamic Python tool for this requirement:\n'{task_description}'"

        raw_reply = llm_caller(system_prompt, user_prompt, 0.2)
        if not raw_reply:
            return {"success": False, "error": "LLM returned empty reply"}

        try:
            clean = raw_reply.replace("```json", "").replace("```", "").strip()
            data = json.loads(clean)
            tool_name = data.get("tool_name", f"tool_{int(time.time())}").strip().replace("-", "_")
            code = data.get("code", "").strip()
            test_code = data.get("test_code", "").strip()
        except Exception as e:
            return {"success": False, "error": f"JSON parse failed: {e}", "raw": raw_reply[:200]}

        # 1. AST Syntax Validation
        try:
            ast.parse(code)
            ast.parse(test_code)
        except SyntaxError as e:
            return {"success": False, "error": f"AST SyntaxError: {e}"}

        # 2. Persist to sandbox
        tool_file = os.path.join(self.demos_dir, f"{tool_name}.py")
        test_file = os.path.join(self.demos_dir, f"test_{tool_name}.py")

        with open(tool_file, "w", encoding="utf-8") as f:
            f.write(code)

        # Generate full test runner importing the tool
        full_test = (
            f"import sys, os\n"
            f"sys.path.insert(0, '{self.demos_dir}')\n"
            f"from {tool_name} import *\n\n"
            f"{test_code}\n"
            f"print('[TEST_PASSED] All assertions verified successfully.')\n"
        )
        with open(test_file, "w", encoding="utf-8") as f:
            f.write(full_test)

        # 3. TDD execution in isolated subprocess with timeout
        start_time = time.time()
        try:
            proc = subprocess.run(
                [sys.executable, test_file],
                capture_output=True,
                text=True,
                timeout=6.0,
                cwd=self.demos_dir
            )
            elapsed_ms = (time.time() - start_time) * 1000
            test_stdout = proc.stdout.strip()
            test_stderr = proc.stderr.strip()

            if proc.returncode == 0 and "[TEST_PASSED]" in test_stdout:
                # Mirror to MeinOS demos
                try:
                    shutil.copy2(tool_file, os.path.join(MEINOS_DEMOS_DIR, f"{tool_name}.py"))
                except Exception:
                    pass

                # Register in tool registry
                self.registry[tool_name] = {
                    "task": task_description,
                    "file": tool_file,
                    "created_at": time.strftime("%Y-%m-%d %H:%M:%S"),
                    "execution_time_ms": round(elapsed_ms, 2),
                    "status": "VERIFIED"
                }
                self._save_registry()

                print(f"[TOOL FORGE] Tool '{tool_name}' successfully synthesized & verified via TDD ({elapsed_ms:.1f}ms)!")
                return {
                    "success": True,
                    "tool_name": tool_name,
                    "file": tool_file,
                    "test_output": test_stdout,
                    "latency_ms": elapsed_ms,
                    "code_summary": code[:120] + "..."
                }
            else:
                return {
                    "success": False,
                    "error": f"TDD Test Failed (code {proc.returncode}): {test_stderr or test_stdout}",
                    "file": tool_file
                }
        except subprocess.TimeoutExpired:
            return {"success": False, "error": "TDD Sandbox Execution Timed Out (>6s)"}
        except Exception as e:
            return {"success": False, "error": f"Execution error: {e}"}

    def list_active_tools(self) -> List[str]:
        return list(self.registry.keys())
