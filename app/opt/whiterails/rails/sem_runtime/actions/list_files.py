import subprocess, shlex, sys

def run(action, ctx):
    target = action.get("path", "/")
    try:
        out = subprocess.check_output(["ls", "-la", target]).decode()
        print(f"[LIST_FILES] {target}\n{out}")
    except subprocess.CalledProcessError as e:
        print(f"[ERR][LIST_FILES] {e}", file=sys.stderr)
