#!/usr/bin/env fish
# setup-vkbasalt-sober.fish
# Sets up vkBasalt for the Sober Flatpak (user: schlonny)
# Usage: fish setup-vkbasalt-sober.fish

set -l APP_ID "org.vinegarhq.Sober"
set -l USER_NAME "schlonny"
set -l HOME_DIR "/home/$USER_NAME"

# --- Paths ---
set -l FLATPAK_CFG "$HOME_DIR/.var/app/$APP_ID/config/vkBasalt"
set -l HOST_CFG    "$HOME_DIR/.config/vkBasalt"
set -l CONF        "$FLATPAK_CFG/vkBasalt.conf"

echo "==> Setting up vkBasalt for $APP_ID"
echo "    Flatpak config dir: $FLATPAK_CFG"
echo "    Host config dir:    $HOST_CFG"
echo

# --- 1. Ensure vkBasalt Flatpak runtime layer is installed ---
echo "==> Checking for vkBasalt Flatpak runtime layer..."
if not flatpak list --runtime --columns=ref 2>/dev/null | string match -q "*org.freedesktop.Platform.VulkanLayer.vkBasalt*"
    echo "    Not found. Installing (branch 25.08)..."
    flatpak install -y flathub org.freedesktop.Platform.VulkanLayer.vkBasalt//25.08
    or begin
        echo "!! Failed to install vkBasalt layer. Aborting." >&2
        exit 1
    end
else
    echo "    Already installed."
end

# --- 2. Create config directories ---
echo "==> Creating config directories..."
mkdir -p "$FLATPAK_CFG"
mkdir -p "$HOST_CFG"

# --- 3. Copy all .fx and .fxh files from host to flatpak config ---
echo "==> Copying shader files (.fx / .fxh)..."
set -l copied 0
if test -d "$HOST_CFG"
    for f in $HOST_CFG/*.fx $HOST_CFG/*.fxh
        if test -f "$f"
            cp -f "$f" "$FLATPAK_CFG/"
            echo "    copied: "(basename "$f")
            set copied (math $copied + 1)
        end
    end
end
if test $copied -eq 0
    echo "    !! No .fx/.fxh files found in $HOST_CFG"
    echo "       You'll need to place your shader files there first,"
    echo "       or drop them directly into $FLATPAK_CFG"
end

# --- 4. Copy an existing config if present, else grab the default ---
echo "==> Preparing vkBasalt.conf..."
if test -f "$HOST_CFG/vkBasalt.conf"
    echo "    Copying existing host config."
    cp -f "$HOST_CFG/vkBasalt.conf" "$CONF"
else if not test -f "$CONF"
    echo "    No host config. Downloading default from upstream..."
    curl -fsSL https://raw.githubusercontent.com/DadSchoorse/vkBasalt/master/config/vkBasalt.conf -o "$CONF"
    or begin
        echo "!! Failed to download default vkBasalt.conf" >&2
        exit 1
    end
else
    echo "    Flatpak config already exists, leaving as-is."
end

# --- 5. Rewrite paths in the config to point inside the Flatpak sandbox ---
echo "==> Rewriting shader paths inside vkBasalt.conf..."
# Replace host config path with flatpak config path
sed -i "s|$HOST_CFG|$FLATPAK_CFG|g" "$CONF"

# Also handle any bare "~/.config/vkBasalt" style paths just in case
sed -i "s|~/.config/vkBasalt|$FLATPAK_CFG|g" "$CONF"

# --- 6. Set the Flatpak override so ENABLE_VKBASALT is always on ---
echo "==> Setting Flatpak override ENABLE_VKBASALT=1..."
flatpak override --user --env=ENABLE_VKBASALT=1 "$APP_ID"
or echo "    (override may already exist, continuing)"

# --- 7. Show final config summary ---
echo
echo "==> Final vkBasalt.conf (shader-related lines):"
grep -E "effects|\.fx|reshade|enableOnLaunch|toggleKey" "$CONF" | sed 's/^/    /'

echo
echo "==> Files in $FLATPAK_CFG:"
ls -1 "$FLATPAK_CFG" | sed 's/^/    /'

echo
echo "==> Done."
echo "    Launch with:  flatpak run --env=ENABLE_VKBASALT=1 $APP_ID"
echo "    Log file:     $FLATPAK_CFG/vkBasalt.log"
