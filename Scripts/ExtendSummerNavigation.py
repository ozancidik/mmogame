"""Enlarge Summer_scene's NavMeshBoundsVolume and rebuild/save Recast data.

Run with UnrealEditor-Cmd -ExecutePythonScript (editor closed). The original
bounds (X/Y +-4000 around (-490,30)) left late-game pockets (Lv58-65 Rift) too
close to early zones. New bounds: X/Y -8500..8500, Z -1500..1500 around (0,0).
After saving, the script re-checks a far point; the map is saved only if both
the hub and that far point project onto navmesh.
"""

import unreal

MAP = '/Game/HandPaintedEnvironment/Maps/Summer_scene/Summer_scene'
unreal.EditorLoadingAndSavingUtils.load_map(MAP)
world = unreal.EditorLevelLibrary.get_editor_world()
vols = [a for a in unreal.EditorLevelLibrary.get_all_level_actors()
        if a.get_class().get_name() == 'NavMeshBoundsVolume']
if len(vols) != 1:
    raise RuntimeError('Expected exactly one NavMeshBoundsVolume in Summer_scene, found %d' % len(vols))
volume = vols[0]
print('SUMMER_NAV_OLD', volume.get_actor_label(), volume.get_actor_location(), volume.get_actor_scale3d())

volume.set_actor_location(unreal.Vector(0, 0, 0), False, False)
volume.set_actor_scale3d(unreal.Vector(85, 85, 15))
origin, extent = volume.get_actor_bounds(False)
print('SUMMER_NAV_BOUNDS_EXTENDED', origin, extent)

nav = unreal.NavigationSystemV1.get_navigation_system(world)
nav.on_navigation_bounds_updated(volume)
unreal.SystemLibrary.execute_console_command(world, 'RebuildNavigation')

hub = unreal.NavigationSystemV1.project_point_to_navigation(
    world, unreal.Vector(0, 0, 300), None, None, unreal.Vector(200, 200, 600))
far = unreal.NavigationSystemV1.project_point_to_navigation(
    world, unreal.Vector(-6000, 4000, 0), None, None, unreal.Vector(300, 300, 2000))
print('SUMMER_NAV_CHECK hub=%s far=%s' % (hub, far))
if hub is None or far is None:
    raise RuntimeError('Summer navmesh not built over the new bounds; map not saved')
if not unreal.EditorLevelLibrary.save_current_level():
    raise RuntimeError('Summer_scene save failed')
print('SUMMER_NAV_EXTENDED_SAVED')
