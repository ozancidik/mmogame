"""Place Winter_scene's first player start and navigation bounds in the editor.

Run with UnrealEditor-Cmd -ExecutePythonScript. Idempotent by actor label.
The bounds cover the measured ground corridor; encounter locations still need
their own walkability and PIE review before any spawners are enabled.
"""

import unreal


MAP = '/Game/HandPaintedEnvironment/Maps/Winter_scene/Winter_scene'
unreal.EditorLoadingAndSavingUtils.load_map(MAP)
actors = unreal.EditorLevelLibrary.get_all_level_actors()


def find_by_label(label):
    for actor in actors:
        if actor.get_actor_label() == label:
            return actor
    return None


start = find_by_label('MMO_Winter_PlayerStart')
if start is None:
    start = unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.PlayerStart, unreal.Vector(0, -1500, 150), unreal.Rotator(0, 0, 0))
    if start is None:
        raise RuntimeError('Winter PlayerStart could not be created')
    start.set_actor_label('MMO_Winter_PlayerStart')

volume = find_by_label('MMO_Winter_NavBounds')
if volume is None:
    volume = unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.NavMeshBoundsVolume, unreal.Vector(0, -1500, 0), unreal.Rotator(0, 0, 0))
    if volume is None:
        raise RuntimeError('Winter NavMeshBoundsVolume could not be created')
    volume.set_actor_label('MMO_Winter_NavBounds')

# The stock volume's half extent is (100,100,100) at scale one. This covers
# X [-4000,4000], Y [-4500,1500], Z [-1000,1000] around the measured corridor.
volume.set_actor_scale3d(unreal.Vector(40, 30, 10))
origin, extent = volume.get_actor_bounds(False)
print('WINTER_NAV_BOUNDS', origin, extent)
if abs(extent.x - 4000) > 10 or abs(extent.y - 3000) > 10:
    raise RuntimeError('Winter NavMeshBoundsVolume dimensions differ from expected')

if not unreal.EditorLevelLibrary.save_current_level():
    raise RuntimeError('Winter_scene could not be saved')
print('WINTER_NAV_SETUP_SAVED')
