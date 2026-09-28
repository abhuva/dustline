param([switch]$BuildOnly, [switch]$NoBuild, [int]$Port = 8765)
$ErrorActionPreference = 'Stop'
Set-Location $PSScriptRoot
if (-not (Test-Path 'tools/map_editor/generated/art.json')) { throw 'Run ./build.ps1 once to generate the game terrain art before opening the editor.' }
$editorImage = 'emscripten/emsdk@sha256:27bc6267cb285223b8aebb7627bfebae7cb3ad2aaa0d5923b8aa5321793033e8' # 4.0.15
if (-not $NoBuild) {
    python tools/generate_materials.py
    if ($LASTEXITCODE -ne 0) { throw 'Could not generate the material catalog.' }
    New-Item -ItemType Directory -Force 'tools/map_editor/generated' | Out-Null
    $mount = "type=bind,source=$PSScriptRoot,target=/work"
    docker run --rm --mount $mount -w /work $editorImage em++ tools/map_recipe_bridge.cpp -Iinclude -std=c++17 -O2 -fno-exceptions -fno-rtti -sMODULARIZE=1 -sEXPORT_ES6=1 '-sENVIRONMENT=web,worker,node' -sFILESYSTEM=0 -sINITIAL_MEMORY=8388608 -sSTACK_SIZE=65536 '-sEXPORTED_RUNTIME_METHODS=HEAPU8,HEAPU16,HEAPU32' '-sEXPORTED_FUNCTIONS=_recipe_input,_recipe_cells,_recipe_meta,_recipe_run,_recipe_collision,_recipe_connect_portal,_recipe_roads,_recipe_apply_materials,_recipe_ground,_recipe_tiles,_recipe_render_signature,_recipe_apply_spawns,_recipe_spawn_count,_recipe_spawns,_recipe_spawn_types,_recipe_apply_decoration,_recipe_decorations,_recipe_material_textures,_recipe_material_surfaces' -o tools/map_editor/generated/engine.mjs
    if ($LASTEXITCODE -ne 0) { throw 'Could not compile the shared map engine to WebAssembly.' }
}
if (-not (Test-Path 'tools/map_editor/generated/engine.wasm')) { throw 'Run ./map-editor.ps1 without -NoBuild first.' }
if ($BuildOnly) { Write-Host 'Editor engine built.'; return }
Write-Host "Map workshop:   http://127.0.0.1:$Port"
Write-Host "Music workshop: http://127.0.0.1:$Port/music.html (Ctrl+C to stop)"
python tools/serve_map_editor.py --port $Port
if ($LASTEXITCODE -ne 0) { throw 'Map editor server stopped with an error.' }
