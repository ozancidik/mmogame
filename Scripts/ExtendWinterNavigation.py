"""Enlarge Winter_scene's NavMeshBoundsVolume and rebuild/save Recast data.

Run with UnrealEditor-Cmd -ExecutePythonScript (editor closed). The original
bounds (X +-4000, Y -4500..1500) only covered the Lv41-57 corridor; the
landscape extends to roughly +-12000, so Lv66-80 pockets need more nav space.
New bounds: X -8000..8000, Y -8500..5500, Z -1500..1500 around (0,-1500).
Encounter locations still need their own ring/path probes and PIE review.
"""

import unreal

MAP = '/Game/HandPaintedEnvironment/Maps/Winter_scene/Winter_scene'
unreal.EditorLoadingAndSavingUtils.load_map(MAP)
world = unreal.EditorLevelLibrary.get_editor_world()
volume = next((a for a in unreal.EditorLevelLibrary.get_all_level_actors()
               if a.get_actor_label() == 'MMO_Winter_NavBounds'), None)
if volume is None:
    raise RuntimeError('MMO_Winter_NavBounds not found; run SetupWinterNavigation.py first')

volume.set_actor_location(unreal.Vector(0, -1500, 0), False, False)
volume.set_actor_scale3d(unreal.Vector(80, 70, 15))
origin, extent = volume.get_actor_bounds(False)
print('WINTER_NAV_BOUNDS_EXTENDED', origin, extent)

nav = unreal.NavigationSystemV1.get_navigation_system(world)
nav.on_navigation_bounds_updated(volume)
unreal.SystemLibrary.execute_console_command(world, 'RebuildNavigation')

hub = unreal.NavigationSystemV1.project_point_to_navigation(
    world, unreal.Vector(0, -1500, 0), None, None, unreal.Vector(100, 100, 300))
far = unreal.NavigationSystemV1.project_point_to_navigation(
    world, unreal.Vector(-6000, -6000, 0), None, None, unreal.Vector(200, 200, 1500))
print('WINTER_NAV_CHECK hub=%s far=%s' % (hub, far))
if hub is None:
    raise RuntimeError('Winter hub lost navmesh; map not saved')
if not unreal.EditorLevelLibrary.save_current_level():
    raise RuntimeError('Winter_scene save failed')
print('WINTER_NAV_EXTENDED_SAVED')
