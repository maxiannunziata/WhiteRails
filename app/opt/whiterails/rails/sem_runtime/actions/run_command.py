import subprocess, shlex, sys

def run(action, ctx):
    cmd = action.get("command")
    if not cmd:
        print("[RUN_CMD] missing command", file=sys.stderr)
        return
    subprocess.Popen(cmd, shell=True)
