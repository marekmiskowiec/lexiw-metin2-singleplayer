#!/bin/sh
# MT2009_CLASSIC_EDITION_V1: the share tree of a MT2009 Classic image (game
# Dockerfile, build arg EDITION=classic), run once after every other share step.
# Classic leaves out the newer places and their people:
#   * the dungeon window lists only the package's own dungeons (no Razador,
#     Nemere, Blue Dragon or Arezzo rows);
#   * the Temple Guardian (20426), the Fire and Ice Land guards (20394, 20395)
#     and Am-heh's statue (20385) are not placed;
#   * the stock quests of the systems Classic has no item for lose their
#     handlers (their state tables stay): the sashes and the alchemy.
# The maps themselves are left out by m2-render-config (M2_EDITION), and no
# item of a left-out system is ever made (server-patches/classic).
set -eu
L=/opt/metin2/share/locale/poland
O="$L/quest/object"

# The dungeon window.
F="$L/dungeon_info.txt"
awk 'BEGIN { split("biblioteka wukong razador skorpion nemere smok dzungla", k, " "); for (i in k) off[k[i]] = 1 }
     { line = $0; sub(/\r$/, "", line); n = split(line, f, /[\t ]+/) }
     n >= 2 && (f[2] in off) && f[1] ~ /^(dungeon|entry|cost|drop)$/ { next }
     { print }' "$F" > "$F.new"
mv "$F.new" "$F"
test "$(grep -c -E '^dungeon[[:blank:]]' "$F")" -ge 2
echo "classic: dungeon window: $(grep -c -E '^dungeon[[:blank:]]' "$F") dungeon(s)"

# The newer places' people.
n=0
for f in $(find "$L/map" -name npc.txt); do
    if grep -q -E '[[:blank:]](20426|20394|20395|20385)[[:space:]]*$' "$f"; then
        sed -i -E '/[[:blank:]](20426|20394|20395|20385)[[:space:]]*$/d' "$f"
        n=$((n + 1))
    fi
done
echo "classic: Ochao and dungeon guards taken out of $n npc file(s)"

# The stock handlers of the sash and alchemy quests.
for q in sash acce_costume_test dragon_soul dragon_soul_refine dragon_soul_shop; do
    find "$O" -path "$O/state" -prune -o -type f -name "$q.*" -exec rm -f {} +
done
echo "classic: share done"
