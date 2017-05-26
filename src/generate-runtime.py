#!/usr/bin/env python3

import os
import subprocess
import sys
import shutil

agent_entrypoint = sys.argv[1]
node_modules = sys.argv[2]
output_js = sys.argv[3]

agent_dir = os.path.dirname(agent_entrypoint)
output_dir = os.path.dirname(output_js)

agent_dir_copy = os.path.join(output_dir, "Agent")
if os.path.exists(agent_dir_copy):
    shutil.rmtree(agent_dir_copy)
shutil.copytree(agent_dir, agent_dir_copy)

compile_args = [
    os.path.relpath(os.path.join(node_modules, ".bin", "frida-compile"), output_dir),
    "Agent",
    "-o", os.path.basename(output_js),
    "-c",
]
exit_code = subprocess.check_call(compile_args, cwd=output_dir)
sys.exit(exit_code)
