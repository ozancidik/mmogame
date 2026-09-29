"""Build and save Winter_scene's Recast navigation data.

Run with UnrealEditor-Cmd -ExecutePythonScript after SetupWinterNavigation.py.
Only the map is saved. Encounter locations still require complete path checks.
"""

import unreal


MAP = '/Game/HandPaintedEnvironment/Maps/Winter_scene/Winter_scene'
unreal.EditorLoadingAndSavingUtils.load_map(MAP)
world = unreal.EditorLevelLibrary.get_editor_world()
nav = unreal.NavigationSystemV1.get_navigation_system(world)
volume = next(
    (actor for actor in unreal.EditorLevelLibrary.get_all_level_actors()
     if actor.get_class().get_name() == 'NavMeshBoundsVolume'),
    None,
)
if nav is None or volume is None:
    raise RuntimeError('Winter navigation system or bounds volume is missing')

nav.on_navigation_bounds_updated(volume)
unreal.SystemLibrary.execute_console_command(world, 'RebuildNavigation')
origin = unreal.Vector(0, -1500, 0)
projected = unreal.NavigationSystemV1.project_point_to_navigation(
    world, origin, None, None, unreal.Vector(100, 100, 300))
if projected is None:
    raise RuntimeError('Winter hub has no navmesh; map was not saved')
if not unreal.EditorLevelLibrary.save_current_level():
    raise RuntimeError('Winter navigation build succeeded but map save failed')
print('WINTER_NAV_BUILD_SAVED', projected)
