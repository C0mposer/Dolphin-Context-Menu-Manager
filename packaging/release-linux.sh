#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
release_build_dir="${DCM_RELEASE_BUILD_DIR:-$project_dir/build-release}"
tools_dir="${DCM_PACKAGING_TOOLS_DIR:-$project_dir/.cache/appimage-tools}"
dist_dir="$project_dir/dist"
linux_dist_dir="$dist_dir/Linux"
appimage_dist_dir="$dist_dir/Linux-AppImage"
appdir="$release_build_dir/AppDir"
output_dir="$release_build_dir/appimage-output"
qt_plugins_staging="$release_build_dir/qt-plugins"
icon_file="$project_dir/data/io.github.dolphincontextmenumanager.svg"
desktop_file="$project_dir/data/io.github.dolphincontextmenumanager.desktop"
qmake_wrapper="$project_dir/packaging/qmake-wrapper.sh"

linuxdeploy_version="1-alpha-20251107-1"
qt_plugin_version="1-alpha-20250213-1"
appimage_plugin_version="1-alpha-20250213-1"

linuxdeploy="$tools_dir/linuxdeploy-x86_64.AppImage"
qt_plugin="$tools_dir/linuxdeploy-plugin-qt-x86_64.AppImage"
appimage_plugin="$tools_dir/linuxdeploy-plugin-appimage-x86_64.AppImage"

download_tool() {
    local destination="$1"
    local url="$2"
    local expected_sha256="$3"
    local temporary_file="${destination}.download"

    if [[ -f "$destination" ]] \
        && printf '%s  %s\n' "$expected_sha256" "$destination" | sha256sum --check --status; then
        chmod +x "$destination"
        return
    fi

    rm -f -- "$temporary_file"
    printf 'Downloading %s\n' "$(basename -- "$destination")"
    curl --fail --location --retry 3 --output "$temporary_file" "$url"
    printf '%s  %s\n' "$expected_sha256" "$temporary_file" | sha256sum --check --status
    mv -- "$temporary_file" "$destination"
    chmod +x "$destination"
}

if [[ "$(uname -m)" != "x86_64" ]]; then
    printf 'This release script currently targets x86_64 Linux only.\n' >&2
    exit 1
fi

for required_tool in cmake curl sha256sum strip; do
    if ! command -v "$required_tool" >/dev/null 2>&1; then
        printf 'Required tool was not found: %s\n' "$required_tool" >&2
        exit 1
    fi
done

generator_args=()
if [[ ! -f "$release_build_dir/CMakeCache.txt" ]] && command -v ninja >/dev/null 2>&1; then
    generator_args=(-G Ninja)
fi

cmake \
    -S "$project_dir" \
    -B "$release_build_dir" \
    "${generator_args[@]}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr
cmake --build "$release_build_dir" --parallel
ctest --test-dir "$release_build_dir" --output-on-failure

release_binary="$release_build_dir/dolphin-context-menu-manager"
version_output="$("$release_binary" --version)"
version="${version_output##* }"
appimage_name="Dolphin_Context_Menu_Manager-${version}-x86_64.AppImage"

# These are deliberately exact, project-owned release directories.
rm -rf -- "$linux_dist_dir" "$appimage_dist_dir" "$appdir" "$output_dir" "$qt_plugins_staging"
mkdir -p "$linux_dist_dir" "$appimage_dist_dir" "$output_dir" "$tools_dir"

install -m 0755 "$release_binary" "$linux_dist_dir/dolphin-context-menu-manager"
strip --strip-unneeded "$linux_dist_dir/dolphin-context-menu-manager"

DESTDIR="$appdir" cmake --install "$release_build_dir" --prefix /usr

download_tool \
    "$linuxdeploy" \
    "https://github.com/linuxdeploy/linuxdeploy/releases/download/${linuxdeploy_version}/linuxdeploy-x86_64.AppImage" \
    "c20cd71e3a4e3b80c3483cef793cda3f4e990aca14014d23c544ca3ce1270b4d"
download_tool \
    "$qt_plugin" \
    "https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/${qt_plugin_version}/linuxdeploy-plugin-qt-x86_64.AppImage" \
    "15106be885c1c48a021198e7e1e9a48ce9d02a86dd0a1848f00bdbf3c1c92724"
download_tool \
    "$appimage_plugin" \
    "https://github.com/linuxdeploy/linuxdeploy-plugin-appimage/releases/download/${appimage_plugin_version}/linuxdeploy-plugin-appimage-x86_64.AppImage" \
    "992d502a248e14ab185448ddf6f6e7d25558cb84d4623c354c3af350c25fccb3"

qmake_command="$(command -v qmake6 || command -v qmake || true)"
if [[ -z "$qmake_command" ]]; then
    printf 'qmake6 is required by the Qt deployment plugin.\n' >&2
    exit 1
fi

qt_plugins_dir="$("$qmake_command" -query QT_INSTALL_PLUGINS)"
extra_platform_plugins=()
for candidate in libqwayland.so libqwayland-egl.so libqwayland-generic.so libqoffscreen.so; do
    if [[ -f "$qt_plugins_dir/platforms/$candidate" ]]; then
        extra_platform_plugins+=("$candidate")
    fi
done

stage_qt_plugin() {
    local relative_path="$1"
    local source="$qt_plugins_dir/$relative_path"
    local destination="$qt_plugins_staging/$relative_path"

    if [[ -f "$source" ]]; then
        mkdir -p "$(dirname -- "$destination")"
        ln -s "$source" "$destination"
    fi
}

# Keep the bundle focused: a desktop widget app needs the display backends,
# basic input handling, and only the image/icon formats used by its UI.
for plugin in \
    platforms/libqxcb.so \
    platforminputcontexts/libcomposeplatforminputcontextplugin.so \
    platforminputcontexts/libibusplatforminputcontextplugin.so \
    imageformats/libqsvg.so \
    imageformats/libqico.so \
    imageformats/libqjpeg.so \
    iconengines/libqsvgicon.so \
    xcbglintegrations/libqxcb-egl-integration.so \
    xcbglintegrations/libqxcb-glx-integration.so; do
    stage_qt_plugin "$plugin"
done
for plugin in "${extra_platform_plugins[@]}"; do
    stage_qt_plugin "platforms/$plugin"
done
for plugin_dir in wayland-decoration-client wayland-graphics-integration-client wayland-shell-integration; do
    if [[ -d "$qt_plugins_dir/$plugin_dir" ]]; then
        while IFS= read -r plugin; do
            stage_qt_plugin "$plugin_dir/$plugin"
        done < <(find "$qt_plugins_dir/$plugin_dir" -maxdepth 1 -type f -printf '%f\n' | sort)
    fi
done

export APPIMAGE_EXTRACT_AND_RUN=1
export ARCH=x86_64
export DCM_REAL_QMAKE="$qmake_command"
export DCM_QT_PLUGINS_DIR="$qt_plugins_staging"
export QMAKE="$qmake_wrapper"
export NO_STRIP=1
export LDAI_OUTPUT="$output_dir/$appimage_name"
if (( ${#extra_platform_plugins[@]} > 0 )); then
    export EXTRA_PLATFORM_PLUGINS="$(IFS=';'; printf '%s' "${extra_platform_plugins[*]}")"
fi

"$linuxdeploy" \
    --appdir "$appdir" \
    --executable "$appdir/usr/bin/dolphin-context-menu-manager" \
    --desktop-file "$desktop_file" \
    --icon-file "$icon_file" \
    --plugin qt \
    --output appimage

if [[ ! -f "$LDAI_OUTPUT" ]]; then
    printf 'AppImage tool did not create the expected output: %s\n' "$LDAI_OUTPUT" >&2
    exit 1
fi
install -m 0755 "$LDAI_OUTPUT" "$appimage_dist_dir/$appimage_name"

plain_count="$(find "$linux_dist_dir" -maxdepth 1 -type f | wc -l)"
appimage_count="$(find "$appimage_dist_dir" -maxdepth 1 -type f | wc -l)"
if [[ "$plain_count" != "1" || "$appimage_count" != "1" ]]; then
    printf 'Release directories contain unexpected files.\n' >&2
    exit 1
fi

APPIMAGE_EXTRACT_AND_RUN=1 "$appimage_dist_dir/$appimage_name" --version
QT_QPA_PLATFORM=offscreen APPIMAGE_EXTRACT_AND_RUN=1 \
    "$appimage_dist_dir/$appimage_name" \
    --screenshot "$release_build_dir/appimage-smoke.png"
if [[ ! -s "$release_build_dir/appimage-smoke.png" ]]; then
    printf 'AppImage GUI smoke test did not produce a screenshot.\n' >&2
    exit 1
fi

printf '\nRelease artifacts:\n'
du -h "$linux_dist_dir/dolphin-context-menu-manager" "$appimage_dist_dir/$appimage_name"
