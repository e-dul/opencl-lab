#!/usr/bin/env bash
# progress.sh — project progress tracker
# Reads workflow/design/*.md and counts [x] vs [ ] task items

DESIGN_DIR="$(dirname "$0")/../workflow/design"

declare -A MODULE_DONE
declare -A MODULE_TOTAL
declare -A MODULE_TITLE

total_done=0
total_all=0

for file in "$DESIGN_DIR"/*.md; do
    name=$(basename "$file" .md)
    title=$(grep -m1 "^#" "$file" | sed 's/^# //')
    done=$(grep -E "^- \[x\]" "$file" | wc -l)
    todo=$(grep -E "^- \[ \]" "$file" | wc -l)
    all=$((done + todo))

    MODULE_TITLE[$name]="$title"
    MODULE_DONE[$name]=$done
    MODULE_TOTAL[$name]=$all

    total_done=$((total_done + done))
    total_all=$((total_all + all))
done

total_pct=0
[ "$total_all" -gt 0 ] && total_pct=$(( total_done * 100 / total_all ))

bar() {
    local pct=$1 width=20
    local filled=$(( pct * width / 100 ))
    local empty=$(( width - filled ))
    local b='['
    for ((i=0; i<filled; i++)); do b+='#'; done
    for ((i=0; i<empty; i++)); do b+='.'; done
    b+=']'
    printf '%s' "$b"
}

printf '\n\033[1mApplied OpenCL Lab — Progress Report\033[0m\n'
printf '%-55s  %6s  %s\n' "Module" "Tasks" "Progress"
printf '%s\n' "────────────────────────────────────────────────────────────────────────────────"

for name in $(ls "$DESIGN_DIR"/*.md | xargs -n1 basename | sed 's/\.md//' | sort); do
    done=${MODULE_DONE[$name]}
    all=${MODULE_TOTAL[$name]}
    title=${MODULE_TITLE[$name]}
    [ "$all" -eq 0 ] && continue

    pct=$(( done * 100 / all ))

    if   [ "$pct" -eq 100 ]; then status="\033[32m✓\033[0m"
    elif [ "$pct" -gt 0 ];   then status="\033[33m~\033[0m"
    else                          status="\033[31m✗\033[0m"
    fi

    printf " %b %-53s  %2d/%-2d  %s %3d%%\n" \
        "$status" "$title" "$done" "$all" "$(bar $pct)" "$pct"
done

printf '%s\n' "────────────────────────────────────────────────────────────────────────────────"
printf ' \033[1mTOTAL: %d/%d tasks done\033[0m  %s \033[1m%d%%\033[0m\n\n' \
    "$total_done" "$total_all" "$(bar $total_pct)" "$total_pct"
