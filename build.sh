#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
build_dir="${DCM_BUILD_DIR:-$project_dir/build}"
build_type="${DCM_BUILD_TYPE:-Release}"
install_prefix="${DCM_INSTALL_PREFIX:-$HOME/.local}"

usage() {
    cat <<'EOF'
Usage: ./build.sh [command] [arguments]

Commands:
  build       Build the application (default)
  run         Build, then launch the application
  install     Build, then install for the current user
  release     Build and test the Linux binary and AppImage release
  clean       Remove compiled outputs while retaining CMake configuration
  help        Show this help

Optional environment variables:
  DCM_BUILD_DIR       Build directory (default: ./build)
  DCM_BUILD_TYPE      CMake build type (default: Release)
  DCM_INSTALL_PREFIX  Install prefix (default: $HOME/.local)

Arguments after "run" are passed to Dolphin Context Menu Manager.
EOF
}

configure() {
    local generator_args=()
    if [[ ! -f "$build_dir/CMakeCache.txt" ]] && command -v ninja >/dev/null 2>&1; then
        generator_args=(-G Ninja)
    fi

    cmake \
        -S "$project_dir" \
        -B "$build_dir" \
        "${generator_args[@]}" \
        -DCMAKE_BUILD_TYPE="$build_type"
}

ensure_configured() {
    if [[ ! -f "$build_dir/CMakeCache.txt" ]]; then
        configure
    fi
}

build() {
    ensure_configured
    cmake --build "$build_dir" --parallel
}

command_name="${1:-build}"
if (( $# > 0 )); then
    shift
fi

case "$command_name" in
    build)
        build
        ;;
    run)
        build
        exec "$build_dir/dolphin-context-menu-manager" "$@"
        ;;
    test)
        build
        ctest --test-dir "$build_dir" --output-on-failure "$@"
        ;;
    install)
        build
        cmake --install "$build_dir" --prefix "$install_prefix" "$@"
        ;;
    release)
        exec "$project_dir/packaging/release-linux.sh" "$@"
        ;;
    configure)
        configure
        ;;
    clean)
        ensure_configured
        cmake --build "$build_dir" --target clean
        ;;
    rebuild)
        ensure_configured
        cmake --build "$build_dir" --target clean
        build
        ;;
    help|-h|--help)
        usage
        ;;
    *)
        printf 'Unknown command: %s\n\n' "$command_name" >&2
        usage >&2
        exit 2
        ;;
esac
