#!/usr/bin/env bash
# Baixa/compila as engines UCI Linux p/ <dest>, no layout do EngineCatalog:
#   stockfish, berserk, berserk.nn (+ THIRD_PARTY_LICENSES/)
#
# Stockfish: binário oficial pré-compilado.
# Berserk:   não tem binário Linux oficial → compila do código (BEST-EFFORT;
#            se falhar, o AppImage segue só com Stockfish, e a UI esconde Berserk).
#
# Uso: fetch_engines.sh <dest_dir>
set -euo pipefail

DEST="${1:?uso: fetch_engines.sh <dest_dir>}"
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck disable=SC1091
source "$HERE/../engines.manifest"

mkdir -p "$DEST" "$DEST/THIRD_PARTY_LICENSES"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

# ── Stockfish ─────────────────────────────────────────────────────────────────
echo ">> Stockfish ($STOCKFISH_TAG)"
curl -fL "$STOCKFISH_LINUX_URL" -o "$WORK/sf.tar"
tar -xf "$WORK/sf.tar" -C "$WORK"
sf_bin="$(find "$WORK" -type f -name 'stockfish-*' ! -name '*.tar' | head -n1)"
if [ -z "$sf_bin" ]; then
    echo "!! Stockfish: binário não encontrado no tarball" >&2
    exit 1
fi
install -m755 "$sf_bin" "$DEST/stockfish"

# ── Berserk (best-effort) ─────────────────────────────────────────────────────
echo ">> Berserk ($BERSERK_TAG) — compilando do código (best-effort)"
if git clone --depth 1 -b "$BERSERK_TAG" "$BERSERK_SRC_REPO" "$WORK/berserk"; then
    if make -C "$WORK/berserk/src" -j"$(nproc)"; then
        bk_bin="$(find "$WORK/berserk/src" -maxdepth 1 -type f -name 'berserk*' -perm -u+x | head -n1 || true)"
        if [ -n "$bk_bin" ]; then
            install -m755 "$bk_bin" "$DEST/berserk"
            curl -fL "$BERSERK_NET_URL" -o "$DEST/berserk.nn" || \
                echo "!! Berserk: net não baixada (binário pode ter rede embutida)"
        else
            echo "!! Berserk: binário não encontrado após make — pulando"
        fi
    else
        echo "!! Berserk: make falhou — pulando"
    fi
else
    echo "!! Berserk: git clone falhou — pulando"
fi

# ── Licenças (GPLv3) ──────────────────────────────────────────────────────────
curl -fL "$STOCKFISH_LICENSE_URL" -o "$DEST/THIRD_PARTY_LICENSES/Stockfish-COPYING.txt" || true
curl -fL "$BERSERK_LICENSE_URL"   -o "$DEST/THIRD_PARTY_LICENSES/Berserk-LICENSE.txt"   || true

echo ">> Engines Linux prontas em: $DEST"
ls -la "$DEST"
