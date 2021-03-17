#!/usr/bin/env python3

import glob
import os
import subprocess
import sys
import shutil

def append_to_agent(agent, script):
	with open(agent, "a+") as out:
		with open(script, "r") as sc:
			out.write(sc.read())

agent_entrypoint = sys.argv[1]
node_modules = sys.argv[2]
standard_library = sys.argv[3]
external_scripts = sys.argv[4]
output_js = sys.argv[5]
output_standard_library = sys.argv[6]

for script_file in glob.glob(external_scripts + "*.js"):
	append_to_agent(agent_entrypoint, script_file)

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
if exit_code != 0:
    sys.exit(exit_code)

if os.path.exists(output_standard_library):
    shutil.rmtree(output_standard_library)
shutil.copytree(standard_library, output_standard_library)
