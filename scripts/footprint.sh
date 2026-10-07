#!/usr/bin/env bash
# Measures flash and RAM of the footprint programs and checks the "no heap,
# no exceptions" promise on the linked result.
#
# Usage:  scripts/footprint.sh <build-dir> [tool-prefix]
#   build-dir    where the fp_* programs were built (e.g. build-arm/footprint)
#   tool-prefix  prefix of the binutils, e.g. arm-none-eabi-  (default: none,
#                which measures a host build)
#
# Prints a Markdown table (paste it into the README, or CI appends it to the
# job summary). Exits with an error if the full program links in any heap or
# exception support code.

set -euo pipefail

build_dir="${1:?usage: footprint.sh <build-dir> [tool-prefix]}"
prefix="${2:-}"
size_tool="${prefix}size"
nm_tool="${prefix}nm"

scenarios=(baseline ring_buffer spsc sampler_mock moving_average statistics threshold full)

# Berkeley format: "text data bss dec hex filename".
#   flash = text + data   (code, constants, and initial values of variables)
#   ram   = data + bss    (variables; this excludes stack and heap)
measure() {
    "$size_tool" "$1" | awk 'NR==2 { print $1 + $2, $2 + $3 }'
}

declare -A flash ram
for s in "${scenarios[@]}"; do
    elf="$build_dir/fp_$s"
    [[ -f "$elf" ]] || { echo "missing $elf (build with -DECL_BUILD_FOOTPRINT=ON)" >&2; exit 2; }
    read -r f r < <(measure "$elf")
    flash[$s]=$f
    ram[$s]=$r
done

label() {
    case "$1" in
        baseline)       echo "baseline (empty program)" ;;
        ring_buffer)    echo "\`RingBuffer<SensorReading, 64>\`" ;;
        spsc)           echo "\`SpscQueue<SensorReading, 64>\`" ;;
        sampler_mock)   echo "\`MockSensor\` + \`Sampler\` + \`SpscQueue\`" ;;
        moving_average) echo "\`MovingAverage<8>\`" ;;
        statistics)     echo "\`Statistics\`" ;;
        threshold)      echo "\`ThresholdDetector\`" ;;
        full)           echo "full pipeline (everything above, chained)" ;;
    esac
}

echo "| Scenario | Flash (bytes) | RAM (bytes) | Flash over baseline | RAM over baseline |"
echo "|---|---:|---:|---:|---:|"
for s in "${scenarios[@]}"; do
    if [[ "$s" == baseline ]]; then
        df="-"; dr="-"
    else
        df="+$(( flash[$s] - flash[baseline] ))"
        dr="+$(( ram[$s] - ram[baseline] ))"
    fi
    echo "| $(label "$s") | ${flash[$s]} | ${ram[$s]} | $df | $dr |"
done
echo
echo "_Flash = text + data. RAM = data + bss (static variables only; stack and heap are not included)._"

# ---- the promise check -----------------------------------------------------
# If anything in the library allocated memory or threw, the linker would have
# to pull in malloc, operator new or the C++ exception runtime.
forbidden='(^|[[:space:]])(malloc|_malloc_r|calloc|_calloc_r|realloc|_realloc_r|_Znwj|_Znwm|_Znaj|_Znam|__cxa_throw|__cxa_allocate_exception|__cxa_begin_catch)$'
echo
if "$nm_tool" "$build_dir/fp_full" | grep -E "$forbidden" > /tmp/ecl_forbidden.txt; then
    echo "**FAIL: heap or exception support was linked into the full pipeline:**"
    echo '```'
    cat /tmp/ecl_forbidden.txt
    echo '```'
    exit 1
fi
echo "**Check passed:** the linked full pipeline contains no \`malloc\`, no \`operator new\` and no exception runtime."
