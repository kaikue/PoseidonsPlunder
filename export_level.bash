#!/usr/bin/env bash

# LEVEL=$1
LEVEL=test_level_complex_v2

blender="/Applications/Blender/blender.app/Contents/MacOS/blender"

$blender --background --python meshes/export-meshes.py -- meshes/$LEVEL.blend:1 dist/$LEVEL.pnc
$blender --background --python meshes/export-scene.py -- meshes/$LEVEL.blend:1 dist/$LEVEL.scene
$blender --background --python meshes/export-walkmeshes.py -- meshes/$LEVEL.blend:1 dist/$LEVEL.collision
