# cmake/FetchEngines.cmake
#
# Baixa as engines UCI (Stockfish + Berserk) para
#   ${CMAKE_BINARY_DIR}/engines-staging
# no layout esperado pelo EngineCatalog: stockfish.exe, berserk.exe, berserk.nn
# (+ THIRD_PARTY_LICENSES/). Usado para montar o payload.zip do .exe Windows.
#
# - Lê packaging/engines.manifest (fonte única de versões/URLs).
# - Idempotente: pula o que já foi baixado.
# - Override: -DCHESS3D_ENGINES_DIR=<dir já populado> copia em vez de baixar.
#
# Exporta a variável CHESS3D_ENGINES_STAGING para o CMakeLists chamador.

set(_staging "${CMAKE_BINARY_DIR}/engines-staging")
set(CHESS3D_ENGINES_STAGING "${_staging}")
file(MAKE_DIRECTORY "${_staging}")
file(MAKE_DIRECTORY "${_staging}/THIRD_PARTY_LICENSES")

if(CHESS3D_ENGINES_DIR)
    message(STATUS "FetchEngines: usando engines de ${CHESS3D_ENGINES_DIR} (override, sem download)")
    file(COPY "${CHESS3D_ENGINES_DIR}/" DESTINATION "${_staging}")
    return()
endif()

# ── Parse do manifesto (KEY=VALUE) ────────────────────────────────────────────
file(STRINGS "${CMAKE_SOURCE_DIR}/packaging/engines.manifest" _lines)
foreach(_l IN LISTS _lines)
    if(NOT _l MATCHES "^[ \t]*#" AND _l MATCHES "^([A-Za-z0-9_]+)=(.*)$")
        set("M_${CMAKE_MATCH_1}" "${CMAKE_MATCH_2}")
    endif()
endforeach()

set(_dl "${CMAKE_BINARY_DIR}/engines-dl")
file(MAKE_DIRECTORY "${_dl}")

function(_fetch url dest fatal)
    if(EXISTS "${dest}")
        return()
    endif()
    message(STATUS "FetchEngines: baixando ${url}")
    file(DOWNLOAD "${url}" "${dest}" STATUS _st)
    list(GET _st 0 _code)
    if(NOT _code EQUAL 0)
        list(GET _st 1 _msg)
        file(REMOVE "${dest}")
        if(fatal)
            message(FATAL_ERROR "FetchEngines: falha ao baixar ${url}: ${_msg}")
        else()
            message(WARNING "FetchEngines: pulando (falha) ${url}: ${_msg}")
        endif()
    endif()
endfunction()

# ── Stockfish: zip oficial → extrai → renomeia p/ stockfish.exe ───────────────
_fetch("${M_STOCKFISH_WIN_URL}" "${_dl}/stockfish.zip" TRUE)
file(ARCHIVE_EXTRACT INPUT "${_dl}/stockfish.zip" DESTINATION "${_dl}/sf")
file(GLOB_RECURSE _sf_exe "${_dl}/sf/*.exe")
if(NOT _sf_exe)
    message(FATAL_ERROR "FetchEngines: nenhum .exe encontrado no zip do Stockfish")
endif()
list(GET _sf_exe 0 _sf_first)
file(COPY "${_sf_first}" DESTINATION "${_staging}")
get_filename_component(_sf_name "${_sf_first}" NAME)
if(NOT _sf_name STREQUAL "stockfish.exe")
    file(RENAME "${_staging}/${_sf_name}" "${_staging}/stockfish.exe")
endif()

# ── Berserk: .exe pré-compilado + rede ────────────────────────────────────────
_fetch("${M_BERSERK_WIN_URL}" "${_staging}/berserk.exe" TRUE)
_fetch("${M_BERSERK_NET_URL}" "${_staging}/berserk.nn"  TRUE)

# ── Licenças (best-effort: não derruba o build se um link mudar) ──────────────
_fetch("${M_STOCKFISH_LICENSE_URL}" "${_staging}/THIRD_PARTY_LICENSES/Stockfish-COPYING.txt" FALSE)
_fetch("${M_BERSERK_LICENSE_URL}"   "${_staging}/THIRD_PARTY_LICENSES/Berserk-LICENSE.txt"   FALSE)

message(STATUS "FetchEngines: engines prontas em ${_staging}")
