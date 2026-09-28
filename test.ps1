$ErrorActionPreference = 'Stop'
Set-Location $PSScriptRoot
if (-not (Test-Path 'dustline.elf')) { throw 'Run ./build.ps1 first.' }
$mount = "type=bind,source=$PSScriptRoot,target=/work"
docker run --rm --mount $mount dustline-build:1 python3 tools/test_map_library.py
if ($LASTEXITCODE -ne 0) { throw 'Shared map library persistence test failed.' }
docker run --rm --mount $mount dustline-build:1 python3 tools/test_world_grid.py
if ($LASTEXITCODE -ne 0) { throw 'Cardinal world graph validation failed.' }
docker run --rm --mount $mount dustline-build:1 python3 tools/test_shop_catalog.py
if ($LASTEXITCODE -ne 0) { throw 'Shared shop catalog validation failed.' }
docker run --rm --mount $mount dustline-build:1 python3 tools/test_wasteland_art.py
if ($LASTEXITCODE -ne 0) { throw 'Wasteland art masks, palette or repeat checks failed.' }
docker run --rm --mount $mount dustline-build:1 python3 tools/test_art_profiles.py
if ($LASTEXITCODE -ne 0) { throw 'Per-map art bank validation failed.' }
docker run --rm --mount $mount dustline-build:1 python3 tools/test_music.py
if ($LASTEXITCODE -ne 0) { throw 'Adaptive music generation test failed.' }

docker run --rm --mount $mount dustline-build:1 python3 tools/test_map_storage.py
if ($LASTEXITCODE -ne 0) { throw 'Build audit regression tests failed.' }

docker run --rm --mount $mount dustline-build:1 g++ -std=c++17 -O2 -Wall -Wextra -Iinclude tools/test_map_recipe.cpp -o build/test_map_recipe
if ($LASTEXITCODE -ne 0) { throw 'Could not compile recipe tests.' }
docker run --rm --mount $mount dustline-build:1 g++ -std=c++17 -O2 -Iinclude tools/map_recipe_bridge.cpp -o build/map_recipe_bridge
if ($LASTEXITCODE -ne 0) { throw 'Could not compile active recipe reference.' }

docker run --rm --mount $mount dustline-build:1 ./build/test_map_recipe
if ($LASTEXITCODE -ne 0) { throw 'Recipe kernel tests failed.' }
docker run --rm --mount $mount dustline-build:1 g++ -std=c++17 -O2 -Wall -Wextra -Iinclude tools/test_placement.cpp -o build/test_placement
if ($LASTEXITCODE -ne 0) { throw 'Could not compile placement tests.' }
docker run --rm --mount $mount dustline-build:1 ./build/test_placement
if ($LASTEXITCODE -ne 0) { throw 'Placement tests failed.' }
docker run --rm --mount $mount dustline-build:1 g++ -std=c++17 -O2 -Wall -Wextra -Iinclude tools/test_vehicle_contact.cpp -o build/test_vehicle_contact
if ($LASTEXITCODE -ne 0) { throw 'Could not compile vehicle contact tests.' }
docker run --rm --mount $mount dustline-build:1 ./build/test_vehicle_contact
if ($LASTEXITCODE -ne 0) { throw 'Vehicle contact physics tests failed.' }
docker run --rm --mount $mount dustline-build:1 g++ -std=c++17 -O2 -Wall -Wextra -Iinclude tools/test_mission.cpp src/mission.cpp -o build/test_mission
if ($LASTEXITCODE -ne 0) { throw 'Could not compile mission-state tests.' }
docker run --rm --mount $mount dustline-build:1 ./build/test_mission
if ($LASTEXITCODE -ne 0) { throw 'Mission-state tests failed.' }
docker run --rm --mount $mount dustline-build:1 g++ -std=c++17 -O2 -Wall -Wextra -Iinclude tools/test_race.cpp src/race.cpp -o build/test_race
if ($LASTEXITCODE -ne 0) { throw 'Could not compile race-state tests.' }
docker run --rm --mount $mount dustline-build:1 ./build/test_race
if ($LASTEXITCODE -ne 0) { throw 'Race route/state tests failed.' }
docker run --rm --mount $mount dustline-build:1 g++ -std=c++17 -O2 -Wall -Wextra -Iinclude tools/test_save_data.cpp src/save_data.cpp -o build/test_save_data
if ($LASTEXITCODE -ne 0) { throw 'Could not compile save-format tests.' }
docker run --rm --mount $mount dustline-build:1 ./build/test_save_data
if ($LASTEXITCODE -ne 0) { throw 'Save-format resilience tests failed.' }
docker run --rm --mount $mount dustline-build:1 g++ -std=c++17 -O2 -Wall -Wextra -Iinclude tools/test_terrain_cache.cpp -o build/test_terrain_cache
if ($LASTEXITCODE -ne 0) { throw 'Could not compile tile cache stress test.' }
docker run --rm --mount $mount dustline-build:1 ./build/test_terrain_cache
if ($LASTEXITCODE -ne 0) { throw 'Tile cache stress test failed.' }

docker run --rm --mount $mount dustline-build:1 g++ -std=c++17 -O2 -Wall -Wextra -Iinclude tools/test_chunk_cache.cpp -o build/test_chunk_cache
if ($LASTEXITCODE -ne 0) { throw 'Could not compile chunk compression test.' }
docker run --rm --mount $mount dustline-build:1 ./build/test_chunk_cache
if ($LASTEXITCODE -ne 0) { throw 'Chunk compression test failed.' }
docker run --rm --mount $mount dustline-build:1 gcc -shared -fPIC -O2 tools/emulator_bridge.c -o build/emulator_bridge.so -lmgba
if ($LASTEXITCODE -ne 0) { throw 'Could not compile headless mGBA adapter.' }
docker run --rm --mount $mount dustline-build:1 g++ -std=c++17 -O2 -Wall -Wextra -Iinclude tools/test_cave_layout.cpp -o build/test_cave_layout
if ($LASTEXITCODE -ne 0) { throw 'Could not compile cave generator tests.' }
docker run --rm --mount $mount dustline-build:1 ./build/test_cave_layout
if ($LASTEXITCODE -ne 0) { throw 'Cave connectivity test failed.' }
docker run --rm --mount $mount dustline-build:1 python3 tools/test_town_scene.py
if ($LASTEXITCODE -ne 0) { throw 'Walkable town scene test failed; see artifacts/town.' }
docker run --rm --mount $mount dustline-build:1 python3 tools/test_portals.py
if ($LASTEXITCODE -ne 0) { throw 'Connected-region gate test failed; see artifacts/portals.' }
docker run --rm --mount $mount dustline-build:1 python3 tools/test_missions.py
if ($LASTEXITCODE -ne 0) { throw 'Contract-board test failed; see artifacts/missions.' }
docker run --rm --mount $mount dustline-build:1 python3 tools/test_races.py
if ($LASTEXITCODE -ne 0) { throw 'Race-office or generated-course test failed; see artifacts/races.' }
docker run --rm --mount $mount dustline-build:1 python3 tools/test_art_banks_rom.py
if ($LASTEXITCODE -ne 0) { throw 'Per-map ROM art-bank switching failed; see artifacts/art-banks.' }
docker run --rm --mount $mount dustline-build:1 python3 tools/test_music_rom.py
if ($LASTEXITCODE -ne 0) { throw 'Adaptive music ROM test failed.' }
docker run --rm --mount $mount dustline-build:1 python3 tools/test_audio_rom.py
if ($LASTEXITCODE -ne 0) { throw 'Audio controls or motor-settling ROM test failed.' }
docker run --rm --mount $mount dustline-build:1 python3 tools/test_radio.py
if ($LASTEXITCODE -ne 0) { throw 'Radio-signal discovery test failed; see artifacts/radio.' }
docker run --rm --mount $mount dustline-build:1 python3 tools/test_rom.py
if ($LASTEXITCODE -ne 0) { throw 'ROM verification failed; see artifacts/test-results.json and captures.' }

docker run --rm --mount $mount dustline-build:1 python3 tools/test_town_streaming.py
if ($LASTEXITCODE -ne 0) { throw 'Town approach streaming regression failed.' }
