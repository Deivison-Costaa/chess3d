#!/usr/bin/env bash
# Monta um AppImage auto-contido do Chess3D: binário + assets + engines Linux.
#
# Uso: build_appimage.sh <chess3d-binary> <assets-source-dir> [saida.AppImage]
#   <chess3d-binary>     ex.: build-package/bin/chess3d
#   <assets-source-dir>  ex.: assets   (de onde copiar models/textures/shaders)
#
# Requer: curl, e (na geração) FUSE ou APPIMAGE_EXTRACT_AND_RUN=1 (setado aqui).
set -euo pipefail

BIN="${1:?uso: build_appimage.sh <binario> <assets-dir> [saida]}"
ASSETS="${2:?informe o diretório de assets de origem}"
OUT="${3:-chess3d-x86_64.AppImage}"

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
STAGE="$(mktemp -d)"
APPDIR="$STAGE/AppDir"
TOOLS="$STAGE/tools"
mkdir -p "$APPDIR/usr/bin" \
         "$APPDIR/usr/share/chess3d/assets/models" \
         "$APPDIR/usr/share/applications" \
         "$APPDIR/usr/share/icons/hicolor/256x256/apps" \
         "$TOOLS"

# ── Binário + assets (sem .bak) ───────────────────────────────────────────────
install -m755 "$BIN" "$APPDIR/usr/bin/chess3d"
cp "$ASSETS/models/chessboard.glb" "$APPDIR/usr/share/chess3d/assets/models/"
cp -r "$ASSETS/textures" "$APPDIR/usr/share/chess3d/assets/"
cp -r "$ASSETS/shaders"  "$APPDIR/usr/share/chess3d/assets/"

# ── Engines Linux ─────────────────────────────────────────────────────────────
"$HERE/fetch_engines.sh" "$APPDIR/usr/share/chess3d/assets/engines"

# ── .desktop + ícone ──────────────────────────────────────────────────────────
cp "$ROOT/packaging/chess3d.desktop" "$APPDIR/usr/share/applications/chess3d.desktop"
cp "$ROOT/packaging/chess3d.png"     "$APPDIR/usr/share/icons/hicolor/256x256/apps/chess3d.png"

# ── Ferramentas AppImage ──────────────────────────────────────────────────────
echo ">> baixando linuxdeploy + appimagetool"
curl -fL https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage \
     -o "$TOOLS/linuxdeploy"
curl -fL https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-x86_64.AppImage \
     -o "$TOOLS/appimagetool"
chmod +x "$TOOLS/linuxdeploy" "$TOOLS/appimagetool"

# CI normalmente não tem FUSE: roda as ferramentas extraindo-se a si mesmas.
export APPIMAGE_EXTRACT_AND_RUN=1

# linuxdeploy empacota libs dependentes (libstdc++ etc.); libGL/libX11/driver
# ficam de fora (excludelist padrão) — vêm do host, como deve ser.
"$TOOLS/linuxdeploy" \
    --appdir "$APPDIR" \
    --executable "$APPDIR/usr/bin/chess3d" \
    --desktop-file "$APPDIR/usr/share/applications/chess3d.desktop" \
    --icon-file "$APPDIR/usr/share/icons/hicolor/256x256/apps/chess3d.png"

ARCH=x86_64 "$TOOLS/appimagetool" "$APPDIR" "$OUT"

echo ">> AppImage gerado: $OUT"
