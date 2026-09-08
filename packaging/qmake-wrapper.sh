#!/usr/bin/env bash
set -euo pipefail

: "${DCM_REAL_QMAKE:?DCM_REAL_QMAKE must name the real qmake executable}"
: "${DCM_QT_PLUGINS_DIR:?DCM_QT_PLUGINS_DIR must name the staged plugin directory}"

if [[ "${1:-}" == "-query" && $# -eq 1 ]]; then
    while IFS= read -r line; do
        if [[ "$line" == QT_INSTALL_PLUGINS:* ]]; then
            printf 'QT_INSTALL_PLUGINS:%s\n' "$DCM_QT_PLUGINS_DIR"
        else
            printf '%s\n' "$line"
        fi
    done < <("$DCM_REAL_QMAKE" -query)
elif [[ "${1:-}" == "-query" && "${2:-}" == "QT_INSTALL_PLUGINS" ]]; then
    printf '%s\n' "$DCM_QT_PLUGINS_DIR"
else
    exec "$DCM_REAL_QMAKE" "$@"
fi
