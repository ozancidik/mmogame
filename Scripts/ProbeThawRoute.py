"""Summer_scene: bir mob cebi adayinin ROTA ACIKLIGINI olcer (editor kapali, UnrealEditor-Cmd ile).

Kullanim (PowerShell):
  $env:THAW_MODE = 'grid'            # 500u adimli grid taramasi (varsayilan)
  $env:THAW_MODE = 'cand'; $env:THAW_CANDS = '1500,-3000;2000,-3500'   # belirli adaylar
  & "...\\UnrealEditor-Cmd.exe" "<uproject>" -ExecutePythonScript="<bu dosya>" -unattended -nopause -nosplash -NullRHI -stdout

Olcutler (Cozulme Yolu normal-mob cebi icin, docs/QA_ROUND_2026-10-06_NIGHT2.md ORTA-1):
  1) mevcut TUM cep merkezlerine >= MIN_CENTER (2200u)
  2) hub -> aday NAV yolu gecerli ve KISMI DEGIL; yol noktalarinin cokluchizgisi, seviyesi >= 20 olan
     her cep merkezine >= MIN_ROUTE (1500u) uzak
  4) navmesh siniri +-8500 icinde, hub'a >= 1500u
Cikti satirlari "THAWROUTE ..." onekiyle stdout'a yazilir; ayrica Saved/thaw_route_result.txt.
Oyun ici dogrulama (olcut 3) ayri: Scripts/SmokeTest.ps1 + MMOProbeSurface.
"""

import math
import os
import unreal

MAP = '/Game/HandPaintedEnvironment/Maps/Summer_scene/Summer_scene'
MIN_CENTER = 2200.0
MIN_ROUTE = 1500.0
MIN_HUB = 1500.0
NAV_LIMIT = 8500.0
MOB_AREA_DISTANCE = 700.0   # AMMOGameMode::MobAreaDistance

unreal.EditorLoadingAndSavingUtils.load_map(MAP)
world = unreal.EditorLevelLibrary.get_editor_world()

start = None
for a in unreal.EditorLevelLibrary.get_all_level_actors():
    if a.get_class().get_name() == 'PlayerStart':
        start = a
        break
if start is None:
    raise RuntimeError('PlayerStart yok')
o = start.get_actor_location()
yaw = start.get_actor_rotation().yaw
print('THAWROUTE playerstart=(%.2f,%.2f,%.2f) yaw=%.2f' % (o.x, o.y, o.z, yaw))
ox, oy = o.x, o.y
fx, fy = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
rx, ry = math.cos(math.radians(yaw + 90)), math.sin(math.radians(yaw + 90))


def rel(f, r):
    return (ox + fx * f + rx * r, oy + fy * f + ry * r)


D = MOB_AREA_DISTANCE
# (ad, x, y, seviye) -- seviye: rota olcutunde kullanilan (>=20 ise rota kontrolune girer)
CENTERS = [
    ('Ana Lv1-5', ) + rel(D * 1.5, 0) + (5,),
    ('OrkReisi', ) + rel(D * 2.2, 0) + (6,),
    ('OrumcekAna', ) + rel(D * 1.6, 700) + (5,),
    ('KadimGolem', ) + rel(D * 1.6, -700) + (7,),
    ('Suikastci', ) + rel(1400, 1350) + (7,),
    ('Ikinci Lv6-7', ) + rel(-650, 1300) + (7,),
    ('Ucuncu Lv8-12', ) + rel(-1300, 0) + (12,),
    ('BuzLordu', ) + rel(-1650, 1300) + (13,),
    ('Dorduncu Lv16-22', ) + rel(-650, -1300) + (22,),
    ('MagmaTirani', ) + rel(1400, -1350) + (24,),
    ('Obsidyen', -2800.0, -1800.0, 29),
    ('ObsidyenReisi', -4000.0, 0.0, 30),
    ('Alacakaranlik', 1000.0, 3200.0, 39),
    ('AlacakaranlikHukumdari', -3200.0, 1800.0, 40),
    ('RiftArayici', 800.0, -4800.0, 58),
    ('RiftBekci', -4800.0, 4800.0, 62),
    ('RiftHaberci', 4800.0, 4000.0, 65),
]
# Rota olcutu: seviyesi >= 20 cepler. Dorduncu (Lv16-22) karisik: hub'a zaten ~1453u, ayri raporlanir.
ROUTE_CENTERS = [c for c in CENTERS if c[3] >= 20]
FOURTH = [c for c in CENTERS if c[0].startswith('Dorduncu')][0]
HUB2D = (ox, oy)


def seg_dist(px, py, ax, ay, bx, by):
    dx, dy = bx - ax, by - ay
    L2 = dx * dx + dy * dy
    if L2 < 1e-6:
        return math.hypot(px - ax, py - ay)
    t = max(0.0, min(1.0, ((px - ax) * dx + (py - ay) * dy) / L2))
    return math.hypot(px - (ax + t * dx), py - (ay + t * dy))


def poly_dist(pts, cx, cy):
    best = 1e18
    for i in range(len(pts) - 1):
        best = min(best, seg_dist(cx, cy, pts[i][0], pts[i][1], pts[i + 1][0], pts[i + 1][1]))
    return best


def center_gate(x, y):
    nearest = min(CENTERS, key=lambda c: math.hypot(c[1] - x, c[2] - y))
    return math.hypot(nearest[1] - x, nearest[2] - y), nearest[0]


hub_nav = unreal.NavigationSystemV1.project_point_to_navigation(
    world, unreal.Vector(ox, oy, o.z), None, None, unreal.Vector(300, 300, 1500))
if hub_nav is None:
    raise RuntimeError('hub navmesh uzerine izdusurulemedi')

results = []


def evaluate(x, y, verbose=False):
    row = {'x': x, 'y': y}
    row['hub_d'] = math.hypot(x - ox, y - oy)
    row['center_d'], row['center_n'] = center_gate(x, y)
    row['inbounds'] = abs(x) <= NAV_LIMIT - 300 and abs(y) <= NAV_LIMIT - 300
    if not row['inbounds']:
        row['nav'] = 'disarida'
        return row
    p = unreal.NavigationSystemV1.project_point_to_navigation(
        world, unreal.Vector(x, y, 0), None, None, unreal.Vector(100, 100, 3000))
    if p is None:
        row['nav'] = 'navyok'
        return row
    path = unreal.NavigationSystemV1.find_path_to_location_synchronously(world, hub_nav, p, None, None)
    if path is None or not path.is_valid():
        row['nav'] = 'yolyok'
        return row
    if path.is_partial():
        row['nav'] = 'kismi'
        return row
    pts = [(v.x, v.y) for v in path.path_points]
    row['nav'] = 'tam'
    row['z'] = p.z
    row['pts'] = pts
    row['plen'] = sum(math.hypot(pts[i + 1][0] - pts[i][0], pts[i + 1][1] - pts[i][1]) for i in range(len(pts) - 1))
    worst = min(ROUTE_CENTERS, key=lambda c: poly_dist(pts, c[1], c[2]))
    row['route_min'] = poly_dist(pts, worst[1], worst[2])
    row['route_n'] = worst[0]
    row['fourth_min'] = poly_dist(pts, FOURTH[1], FOURTH[2])
    return row


mode = os.environ.get('THAW_MODE', 'grid')
out_lines = []
if mode == 'ring':
    # THAW_CANDS merkezlerinin 225u halkasinda 32 noktanin hub'a TAM (kismi olmayan) yolla baglanip baglanmadigi
    cands = [tuple(float(v) for v in s.split(',')) for s in os.environ.get('THAW_CANDS', '').split(';') if s.strip()]
    for (x, y) in cands:
        full = 0
        zs = []
        worst_route = 1e18
        for i in range(32):
            a = math.radians(i * 360.0 / 32.0)
            r = evaluate(x + 225.0 * math.cos(a), y + 225.0 * math.sin(a))
            if r['nav'] == 'tam':
                full += 1
                zs.append(r['z'])
                worst_route = min(worst_route, r['route_min'])
        line = 'THAWROUTE RING (%.0f,%.0f) r225 tam_yol=%d/32 zemin_z=%s route_min_halka=%s' % (
            x, y, full, ('%.1f..%.1f' % (min(zs), max(zs))) if zs else '-', ('%.0f' % worst_route) if zs else '-')
        print(line)
        out_lines.append(line)
elif mode == 'cand':
    cands = [tuple(float(v) for v in s.split(',')) for s in os.environ.get('THAW_CANDS', '').split(';') if s.strip()]
    for (x, y) in cands:
        r = evaluate(x, y)
        line = 'THAWROUTE CAND (%.0f,%.0f) nav=%s z=%s hub_d=%.0f center_d=%.0f(%s) route_min=%s(%s) fourth_min=%s plen=%s' % (
            x, y, r['nav'], ('%.1f' % r['z']) if 'z' in r else '-', r['hub_d'], r['center_d'], r['center_n'],
            ('%.0f' % r['route_min']) if 'route_min' in r else '-', r.get('route_n', '-'),
            ('%.0f' % r['fourth_min']) if 'fourth_min' in r else '-', ('%.0f' % r['plen']) if 'plen' in r else '-')
        print(line)
        out_lines.append(line)
        if 'pts' in r:
            pl = 'THAWROUTE PTS (%.0f,%.0f): ' % (x, y) + ' '.join('(%.0f,%.0f)' % p for p in r['pts'])
            print(pl)
            out_lines.append(pl)
else:
    step = float(os.environ.get('THAW_STEP', '500'))
    lo, hi = -8000.0, 8000.0
    x = lo
    while x <= hi:
        y = lo
        while y <= hi:
            hubd = math.hypot(x - ox, y - oy)
            cd, cn = center_gate(x, y)
            if hubd >= MIN_HUB and cd >= MIN_CENTER:
                r = evaluate(x, y)
                if r['nav'] == 'tam':
                    ok = r['route_min'] >= MIN_ROUTE
                    line = 'THAWROUTE GRID (%.0f,%.0f) z=%.1f hub_d=%.0f center_d=%.0f(%s) route_min=%.0f(%s) fourth_min=%.0f plen=%.0f %s' % (
                        x, y, r['z'], r['hub_d'], r['center_d'], r['center_n'], r['route_min'], r['route_n'],
                        r['fourth_min'], r['plen'], 'ADAY' if ok else 'rota_yakin')
                else:
                    line = 'THAWROUTE GRID (%.0f,%.0f) nav=%s' % (x, y, r['nav'])
                print(line)
                out_lines.append(line)
            y += step
        x += step

try:
    saved = unreal.Paths.project_saved_dir()
    with open(os.path.join(saved, 'thaw_route_result.txt'), 'w') as f:
        f.write('\n'.join(out_lines))
except Exception as e:
    print('THAWROUTE yazilamadi: %s' % e)
print('THAWROUTE BITTI')
