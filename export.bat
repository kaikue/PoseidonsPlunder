blender --background --python meshes/export-bone-animations.py -- meshes/test_level_complex_character_pose.blend Player_Anim [1,24]Swim dist/test_level_complex.banim
blender --background --python meshes/export-meshes.py -- meshes/test_level_complex_character_pose.blend:1 dist/test_level_complex.pnc
blender --background --python meshes/export-scene.py -- meshes/test_level_complex_character_pose.blend:1 dist/test_level_complex.scene
blender --background --python meshes/export-walkmeshes.py -- meshes/test_level_complex_character_pose.blend:1 dist/test_level_complex.collision
