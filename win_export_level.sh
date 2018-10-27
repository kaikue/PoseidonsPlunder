blender --background --python meshes/export-meshes.py -- meshes/test_level_complex_v2.blend:1 dist/test_level_complex_v2.pnc
blender --background --python meshes/export-scene.py -- meshes/test_level_complex_v2.blend:1 dist/test_level_complex_v2.scene
blender --background --python meshes/export-walkmeshes.py -- meshes/test_level_complex_v2.blend:1 dist/test_level_complex_v2.collision