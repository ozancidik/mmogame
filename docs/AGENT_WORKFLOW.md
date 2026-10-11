# MyProject: 12 uzman ajan ile Codex + Claude

Bu kurulum UE 5.7 / C++ MMORPG icin 12 mantiksal uzmanlik saglar. Codex ana
uygulama, Claude mimari ve bagimsiz ikinci inceleme icin kullanilir. Her iki
ortam da ayni rol sozlesmesini kullanabilir. On iki rolun surekli veya ayni
anda calismasi gerekmez; Codex icin acik alt ajan siniri alti, ana ajan harictir.

## Desteklenen yapilandirma ve yuklenme zamani

2026-10-11 tarihinde kurulu yerel calistirici:
`C:\Users\OZAN CIDIK\AppData\Local\OpenAI\Codex\bin\2e5e00daee91c61d\codex.exe`
`--version` sonucu: `codex-cli 0.162.0-alpha.17.2`.
`features list` sonucu `multi_agent stable true`. Bu komut model cagirmaz;
ozelligin durumunu gosterir, uzman dispatch edildigini kanitlamaz.

[Resmi OpenAI Subagents belgesi](https://learn.chatgpt.com/docs/agent-configuration/subagents)
Custom agents bolumu, proje kapsaminda `.codex/agents/*.toml` bagimsiz
dosyalarini ve zorunlu `name`, `description`, `developer_instructions`
alanlarini tanimlar. Dosya adi yerine `name` kimliktir. Burada bu format
kullanilir; `[agents.<role>.config_file]` kaydi gerekmez.
[Resmi Configuration Reference](https://learn.chatgpt.com/docs/config-file/config-reference)
`agents.enabled` (boolean), `agents.max_concurrent_threads_per_session`
(sayi; ana thread haric) ve `features.multi_agent` (boolean) alanlarini
belgeler. `.codex/config.toml` yalniz bu alanlari ayarlar. Model, provider,
izin, sandbox ve kullanici ayarlari degistirilmez. Security/performance rol
dosyalarinda `sandbox_mode = "read-only"` tercih edilir; parent runtime
izin override'lari bu varsayilani etkileyebilir. Talimat bir OS guvenlik siniri
degildir; gercek runtime izinlerini de kontrol et.

Codex'in proje config'ini yeni bir oturum baslatirken okumasini sagla; yeni
dosyalarin zaten acik sohbetin rol listesine kendiliginden eklendigini
varsayma. Mevcut oturumun config/rol katalogunu yeniden yukleme garantisi bu
kurulumda dogrulanmadi. Dosyalar kaydedildikten sonra yeni Codex oturumu ac;
gerekirse CLI veya uygulamayi yeniden baslat. Uygulama/CLI'nin aktif rol
listesini ve gercek spawn sonucunu kontrol et. Yeni ad taninmiyorsa, desteklenen
yerlesik `default` ajanini acik uzman talimatlari ve dosya sahipligiyle dispatch
et; bu durumu kayda yaz. Bunu yeni ozel rolun yuklendigine kanit sayma.

2026-10-11 kurulum oturumunda koordinatorun mevcut sohbetten kanonik
`agent_type=security` dispatch denemesi `unknown agent_type` dondurdu; child
olusturulmadi. Mevcut katalog durumu **NOT_RELOADED**. Koordinator uzmanlari
yerlesik `default` + acik uzman rol talimatlariyla gercekten dispatch etti.
Yeni oturumda kanonik rol adiyla dispatch bu tooling gorevinde **NOT_VERIFIED**;
kurulum dosyalari mevcut sohbeti yeniden yukledi diye raporlanmaz.

Claude tanimlari `.claude/agents/*.md` altinda YAML frontmatter (`name`,
`description`, `tools`, `model: inherit`) ve govdede uzman talimatlaridir.
[Resmi Claude Code subagents belgesi](https://code.claude.com/docs/en/sub-agents)
bu kesfi ve `model: inherit` kullanimini aciklar. Guncel belge, oturum basinda
var olan agents dizinindeki degisikliklerin watcher tarafindan algilandigini ve
sonraki delegation'da kullanildigini soyluyor. Yeni agents dizini, --add-dir
altindaki tanimlar veya --disable-slash-commands ile baslatilmis oturumlar icin
restart gerekebilir. Burada Claude sureci calistirilarak hot reload dogrulanmadi.
Guvenilir kurulum kontrolu icin yeni oturum acip rolun kesfedildigini ve
gercek Agent cagrisini dogrula; `/agents` davranisi surume gore degisir.
Orchestrator'un tools alani mevcut runtime araclarini miras alir; bir alt
ajan dispatch edemiyorsa gorev paketlerini ana oturuma dondurur.

## Roller, dosyalar ve eski adlar

Her satir icin `.codex/agents/<rol>.toml` ve `.claude/agents/<rol>.md` vardir.
Asagidaki alanlar otomatik yazma yetkisi degildir: kesin dosya sahipligini
orchestrator gorev manifestinde belirler.

| Rol | Sorumluluk / olasi alan | Mevcut rol bilgisinin yeniden kullanimi |
| --- | --- | --- |
| orchestrator | Plan, dispatch, sahiplik, bagimsiz kabul ve entegrasyon | Ana Codex/Claude oturumu |
| ue5-developer | Characters, Components, Skills, Inventory, UI C++ | mmo-developer |
| network | RPC, replication, GameState/ownership sozlesmesi | Claude ag-guvenlik-denetcisi inceleme listesi |
| backend | Store/Save/Mail, karakter verisi ve ekonomi kaliciligi | Mevcut kaynak mimarisi |
| qa-automation | Test senaryosu, self-test/runner, bagimsiz inceleme | qa-kontrolcu; Claude test-muhendisi |
| game-designer | XP/skill/PvP/PvE/ekonomi, atanmis docs/veri | Claude oyun-tasarimcisi, ekonomi-tasarimcisi, seviye-tasarimcisi |
| npc-ai | Mob/NPC/AI/quest, timer/aggro/leash/nav | Mevcut World ve quest mimarisi |
| security | Authority, replay, exploit, sir ve veri gizliligi; salt okunur | Claude ag-guvenlik-denetcisi |
| asset-integration | Editor ile Content, mesh, materyal, animasyon, OFPA | Editorun yonettigi ikili dosyalar |
| devops | Scripts/CI/target, build kilidi, server/deploy hazirligi | ue-build bilgisi, guvenli ortak kilit |
| performance | CPU/RAM/tick/FPS/replication olcum ve incelemesi; salt okunur | Gercek profiler kaniti |
| documentation | Teknik docs, API/semalar, gorev ve sonuc kayitlari | Claude belge-denetcisi |

Alias yukarida prosedurel rol eslesmesidir; Claude icin desteklenmeyen bir
`alias` frontmatter anahtari veya Codex icin ikinci kimlik tanimlanmaz.
Mevcut uc Codex ve dokuz Claude tanimi aynen korunur. Kanonik dosyalar,
eski tanimlardaki otomatik master commit/editor oldurme gibi genis talimatlari
task sinirlarina tasimayan giris noktalaridir. Alan bilgisi eski dosyadan
alinir; davranis ve sahiplik bu workflow ile gorev talimatindan gelir.
`metin-yazari` oyun metni/lore icin ek rol olarak kalir; teknik documentation
rolu onun yerine gecmez. Dolayisiyla dosya sayisi tam 12 ile sinirli degildir.

## Zorunlu gercek dispatch ve gorev akisi

Kullanicinin istegi: gameplay calismasi uzman ajanlar olmadan yapilmaz.
Orchestrator once `git status --short` okur, degisiklikleri ve sahipligi kayda
alir. Runtime'in sundugu gercek native multi_agent araciyla uzmani baslatir,
kimligini tutar, sonucunu bekler ve bagimsiz QA sonucunu toplar. Desteklenen
arac adlari resmi belgede `spawn_agent`, `send_input`, `wait_agent`,
`resume_agent`, `close_agent` olarak listelenir; cagri argumanlari aktif
runtime'in gercek semasindan alinir. Uydurma fonksiyon/CLI komutu kullanma.

Mevcut sohbet eski rol kataloguyla aciksa `default` + acik rol talimati gercek
dispatch icin gecerlidir. Gorev kaydinda `runtime type=default; specialist=...`
yaz; bunu kanonik rol adiyla native dispatch diye raporlama. Yapilandirma
dosyasi, tek ajan icinde rol taklidi veya worktree olusturmak dispatch degildir.
Runtime araci yoksa gameplay uygulamasina baslamadan engeli bildir.

Her gorev paketi: hedef, mode, base SHA, absolute worktree, kesin owned paths,
yasak dosyalar, authority/RPC/veri sozlesmesi, kabul kriterleri ve beklenen
rapor. Ajanin ana sohbeti otomatik devraldigini varsayma. Her yazara ayri
worktree ayir. Ortak dosyada tek yazar; diger uzman diff uzerinden onerir.
QA/security/performance okuma incelemeleri gelistirme boyunca paralel olabilir;
son kabul incelemesi entegrasyona aday tam diff/SHA uzerinde yapilir.

Tasarim gerekiyorsa game-designer ile basla. C++'i ue5-developer, RPC'yi
network, kaliciligi backend, NPC'yi npc-ai, Content'i asset-integration
sahiplenir. Her gameplay gorevinde uygulama uzmani + bagimsiz qa-automation
zorunludur. Combat/odul/trade/save icin network + security incelemesi;
olcekleme icin performance; tooling/server icin devops; sonunda documentation
kullan. On iki rolu gorevle ilgisi olmadan bos yere dispatch etme.

Codex ile Claude arasinda merkezi bir scheduler, daemon veya otomatik mesaj
servisi bu paketle kurulmaz. Orchestrator gorev kaydi ve exact diff/base SHA ile
diger runtime'a handoff yapar. Claude sureci/Agent sonucu yoksa `Claude:
NOT_RUN` yazar. Basarili config parse sonucunu Claude'un calistigi seklinde
sunmaz. Kok AGENTS.md routing'i parent koordinator yonetir; kok CLAUDE.md
degisikligi bu kurulum icin gerekli degildir.

## Worktree/task betikleri

PowerShell 5.1+, Git ve commiti olan bir repo gerekir. Betikler source
checkout'taki peer dosyalarini degistirmez. `NewAgentTask.ps1` safe slug ve rol,
ref, relative sahiplik ve canonical absolute yollarini dogrular; reparse/junction,
UNC/drive root, traversal, var olan hedef/branch ve checkout icindeki worktree
havuzunu reddeder. Ortak Git dizinindeki `agent-task-create.lock` handle'i
task allocation'i siralar. Git metadata'sina yeni branch/worktree kaydi ekler.

```powershell
# Once yalniz plani gor; -WhatIf dizin/branch/lock yaratmaz.
.\Scripts\NewAgentTask.ps1 -Task warrior-skills -Owner codex `
  -Role ue5-developer -Objective 'Warrior skill sozlesmesini uygula' `
  -OwnedPath 'Source/MyProject/Private/Skills','Source/MyProject/Public/Skills' -WhatIf

$task = .\Scripts\NewAgentTask.ps1 -Task warrior-skills -Owner codex `
  -Role ue5-developer -Objective 'Warrior skill sozlesmesini uygula' `
  -OwnedPath 'Source/MyProject/Private/Skills','Source/MyProject/Public/Skills'
.\Scripts\CheckAgentTask.ps1 -TaskManifest $task.TaskManifest
```

Varsayilan pool source checkout'un kardesindeki `<repo>-agent-worktrees`
dizinidir. Branch `<owner>/<task>-<owner>-<role>` olur. `-WorktreeRoot` ile baska
guvenli dis dizin, `-BaseRef` ile kesin commit/ref secilebilir; varsayilan HEAD.
Gorev worktree'sinde `docs/tasks/<task>.<owner>.<role>.agent-task.json` manifesti
ve ayni isimli `.md` kaydi CreateNew ile uretilir. Existing dosya ezilmez.
`docs/tasks/AGENT_TASK_TEMPLATE.md` kabul/handoff sablonudur.

**Kirli ana checkout otomatik kopyalanmaz.** HEAD'de henuz bulunmayan config,
ajanlar, betikler veya peer duzeltmeleri yeni worktree'de bulunmaz. Kullanici
rutin yerel commit'leri yetkilendirmistir; yalniz orchestrator, sahipligi belli
dosyalari isimleriyle commitleyip uygun base'i secer. Uzmanlar kendi kendine
commit atmaz; bu kisit ek bir kullanici onayi gerektirmez. Bu tooling uzmani
gorevi commit atmaz. Untracked config'i baska checkout'a sessizce kopyalama.
Bagimsiz Claude incelemesi icin koordinatorun aday committed base'ini veya
acikca verilen diff'i kullan; kirli ana HEAD'den acilan bos worktree'yi incelenen
degisiklik sanma.

`CheckAgentTask.ps1` manifesti, ortak Git dizinini, registered ayri worktree'yi,
branch/base ancestry'yi, committed/staged/unstaged/untracked degisiklikleri ve
sahipligi kontrol eder. Yalniz iki task kayit dosyasini scope disinda muaf tutar.
Ignored dosyalarin icerigini denetlemez, test/oynanis/dispatch kaniti uretmez.
Sahiplik deklaratiftir, OS sandbox'i degildir; manifesti elle genisletmek yerine
koordinatorle sahipligi yeniden kararlastir. Iki worktree'nin ayni kaynak dosyasini
yazmasini orkestrator onler; betik bir global dosya sahipligi veritabani tutmaz.

## Tek UE islemi ve derleme

Normal check build yapmaz. Kullanici kapsaminda build yetkisi varsa:

```powershell
.\Scripts\CheckAgentTask.ps1 -TaskManifest $task.TaskManifest -Build
```

Build sabit `MyProjectEditor Win64 Development`, hedef worktree'nin
`MyProject.uproject` yolu ve `-WaitMutex` kullanir. Yalniz gerekli engine
degisirse `-EngineRoot 'C:\Program Files\Epic Games\UE_5.7'` verilir. Arbitrary
komut veya shell string kabul edilmez. Tum worktree'lerde ortak Git dizinindeki
`agent-build.lock` FileShare.None handle'i build boyunca tutulur ve finally'de
birakilir; dosya kalabilir, stale dosyayi silme. Baska build varsa hemen hata verir.

Kilit altinda Win32_Process envanteri kontrol edilir. UnrealEditor (Cmd dahil),
LiveCoding, UnrealBuildTool, AutomationTool, MSBuild ve ilgili dotnet sureci varsa
build reddedilir. Process envanteri okunamazsa da build yapilmaz. Editor oldurme,
kaydedilmemis sahneyi kapatma veya otomatik Live Coding tetikleme yok. UE
calistirici/PIE/cook/asset editor isleri de devops tarafindan ayni kilit altinda
siralanmali; bu iki betik build disi UE otomasyonu calistirmaz.

Kilit yalniz bu protokolu kullanan islemleri siralar. Harici editorun kontrol ile
build arasinda baslamasini OS duzeyinde engellemez; ekip ayni protokole uyar.
UBT'nin -WaitMutex secenegi ek korumadir. Build hata kodu exception olur;
basari `PASS exit=0` diye raporlanir. Gercek stdout/log handoff'a eklenir.
Build sonucu PIE, multi-client replication veya dedicated server kaniti degildir.

Betikler commit, merge, push, deploy, reset, clean, process kill veya worktree
silme yapmaz. Yarim olusan worktree hata halinde inceleme icin tutulur. Temizlik
ve entegrasyon koordinatorun ayri gorevidir; otomatik cleanup komutu yoktur.

## Kabul ve bagimsiz QA icin kanit

Her teslimde exact degisen dosya allowlist'i, base/diff, actual agent kimlikleri,
test komutlari/exit/log, calistirilmayan alanlar ve acik riskleri ver. QA kendi
degisikligini onaylamaz; bagimsiz ajan son diff'i inceler. Bu kurulumda guvenli
kontroller: TOML parse + zorunlu alanlar, PowerShell AST parse, YAML frontmatter,
ve gecici fixture repo/worktree'de path/scope/lock/WhatIf senaryolaridir.
UE build, gameplay/Content duzeltmesi veya Claude sureci calistirmak bu tooling
kontrollerinin parcasi degildir. Yapilandirma kesfi icin resmi belge ve yerel
CLI kaniti, runtime dispatch icin gercek agent sonucu ayri raporlanir.
