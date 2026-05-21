#!/bin/bash
# Blocks dangerous commands in Claude Code sessions.
# Exit non-zero to block; exit 0 to allow.

INPUT=$(cat)
CMD=$(echo "$INPUT" | python3 -c "import sys,json; d=json.load(sys.stdin); print(d.get('tool_input',{}).get('command',''))" 2>/dev/null || echo "")

# Block recursive force-deletes from root or workspace root
if echo "$CMD" | grep -qE 'rm\s+-[a-z]*r[a-z]*f|rm\s+-[a-z]*f[a-z]*r'; then
  if echo "$CMD" | grep -qE '(/\s*$|/\s+"|\.\s*$|\*\s*$|bot_ws\s*$)'; then
    echo "BLOCKED: recursive force-delete on broad path" >&2
    exit 1
  fi
fi

# Block force-push to main/master
if echo "$CMD" | grep -qE 'git push.*--force|git push.*-f\b'; then
  if echo "$CMD" | grep -qE 'main|master'; then
    echo "BLOCKED: force-push to main/master" >&2
    exit 1
  fi
fi

# Block git reset --hard without explicit user context
if echo "$CMD" | grep -qE 'git reset --hard origin'; then
  echo "BLOCKED: git reset --hard to remote — confirm this is intentional" >&2
  exit 1
fi

exit 0
