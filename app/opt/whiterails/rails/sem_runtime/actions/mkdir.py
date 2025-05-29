import os, sys

def run(action, ctx):
    path = action.get("path")
    if not path:
        print("[MKDIR] missing path", file=sys.stderr)
        return
    try:
        os.makedirs(path, exist_ok=True)
        print(f"[MKDIR] created {path}")
    except OSError as e:
        print(f"[ERR][MKDIR] {e}", file=sys.stderr)
