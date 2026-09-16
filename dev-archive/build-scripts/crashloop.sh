#!/usr/bin/env bash
# Automated bring-up loop for The Darkness recompilation.
#   run -> read log -> register the missing function -> codegen -> build -> deploy -> repeat
# Stops early on: success (survives the timeout), a non-function failure, or no progress.

PROJ="E:/the-darkness/src/DarknessRecomp"
OUT="E:/the-darkness/build-local"
BUILT="$PROJ/out/build/win-amd64-release"
CFG="$PROJ/darknessrecomp_config.toml"
REX="E:/condemned-2-vr/src/rexglue-sdk/out/win-amd64/Release/rexglue.exe"
BAT="C:/Users/Tefa/AppData/Local/Temp/claude/D--Program-Files--x86--Steam-steamapps-common/433c5e9a-8719-4657-add8-0d486cd69aab/scratchpad/build_darkness.bat"
MAX=${1:-12}
RUNSEC=${2:-30}

deploy() {
  cp -f "$BUILT/darknessrecomp.exe" "$OUT/" 2>/dev/null || return 1
  [ -f "$BUILT/rexruntime.dll" ] && cp -f "$BUILT/rexruntime.dll" "$OUT/"
  [ -f "$BUILT/rexgpu-xenos.dll" ] && cp -f "$BUILT/rexgpu-xenos.dll" "$OUT/"
  return 0
}

deploy || { echo "FATAL: nothing built to deploy"; exit 1; }
echo "### deployed $(sha256sum "$OUT/darknessrecomp.exe" | cut -c1-12)"

prev_addr=""
for i in $(seq 1 "$MAX"); do
  echo
  echo "================ ITERATION $i ================"
  before=$(ls -t "$OUT/logs"/*.log 2>/dev/null | head -1)

  ( cd "$OUT" && timeout "$RUNSEC" ./darknessrecomp.exe >/dev/null 2>&1 )
  rc=$?
  # make sure nothing lingers
  taskkill //F //IM darknessrecomp.exe >/dev/null 2>&1

  if [ "$rc" -eq 124 ]; then
    echo "⭐ SURVIVED ${RUNSEC}s WITHOUT EXITING - this is the win condition. Stopping."
    echo "last log: $(ls -t "$OUT/logs"/*.log | head -1)"
    exit 0
  fi

  log=$(ls -t "$OUT/logs"/*.log 2>/dev/null | head -1)
  if [ -z "$log" ] || [ "$log" = "$before" ]; then
    echo "NO NEW LOG (exit code $rc) - the program died before it could log. Stopping."
    exit 2
  fi
  echo "exit=$rc log=$(basename "$log")"

  addr=$(grep -oE "unregistered function at guest address 0x[0-9A-Fa-f]+" "$log" | tail -1 | grep -oE "0x[0-9A-Fa-f]+")
  if [ -z "$addr" ]; then
    echo "### NOT A MISSING-FUNCTION FAILURE. Last 12 significant lines:"
    grep -E "critical|error|FATAL" "$log" | tail -12
    echo "Stopping - this needs thought, not another line of config."
    exit 3
  fi

  if [ "$addr" = "$prev_addr" ]; then
    echo "### NO PROGRESS: died at $addr twice running. Stopping."
    exit 4
  fi
  prev_addr="$addr"
  echo "missing function: $addr -> registering"

  printf '\n# Auto-registered by the bring-up loop, iteration %s.\n%s = { name = "sub_%s" }\n' \
    "$i" "$addr" "$(echo "$addr" | sed 's/^0x//')" >> "$CFG"

  ( cd "$PROJ" && "$REX" codegen ) 2>&1 | tail -2
  cmd //c "$(cygpath -w "$BAT")" > /tmp/bl_$i.txt 2>&1
  if ! grep -q "build errorlevel: 0" /tmp/bl_$i.txt; then
    echo "### BUILD FAILED after registering $addr:"
    grep -E "error:|FAILED" /tmp/bl_$i.txt | head -6
    exit 5
  fi
  deploy && echo "rebuilt + deployed $(sha256sum "$OUT/darknessrecomp.exe" | cut -c1-12)"
done

echo
echo "### Reached the $MAX-iteration cap, still failing on missing functions."
echo "Addresses registered so far:"
grep -cE "^0x[0-9A-Fa-f]+ = " "$CFG"
