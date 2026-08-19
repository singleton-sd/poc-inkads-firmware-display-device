#!/usr/bin/env bash
# Validate targets.json and resolve compile metadata.
# Requires jq. Subcommands: validate, matrix, list, resolve, write-features.

set -euo pipefail

die() { echo "error: $*" >&2; exit 1; }

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
CATALOG_PATH="${INKADS_CATALOG:-$REPO_ROOT/targets.json}"
VERSION_PATH="${INKADS_VERSION_FILE:-$REPO_ROOT/version.json}"

SLUG_RE='^[a-z0-9]+(-[a-z0-9]+)*$'
PARTITION_RE='^[a-z0-9]+([_-][a-z0-9]+)*$'
VERSION_RE='^[0-9]+\.[0-9]+\.[0-9]+$'

need_jq() {
  command -v jq >/dev/null || die 'jq is required to read targets.json'
}

json_type() {
  jq -er "$1 | type" "$2"
}

load_version() {
  [[ -f "$VERSION_PATH" ]] || die "missing version file: $VERSION_PATH"
  local version
  version="$(jq -er '.version' "$VERSION_PATH")" || die "version.json must contain string .version"
  [[ "$version" =~ $VERSION_RE ]] || die "version.json version must be major.minor.patch: $version"
  printf '%s' "$version"
}

feature_macro() {
  printf 'INKADS_FEATURE_%s' "$(printf '%s' "$1" | tr '[:lower:]' '[:upper:]')"
}

validate_catalog() {
  need_jq
  [[ -f "$CATALOG_PATH" ]] || die "missing catalog: $CATALOG_PATH"
  [[ "$(json_type '.' "$CATALOG_PATH")" == object ]] || die 'catalog must be a JSON object'

  local version
  version="$(load_version)"

  jq -e '.features | type == "array" and length > 0' "$CATALOG_PATH" >/dev/null \
    || die 'catalog.features must be a non-empty array'
  jq -e '.boards | type == "array" and length > 0' "$CATALOG_PATH" >/dev/null \
    || die 'catalog.boards must be a non-empty array'
  jq -e '.targets | type == "array" and length > 0' "$CATALOG_PATH" >/dev/null \
    || die 'catalog.targets must be a non-empty array'

  local feature
  while IFS= read -r feature; do
    [[ "$feature" =~ $PARTITION_RE ]] || die "invalid feature name: $feature"
  done < <(jq -er '.features[]' "$CATALOG_PATH")

  local board_ids=()
  local filenames=()
  local board_id display filename fqbn_type
  while IFS= read -r board_id; do
    [[ "$board_id" =~ $SLUG_RE ]] || die "invalid board id: $board_id"
    for existing in "${board_ids[@]+"${board_ids[@]}"}"; do
      [[ "$existing" != "$board_id" ]] || die "duplicate board id: $board_id"
    done
    board_ids+=("$board_id")

    display="$(jq -er --arg id "$board_id" '.boards[] | select(.id == $id) | .displayName' "$CATALOG_PATH")"
    [[ -n "$display" ]] || die "board $board_id is missing displayName"

    filename="$(jq -er --arg id "$board_id" '.boards[] | select(.id == $id) | .filename' "$CATALOG_PATH")"
    [[ "$filename" =~ $SLUG_RE ]] || die "invalid board filename: $filename"
    for existing in "${filenames[@]+"${filenames[@]}"}"; do
      [[ "$existing" != "$filename" ]] || die "duplicate board filename: $filename"
    done
    filenames+=("$filename")

    fqbn_type="$(jq -er --arg id "$board_id" '.boards[] | select(.id == $id) | .fqbn | type' "$CATALOG_PATH")"
    [[ "$fqbn_type" == string ]] || die "board $board_id fqbn must be a string, not $fqbn_type"
    jq -er --arg id "$board_id" '.boards[] | select(.id == $id) | .fqbn | length > 0' "$CATALOG_PATH" >/dev/null \
      || die "board $board_id fqbn must be non-empty"
  done < <(jq -er '.boards[].id' "$CATALOG_PATH")

  local pairs=()
  local board suffix partition
  while IFS= read -r row; do
    board="$(printf '%s' "$row" | jq -er '.board')"
    suffix="$(printf '%s' "$row" | jq -er '.suffix')"
    jq -e --arg id "$board" '.boards[] | select(.id == $id)' "$CATALOG_PATH" >/dev/null \
      || die "target board is not in catalog.boards: $board"
    [[ "$suffix" =~ $SLUG_RE ]] || die "invalid or missing target suffix: $suffix"

    partition="$(printf '%s' "$row" | jq -er '.partitionScheme')"
    [[ "$partition" =~ $PARTITION_RE ]] || die "invalid partitionScheme: $partition"

    local pair="$board|$suffix"
    for existing in "${pairs[@]+"${pairs[@]}"}"; do
      [[ "$existing" != "$pair" ]] || die "duplicate target board+suffix: $board $suffix"
    done
    pairs+=("$pair")

    local has_https=0 has_entra=0 has_ota=0
    local target_feature
    while IFS= read -r target_feature; do
      jq -e --arg name "$target_feature" '.features | index($name) != null' "$CATALOG_PATH" >/dev/null \
        || die "unknown feature in target $board/$suffix: $target_feature"
      case "$target_feature" in
        https_admin) has_https=1 ;;
        entra) has_entra=1 ;;
        ota) has_ota=1 ;;
      esac
    done < <(printf '%s' "$row" | jq -er '.features[]')

    if [[ "$has_entra" -eq 1 && "$has_https" -eq 0 ]]; then
      die "target $board/$suffix: entra requires https_admin"
    fi
    if [[ "$has_ota" -eq 1 && "$has_https" -eq 0 ]]; then
      die "target $board/$suffix: ota requires https_admin"
    fi
    if [[ "$has_https" -eq 1 && "$has_entra" -eq 0 ]]; then
      die "target $board/$suffix: https_admin requires entra"
    fi
  done < <(jq -c '.targets[]' "$CATALOG_PATH")

  printf 'catalog ok (version %s)\n' "$version" >&2
}

board_filename() {
  local board_id="$1"
  jq -er --arg id "$board_id" '.boards[] | select(.id == $id) | .filename' "$CATALOG_PATH"
}

board_fqbn() {
  local board_id="$1"
  jq -er --arg id "$board_id" '.boards[] | select(.id == $id) | .fqbn' "$CATALOG_PATH"
}

resolved_fqbn() {
  local board_id="$1"
  local partition="$2"
  local fqbn
  fqbn="$(board_fqbn "$board_id")"
  if [[ "$partition" == default ]]; then
    printf '%s' "$fqbn"
    return
  fi
  printf '%s:PartitionScheme=%s' "$fqbn" "$partition"
}

target_id_for() {
  local filename="$1"
  local suffix="$2"
  printf '%s-%s' "$filename" "$suffix"
}

artifact_stem_for() {
  local filename="$1"
  local suffix="$2"
  local version="$3"
  printf 'inkads-%s-%s-v%s' "$filename" "$suffix" "$version"
}

resolve_target_json() {
  need_jq
  validate_catalog >/dev/null
  local wanted="${1:-}"
  [[ -n "$wanted" ]] || die 'resolve requires a target id (filename-suffix)'
  local version
  version="$(load_version)"

  local row board suffix filename partition fqbn target_id stem
  while IFS= read -r row; do
    board="$(printf '%s' "$row" | jq -er '.board')"
    suffix="$(printf '%s' "$row" | jq -er '.suffix')"
    filename="$(board_filename "$board")"
    target_id="$(target_id_for "$filename" "$suffix")"
    [[ "$target_id" == "$wanted" ]] || continue
    partition="$(printf '%s' "$row" | jq -er '.partitionScheme')"
    fqbn="$(resolved_fqbn "$board" "$partition")"
    stem="$(artifact_stem_for "$filename" "$suffix" "$version")"
    jq -nc --arg id "$target_id" \
      --arg board "$board" \
      --arg suffix "$suffix" \
      --arg filename "$filename" \
      --arg fqbn "$fqbn" \
      --arg partition "$partition" \
      --arg version "$version" \
      --arg stem "$stem" \
      --argjson features "$(printf '%s' "$row" | jq -c '.features')" \
      '{
        id: $id,
        board: $board,
        suffix: $suffix,
        filename: $filename,
        fqbn: $fqbn,
        partitionScheme: $partition,
        version: $version,
        artifactStem: $stem,
        features: $features
      }'
    return 0
  done < <(jq -c '.targets[]' "$CATALOG_PATH")
  die "unknown target id: $wanted"
}

list_target_ids() {
  need_jq
  validate_catalog >/dev/null
  local row board suffix filename
  while IFS= read -r row; do
    board="$(printf '%s' "$row" | jq -er '.board')"
    suffix="$(printf '%s' "$row" | jq -er '.suffix')"
    filename="$(board_filename "$board")"
    printf '%s\n' "$(target_id_for "$filename" "$suffix")"
  done < <(jq -c '.targets[]' "$CATALOG_PATH")
}

emit_matrix() {
  need_jq
  validate_catalog >/dev/null
  local version
  version="$(load_version)"
  jq -c --arg version "$version" '
    . as $root
    | {
        include: [
          .targets[] as $t
          | ($root.boards[] | select(.id == $t.board)) as $b
          | {
              id: ($b.filename + "-" + $t.suffix),
              board: $t.board,
              suffix: $t.suffix,
              filename: $b.filename,
              fqbn: (if $t.partitionScheme == "default" then $b.fqbn else ($b.fqbn + ":PartitionScheme=" + $t.partitionScheme) end),
              partitionScheme: $t.partitionScheme,
              version: $version,
              artifactStem: ("inkads-" + $b.filename + "-" + $t.suffix + "-v" + $version)
            }
        ]
      }
  ' "$CATALOG_PATH"
}

write_features_header() {
  local target_id="$1"
  local out_path="$2"
  [[ -n "$target_id" && -n "$out_path" ]] || die 'write-features requires <target-id> <output-path>'
  local resolved feature enabled macro
  resolved="$(resolve_target_json "$target_id")"
  {
    printf '#pragma once\n'
    printf '#define INKADS_TARGET_ID "%s"\n' "$(printf '%s' "$resolved" | jq -er '.id')"
    while IFS= read -r feature; do
      enabled="$(printf '%s' "$resolved" | jq -er --arg name "$feature" '.features | index($name) | if . == null then 0 else 1 end')"
      macro="$(feature_macro "$feature")"
      printf '#define %s %s\n' "$macro" "$enabled"
    done < <(jq -er '.features[]' "$CATALOG_PATH")
  } >"$out_path"
}

usage() {
  cat <<'EOF'
Usage: targets-catalog.sh <command>

Commands:
  validate                         Check targets.json and version.json
  matrix                           Print GitHub Actions matrix JSON
  list                             Print target ids
  resolve <target-id>              Print one resolved target as JSON
  write-features <id> <path>       Write InkAdsFeatures.h for a target
EOF
}

cmd="${1:-}"
shift || true
case "$cmd" in
  validate)
    need_jq
    validate_catalog
    ;;
  matrix)
    need_jq
    emit_matrix
    ;;
  list)
    need_jq
    list_target_ids
    ;;
  resolve)
    need_jq
    [[ $# -ge 1 ]] || die 'resolve requires a target id'
    resolve_target_json "$1"
    printf '\n'
    ;;
  write-features)
    need_jq
    [[ $# -ge 2 ]] || die 'write-features requires <target-id> <output-path>'
    write_features_header "$1" "$2"
    ;;
  -h|--help|'')
    usage
    [[ -n "$cmd" ]] || exit 1
    ;;
  *) die "unknown command: $cmd" ;;
esac
