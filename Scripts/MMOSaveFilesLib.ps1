# Kayit dosyasi yedekleme/dogrulama kutuphanesi (docs/DESIGN_BACKUP.md, dilim B0). Dot-source edilir:
#   . (Join-Path $PSScriptRoot 'MMOSaveFilesLib.ps1')
# Saf mantik + dosya sistemi yardimcilari; oyuna/Saved'e baglanti YOK, kendiliginden hicbir sey yazmaz
# (yalniz Write-MMOManifest verilen goruntu dizinine manifest.json yazar). Windows PowerShell 5.1 icin dogrulandi; PowerShell 7
# DOGRULANMADI (pwsh bu makinede yok; created_utc DateTime'a cevrilirse normalize edilir ama denenmedi).
# Dosya yalniz ASCII (5.1 BOM'suz UTF-8'i ANSI okur). Dosya ICERIKLERI asla yeniden yazilmaz: bayt okuma + hash.
#
# Dosya sinifi (Get-MMOSaveFileClass):
#   character = Saved/SaveGames/MMOSave_<ad>.sav
#   side      = basarim/gunluk/posta/irk .json (+ .tmp, posta icin .bak)
#   side-lock = .bad / .tmp.bad (karantina; irk .bad = secim kilidi, korunur)
#   ignored   = beyaz liste disi (neden: Reason)
# Irk dosyasi GERCEK adi: MMORace_<SlotKey("MMOSave_<ad>",64).ToLower()>.json = MMORace_mmosave_<ad>.json (MMORaceStore.cpp:42,
# AMMOCharacter::GetSaveSlotName). Grup anahtari 'mmosave_' onekini soyar; onceki varsayim MMORace_<ad>.json de geriye uyumlu kabul edilir.
# Butunluk (Get-MMOBytesIntegrity): .sav = boyut + GVAS basligi; .json/.bak = kati UTF-8 + JSON nesnesi + tam sayi "version" (1..1000)
# + tur basina zorunlu alanlar; .tmp = ayni denetim ama bozuksa valid=true notes=tmp_unparsable (yarim yazim beklenir); kilit dosyalari
# valid=true notes=lock_file. Her dosya BELLEGE TEK KEZ okunur (Read-MMOFileBytes); boyut/hash/gecerlilik o diziden (TOCTOU).
# NOT: .sav icin yalniz baslik denetlenir; oyun anlamli yukleme dogrulamasi YOK (DESIGN_BACKUP R6).

$script:MMOSvMinSavBytes = 64          # [PLACEHOLDER] DESIGN_BACKUP 4.3
$script:MMOSvMaxNameLen = 32           # MMONameRules::SlotKey MaxLen
$script:MMOSvMaxRaceKeyLen = 64        # MMORaceStore.cpp:42 SlotKey(..., 64)
$script:MMOSvMaxRelPath = 200
$script:MMOSvMaxFileBytes = 8388608    # 8 MB: .sav / JSON / yan dosya ust siniri (sabit)
$script:MMOSvMaxManifestBytes = 8388608
$script:MMOSvMinVersion = 1
$script:MMOSvMaxVersion = 1000
$script:MMOSvManifestFormat = 1
$script:MMOSvToolName = 'MMOSaveFilesLib/2'
$script:MMOSvNativeOk = $null
$script:MMOSvZeroSha = ('0' * 64)        # icerigi alinamayan (read_error/too_large) giris icin "icerik yok" ozeti
$script:MMOSvSuperscripts = ([string][char]0x00B9) + [char]0x00B2 + [char]0x00B3
$script:MMOSvDeviceRx = '^(CON|PRN|AUX|NUL|CONIN\$|CONOUT\$|CLOCK\$|COM[0-9' + $script:MMOSvSuperscripts + ']|LPT[0-9' + $script:MMOSvSuperscripts + '])$'

# ---- yol sozdizimi ------------------------------------------------------------------------------------------------------
function Get-MMORelPathProblem {
    # Goreli yol (manifest/komut satiri girdisi) sozdizimi sorunu: $null = guvenli, aksi halde neden kodu.
    param([AllowNull()][AllowEmptyString()][string]$RelPath)
    if ([string]::IsNullOrEmpty($RelPath)) { return 'empty' }
    if ($RelPath.Length -gt $script:MMOSvMaxRelPath) { return 'too_long' }
    foreach ($ch in $RelPath.ToCharArray()) {
        $c = [int]$ch
        if ($c -lt 32 -or $c -eq 127) { return 'control_char' }
    }
    if ($RelPath.IndexOf('\') -ge 0) { return 'backslash' }
    if ($RelPath.IndexOf(':') -ge 0) { return 'colon' }
    if ($RelPath.IndexOfAny([char[]]'<>"|?*') -ge 0) { return 'invalid_char' }
    if ($RelPath.StartsWith('/')) { return 'rooted' }
    foreach ($seg in $RelPath.Split('/')) {
        if ($seg.Length -eq 0) { return 'empty_segment' }
        if ($seg -match '^\.+$') {
            if ($seg -eq '.' -or $seg -eq '..') { return 'dot_segment' }
            return 'trailing_dot_space'
        }
        if ($seg.EndsWith('.') -or $seg.EndsWith(' ')) { return 'trailing_dot_space' }
        $base = $seg
        $dot = $seg.IndexOf('.')
        if ($dot -ge 0) { $base = $seg.Substring(0, $dot) }
        if ($base.TrimEnd(' ') -match $script:MMOSvDeviceRx) { return 'device_name' }
    }
    return $null
}

function Test-MMOSafeRelPath {
    param([AllowNull()][AllowEmptyString()][string]$RelPath)
    return ($null -eq (Get-MMORelPathProblem $RelPath))
}

# ---- mutlak yol + dosya sistemi durumu (fail-closed) -------------------------------------------------------------------
function ConvertTo-MMOFullPath {
    # Tum genel girisler yolu burada mutlaklastirir. Goreli yol PowerShell KONUMUNA gore cozulur ([Environment]::CurrentDirectory
    # ile Join-Path/Get-ChildItem konumu farkli olabilir; .NET API'leri ve cmdlet'ler ayni dosyayi gormeli). Gecersiz = throw 'path_invalid'.
    param([AllowNull()][AllowEmptyString()][string]$Path)
    if ([string]::IsNullOrEmpty($Path)) { throw 'path_empty' }
    if ($Path.IndexOf([char]0) -ge 0) { throw 'path_invalid' }
    if ($Path.StartsWith('\\.\') -or $Path.StartsWith('\\?\')) { throw 'path_invalid' }
    if ($Path -match '^[A-Za-z]:([^\\/]|$)') { throw 'path_invalid' }
    try {
        if ([IO.Path]::IsPathRooted($Path)) { return [IO.Path]::GetFullPath($Path) }
        $base = (Get-Location -PSProvider FileSystem).ProviderPath
        return [IO.Path]::GetFullPath([IO.Path]::Combine($base, $Path))
    } catch { throw 'path_invalid' }
}

function Get-MMOInnerException {
    param($ErrorRecord)
    $ex = $ErrorRecord.Exception
    while ($null -ne $ex.InnerException) { $ex = $ex.InnerException }
    return $ex
}

function Test-MMOPathUnder {
    # $Path kanonik olarak $Root'un ALTINDA mi (ayrac sinirli; 'C:\ab' , 'C:\a' altinda degil). -AllowEqual: esitlik de kabul.
    # Yol cozulemezse varsayilan $false ("altinda degil": Resolve-MMOSafePath path_escape icin guvenli yon). Sonucu bir IHLAL denetimi
    # olarak kullananlar (yedek koku Saved/repo icinde mi) -ErrorIsViolation verir: cozulemeyen yol = ihlal ($true), fail-closed (D2).
    param([string]$Path, [string]$Root, [switch]$AllowEqual, [switch]$ErrorIsViolation)
    try {
        $p = (ConvertTo-MMOFullPath $Path).TrimEnd('\')
        $r = (ConvertTo-MMOFullPath $Root).TrimEnd('\')
    } catch { return [bool]$ErrorIsViolation }
    if ($p.Equals($r, [StringComparison]::OrdinalIgnoreCase)) { return [bool]$AllowEqual }
    return $p.StartsWith($r + '\', [StringComparison]::OrdinalIgnoreCase)
}

function Initialize-MMONative {
    # kernel32 P/Invoke (sabit C# kaynagi, kullanici girdisi derlenmez): GetFinalPathNameByHandleW (gercek yol, junction/symlink/8.3 cozer) ve
    # GetDiskFreeSpaceExW (bos alan; UNC dahil). Derlenemezse $false (cagiranlar fail-closed ya da belgelenmis dusus yapar).
    if ($null -ne $script:MMOSvNativeOk) { return $script:MMOSvNativeOk }
    $ok = $false
    try {
        if (-not ('MMOSaveNative' -as [type])) {
            Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
using System.Text;
public static class MMOSaveNative {
    [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    static extern IntPtr CreateFileW(string name, uint access, uint share, IntPtr sec, uint disp, uint flags, IntPtr tmpl);
    [DllImport("kernel32.dll", SetLastError = true)]
    static extern bool CloseHandle(IntPtr h);
    [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    static extern uint GetFinalPathNameByHandleW(IntPtr h, StringBuilder buf, uint len, uint flags);
    [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    static extern bool GetDiskFreeSpaceExW(string dir, out ulong avail, out ulong total, out ulong free);
    public static string FinalPath(string path) {
        IntPtr h = CreateFileW(path, 0, 7, IntPtr.Zero, 3, 0x02000000, IntPtr.Zero);
        if (h == new IntPtr(-1)) return null;
        try {
            StringBuilder sb = new StringBuilder(2048);
            uint n = GetFinalPathNameByHandleW(h, sb, (uint)sb.Capacity, 0);
            if (n == 0) return null;
            if (n >= sb.Capacity) { sb = new StringBuilder((int)n + 2); n = GetFinalPathNameByHandleW(h, sb, (uint)sb.Capacity, 0); if (n == 0) return null; }
            return sb.ToString();
        } finally { CloseHandle(h); }
    }
    public static long FreeBytes(string dir) {
        ulong a, t, f;
        if (!GetDiskFreeSpaceExW(dir, out a, out t, out f)) return -1;
        return a > (ulong)long.MaxValue ? long.MaxValue : (long)a;
    }
}
'@
        }
        $ok = ('MMOSaveNative' -as [type]) -ne $null
    } catch { $ok = $false }
    $script:MMOSvNativeOk = $ok
    return $ok
}

function Get-MMOFinalPath {
    # Gercek (junction / sembolik baglanti / 8.3 kisa ad cozulmus) tam yol. Yol henuz yoksa var olan en yakin atanin gercek yolu + kalan
    # bolum. Cozulemezse $null (cagiran karar verir; BackupSaves -AllowRootAncestorReparse'te fail-closed).
    # SINIR: anlik goruntudur (TOCTOU: cozumden sonra baglanti degistirilebilir); ag/yer imi surucularinda sonuc sunucuya baglidir.
    param([string]$Path)
    try { $full = (ConvertTo-MMOFullPath $Path).TrimEnd('\') } catch { return $null }
    if (-not (Initialize-MMONative)) { return $null }
    $rest = New-Object System.Collections.ArrayList
    $cur = $full
    while ($true) {
        $a = Get-MMOAttributes $cur
        if ($a.State -eq 'ok') { break }
        if ($a.State -ne 'missing') { return $null }
        $leaf = [IO.Path]::GetFileName($cur)
        $parent = [IO.Path]::GetDirectoryName($cur)
        if ([string]::IsNullOrEmpty($leaf) -or [string]::IsNullOrEmpty($parent)) { return $null }
        $rest.Insert(0, $leaf)
        $cur = $parent
    }
    $fp = $null
    try { $fp = [MMOSaveNative]::FinalPath($cur) } catch { $fp = $null }
    if ([string]::IsNullOrEmpty($fp)) { return $null }
    if ($fp.StartsWith('\\?\UNC\', [StringComparison]::OrdinalIgnoreCase)) { $fp = '\\' + $fp.Substring(8) }
    elseif ($fp.StartsWith('\\?\')) { $fp = $fp.Substring(4) }
    $fp = $fp.TrimEnd('\')
    foreach ($seg in $rest) { $fp = $fp + '\' + $seg }
    return $fp
}

function Get-MMOFreeBytes {
    # Yolun bulundugu birimdeki kullanilabilir bos bayt (yol yoksa var olan en yakin ata). Olculemezse -1.
    param([string]$Path)
    try { $cur = (ConvertTo-MMOFullPath $Path).TrimEnd('\') } catch { return -1L }
    if (-not (Initialize-MMONative)) { return -1L }
    while ($true) {
        $a = Get-MMOAttributes $cur
        if ($a.State -eq 'ok' -and (($a.Attributes -band [IO.FileAttributes]::Directory) -ne 0)) { break }
        if ($a.State -ne 'missing' -and $a.State -ne 'ok') { return -1L }
        # yol yok ya da dizin degil (dosya): ayni birimdeki ust dizine bak
        $parent = [IO.Path]::GetDirectoryName($cur)
        if ([string]::IsNullOrEmpty($parent)) { return -1L }
        $cur = $parent
    }
    try { return [long][MMOSaveNative]::FreeBytes($cur + '\') } catch { return -1L }
}

function Get-MMOAttributes {
    # State: 'ok' (Attributes dolu) | 'missing' (dosya/dizin yok) | 'inaccessible' (okunamadi/gecersiz yol: BILINMIYOR, guvenli sayilmaz).
    param([string]$Path)
    try { $full = ConvertTo-MMOFullPath $Path } catch { return [pscustomobject]@{ State = 'inaccessible'; Attributes = $null } }
    try {
        $a = [IO.File]::GetAttributes($full)
        return [pscustomobject]@{ State = 'ok'; Attributes = $a }
    } catch {
        $ex = Get-MMOInnerException $_
        if (($ex -is [IO.FileNotFoundException]) -or ($ex -is [IO.DirectoryNotFoundException])) {
            return [pscustomobject]@{ State = 'missing'; Attributes = $null }
        }
        return [pscustomobject]@{ State = 'inaccessible'; Attributes = $null }
    }
}

function Test-MMOReparse {
    # Junction/sembolik bag/baska reparse point mi. Var olmayan yol = $false (varlik ayrica denetlenir).
    # Erisilemeyen yol (izin, gecersiz ad) = $true: fail-closed, reddedilir.
    param([string]$Path)
    $a = Get-MMOAttributes $Path
    if ($a.State -eq 'inaccessible') { return $true }
    if ($a.State -eq 'missing') { return $false }
    return (($a.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0)
}

function Get-MMOReparseAncestor {
    # $Path'in (kendisi haric) ilk reparse point / erisilemeyen atasi; yoksa $null. Olmayan atalar atlanir.
    param([string]$Path)
    $full = (ConvertTo-MMOFullPath $Path).TrimEnd('\')
    $p = [IO.Path]::GetDirectoryName($full)
    while (-not [string]::IsNullOrEmpty($p)) {
        $a = Get-MMOAttributes $p
        if ($a.State -eq 'inaccessible') { return $p }
        if ($a.State -eq 'ok' -and (($a.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0)) { return $p }
        $p = [IO.Path]::GetDirectoryName($p)
    }
    return $null
}

function Assert-MMORootSafe {
    # Kok dizin: var, erisilebilir, reparse degil; atalarinda reparse yok (bilincli istisna: -AllowRootAncestorReparse). Tam yolu dondurur.
    param([string]$Root, [switch]$AllowRootAncestorReparse)
    try { $rootFull = (ConvertTo-MMOFullPath $Root).TrimEnd('\') } catch { throw 'root_invalid' }
    $a = Get-MMOAttributes $rootFull
    if ($a.State -eq 'missing') { throw 'root_missing' }
    if ($a.State -ne 'ok') { throw 'root_inaccessible' }
    if (($a.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) { throw 'reparse_root' }
    if (-not $AllowRootAncestorReparse) {
        $anc = Get-MMOReparseAncestor $rootFull
        if ($null -ne $anc) { throw ('reparse_root_ancestor:' + $anc) }
    }
    return $rootFull
}

function Resolve-MMOSafePath {
    # Kok altinda guvenli tam yol dondurur ya da throw eder (mesaj on eki = neden kodu). Beyaz liste siniflandirmasi ZORUNLU.
    param([string]$Root, [string]$RelPath, [switch]$MustExist, [switch]$AllowRootAncestorReparse)
    $problem = Get-MMORelPathProblem $RelPath
    if ($null -ne $problem) { throw ('unsafe_relpath:' + $problem) }
    $cls = Get-MMOSaveFileClass $RelPath
    if ($cls.Class -eq 'ignored') { throw ('not_whitelisted:' + $cls.Reason) }
    $rootFull = Assert-MMORootSafe -Root $Root -AllowRootAncestorReparse:$AllowRootAncestorReparse
    $full = [IO.Path]::GetFullPath([IO.Path]::Combine($rootFull, $RelPath.Replace('/', '\')))
    if (-not (Test-MMOPathUnder -Path $full -Root $rootFull)) { throw 'path_escape' }
    $cur = $rootFull
    foreach ($seg in $RelPath.Split('/')) {
        $cur = $cur + '\' + $seg
        if (Test-MMOReparse $cur) { throw ('reparse_component:' + $seg) }
    }
    if ($MustExist -and -not [IO.File]::Exists($full)) { throw 'missing' }
    return $full
}

# ---- ad kurallari ------------------------------------------------------------------------------------------------------
function Test-MMOPlayerName {
    # MMONameRules: yalniz ASCII [A-Za-z0-9 _-], 1..32, bas/son bosluk yok (kirpilip baska ada donusturulmez).
    param([AllowNull()][string]$Name)
    if ($null -eq $Name) { return $false }
    if ($Name -cnotmatch '^[A-Za-z0-9 _-]{1,32}\z') { return $false }
    if ($Name.StartsWith(' ') -or $Name.EndsWith(' ')) { return $false }
    return $true
}

function ConvertTo-MMOCanonicalKey {
    # MMONameRules::CanonicalKey: bosluk -> '_', ASCII kucuk harf. Gecersiz ad = $null.
    param([AllowNull()][string]$Name)
    if (-not (Test-MMOPlayerName $Name)) { return $null }
    return $Name.Replace(' ', '_').ToLowerInvariant()
}

function Test-MMOFileKey {
    # Posta/irk dosya anahtari: zaten kanonik (ASCII kucuk harf, '_' '-'). Posta en cok 32, irk en cok 64 karakter.
    param([AllowNull()][string]$Key, [int]$MaxLen = 32)
    if ($null -eq $Key) { return $false }
    return ($Key -cmatch ('^[a-z0-9_-]{1,' + $MaxLen + '}\z'))
}

function New-MMOClassResult {
    param([string]$Class, [string]$Kind, [string]$Group, [string]$Suffix, [string]$Reason)
    return [pscustomobject]@{ Class = $Class; Kind = $Kind; Group = $Group; Suffix = $Suffix; Reason = $Reason }
}

function Get-MMOSaveFileClass {
    # Goreli yol (Saved'e gore, '/' ayracli, ornek 'SaveGames/MMOSave_Ali.sav') -> sinif.
    param([AllowNull()][AllowEmptyString()][string]$RelPath)
    $problem = Get-MMORelPathProblem $RelPath
    if ($null -ne $problem) { return (New-MMOClassResult 'ignored' '' '' '' ('unsafe_path:' + $problem)) }
    $parts = $RelPath.Split('/')
    if ($parts.Count -ne 2) { return (New-MMOClassResult 'ignored' '' '' '' 'depth') }
    $dir = $parts[0]
    $file = $parts[1]
    $kind = $null
    $rx = $null
    $nameMode = 'player'
    if ($dir -ceq 'SaveGames') {
        $kind = 'character'
        $rx = '^MMOSave_(?<n>.+)\.sav\z'
    } elseif ($dir -ceq 'MMOAchievements') {
        $kind = 'achievement'
        $rx = '^MMOSave_(?<n>.+?)\.json(?<s>\.tmp\.bad|\.tmp|\.bad)?\z'
    } elseif ($dir -ceq 'MMODaily') {
        $kind = 'daily'
        $rx = '^MMOSave_(?<n>.+?)\.json(?<s>\.tmp\.bad|\.tmp|\.bad)?\z'
    } elseif ($dir -ceq 'MMOMail') {
        $kind = 'mail'
        $nameMode = 'key'
        $rx = '^MMOMail_(?<n>.+?)\.json(?<s>\.tmp\.bad|\.tmp|\.bad|\.bak)?\z'
    } elseif ($dir -ceq 'MMORace') {
        $kind = 'race'
        $nameMode = 'racekey'
        $rx = '^MMORace_(?<n>.+?)\.json(?<s>\.tmp\.bad|\.tmp|\.bad)?\z'
    } else {
        return (New-MMOClassResult 'ignored' '' '' '' 'not_whitelisted_dir')
    }
    if (-not ($file -cmatch $rx)) { return (New-MMOClassResult 'ignored' $kind '' '' 'not_whitelisted_name') }
    $name = $Matches['n']
    $suffix = ''
    if ($Matches.ContainsKey('s')) { $suffix = [string]$Matches['s'] }
    $group = $null
    if ($nameMode -eq 'player') {
        if (-not (Test-MMOPlayerName $name)) { return (New-MMOClassResult 'ignored' $kind '' '' 'name_rule') }
        $group = ConvertTo-MMOCanonicalKey $name
    } elseif ($nameMode -eq 'racekey') {
        if (-not (Test-MMOFileKey $name $script:MMOSvMaxRaceKeyLen)) { return (New-MMOClassResult 'ignored' $kind '' '' 'name_rule') }
        $group = $name
        if ($name -ceq 'mmosave_') { return (New-MMOClassResult 'ignored' $kind '' '' 'name_rule') }
        if ($name -cmatch '^mmosave_(?<r>.+)\z') { $group = $Matches['r'] }
    } else {
        if (-not (Test-MMOFileKey $name 32)) { return (New-MMOClassResult 'ignored' $kind '' '' 'name_rule') }
        $group = $name
    }
    $class = 'side'
    if ($kind -eq 'character') { $class = 'character' }
    elseif ($suffix -eq '.bad' -or $suffix -eq '.tmp.bad') { $class = 'side-lock' }
    return (New-MMOClassResult $class $kind $group $suffix '')
}

function Get-MMOSlotGroups {
    # Beyaz listedeki dosyalari slot grubuna (kanonik anahtar) toplar. Donus: grup anahtari -> {Key; Items[]}.
    param([string[]]$RelPaths)
    $groups = [ordered]@{}
    foreach ($rel in $RelPaths) {
        $c = Get-MMOSaveFileClass $rel
        if ($c.Class -eq 'ignored') { continue }
        if (-not $groups.Contains($c.Group)) {
            $groups[$c.Group] = [pscustomobject]@{ Key = $c.Group; Items = (New-Object System.Collections.ArrayList) }
        }
        [void]$groups[$c.Group].Items.Add([pscustomobject]@{ Path = $rel; Class = $c.Class; Kind = $c.Kind; Suffix = $c.Suffix })
    }
    return $groups
}

function Find-MMOCollisions {
    # Windows'ta ayni dosya / baska makinede iki dosya olacak cakismalar: ayni (tur, sonek, kanonik grup) icin birden cok
    # farkli yol (MMOSave_ALI.sav + MMOSave_ali.sav, 'Ali Baba' + 'Ali_Baba') ya da beyaz liste disinda yalniz harf farkli yollar.
    # Sonuc BOS olmak zorunda: New-MMOManifest ve Read-MMOManifest cakismada hata verir.
    param([string[]]$RelPaths)
    $buckets = @{}
    foreach ($rel in $RelPaths) {
        $c = Get-MMOSaveFileClass $rel
        if ($c.Class -eq 'ignored') { $key = 'i|' + $rel.ToLowerInvariant() }
        else { $key = 'k|' + $c.Kind + '|' + $c.Suffix + '|' + $c.Group }
        if (-not $buckets.ContainsKey($key)) { $buckets[$key] = New-Object 'System.Collections.Generic.HashSet[string]' ([StringComparer]::Ordinal) }
        [void]$buckets[$key].Add($rel)
    }
    $out = New-Object System.Collections.ArrayList
    foreach ($k in $buckets.Keys) {
        if ($buckets[$k].Count -gt 1) {
            $list = [string[]]@($buckets[$k])
            [Array]::Sort($list, [StringComparer]::Ordinal)
            [void]$out.Add($list)
        }
    }
    return $out.ToArray()
}

function Get-MMORestoreSelection {
    # Geri yukleme politikasi (B3 kullanir): yan kilit/.tmp/.bak/.bad (soneki olan her yan dosya) VARSAYILAN geri yuklenmez;
    # yalniz -IncludeSideFiles ile. Beyaz liste disi her sey atlanir. Donus: Restore[] (yollar), Skipped[] ({Path; Reason}).
    # O6 (B3 hazirligi): valid=false girisler (bozuk kaynak) ve notes=unstable (canli yedekte kararsiz) girisler VARSAYILAN geri yuklenmez
    # (Skipped: invalid_entry | unstable); zorlamak icin ayri ACIK bayrak -IncludeInvalid: bu girisler Restore'a girer, Forced'a yazilir ve
    # konsola [MMO_RESTORE_FORCE] satiri basilir (loglanir). Icerigi hic alinamayan girisler (notes=read_error|too_large: dosya goruntude YOK)
    # bayrakla da geri yuklenemez (Skipped: no_content). -CollisionPaths: manifestin bildirdigi cakisan yollar ('collision', ret B3'te).
    param($Entries, [switch]$IncludeSideFiles, [switch]$IncludeInvalid, [string[]]$CollisionPaths = @())
    $restore = New-Object System.Collections.ArrayList
    $skipped = New-Object System.Collections.ArrayList
    $forced = New-Object System.Collections.ArrayList
    $collSet = New-Object 'System.Collections.Generic.HashSet[string]' ([StringComparer]::Ordinal)
    foreach ($cp in @($CollisionPaths)) { [void]$collSet.Add([string]$cp) }
    foreach ($e in @($Entries)) {
        $path = [string]$e.path
        $c = Get-MMOSaveFileClass $path
        if ($c.Class -eq 'ignored') { [void]$skipped.Add([pscustomobject]@{ Path = $path; Reason = 'not_whitelisted' }); continue }
        if ($collSet.Contains($path)) { [void]$skipped.Add([pscustomobject]@{ Path = $path; Reason = 'collision' }); continue }
        if ($c.Suffix -ne '' -and -not $IncludeSideFiles) { [void]$skipped.Add([pscustomobject]@{ Path = $path; Reason = 'side_file_requires_flag' }); continue }
        $notes = [string](Get-MMOMember $e 'notes')
        $ev = Get-MMOMember $e 'valid'
        if ($notes -ceq 'read_error' -or $notes -ceq 'too_large') { [void]$skipped.Add([pscustomobject]@{ Path = $path; Reason = 'no_content' }); continue }
        $why = ''
        if ($notes -ceq 'unstable') { $why = 'unstable' } elseif ($ev -eq $false) { $why = 'invalid_entry' }
        if ($why -ne '') {
            if (-not $IncludeInvalid) { [void]$skipped.Add([pscustomobject]@{ Path = $path; Reason = $why }); continue }
            [void]$forced.Add([pscustomobject]@{ Path = $path; Reason = $why })
            Write-Host ('[MMO_RESTORE_FORCE] path=' + $path + ' reason=' + $why)
        }
        [void]$restore.Add($path)
    }
    return [pscustomobject]@{ Restore = [string[]]@($restore.ToArray()); Skipped = @($skipped.ToArray()); Forced = @($forced.ToArray()) }
}

# ---- bayt okuma + hash + butunluk (dosya TEK KEZ okunur) --------------------------------------------------------------
function Read-MMOFileBytes {
    # Dosyayi tek seferde bellege okur. Throw kodlari: path_invalid, reparse_file, file_missing, file_unreadable, too_large, changed_during_read.
    # Okuma sirasinda buyuyen/kuculen dosya reddedilir. B3 yazimi AYNI diziyi kullanir (hash ile yazilan bayt ayni olsun).
    param([string]$Path, [long]$MaxBytes = $script:MMOSvMaxFileBytes)
    $full = ConvertTo-MMOFullPath $Path
    if (Test-MMOReparse $full) { throw 'reparse_file' }
    $fs = $null
    try {
        try { $fs = [IO.File]::Open($full, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::ReadWrite) } catch {
            $ex = Get-MMOInnerException $_
            if (($ex -is [IO.FileNotFoundException]) -or ($ex -is [IO.DirectoryNotFoundException])) { throw 'file_missing' }
            throw 'file_unreadable'
        }
        $len = $fs.Length
        if ($len -gt $MaxBytes) { throw 'too_large' }
        $buf = New-Object byte[] ([int]$len)
        $off = 0
        while ($off -lt $len) {
            $n = $fs.Read($buf, $off, [int]($len - $off))
            if ($n -le 0) { throw 'changed_during_read' }
            $off += $n
        }
        if ($fs.ReadByte() -ge 0) { throw 'changed_during_read' }
    } finally { if ($null -ne $fs) { $fs.Dispose() } }
    return , $buf
}

function ConvertTo-MMOHex {
    param([byte[]]$Bytes)
    return ([BitConverter]::ToString($Bytes).Replace('-', '').ToLowerInvariant())
}

function Get-MMOBytesSha256 {
    param([byte[]]$Bytes)
    $sha = [Security.Cryptography.SHA256]::Create()
    try { return (ConvertTo-MMOHex ($sha.ComputeHash($Bytes))) } finally { $sha.Dispose() }
}

function Get-MMOFileSha256 {
    param([string]$Path)
    return (Get-MMOBytesSha256 (Read-MMOFileBytes $Path))
}

function Test-MMOSavBytes {
    # .sav: boyut >= MinBytes ve ilk 4 bayt 'GVAS'. Oyun anlamli dogrulama DEGIL.
    param([byte[]]$Bytes, [int]$MinBytes = $script:MMOSvMinSavBytes)
    if ($Bytes.Length -gt $script:MMOSvMaxFileBytes) { return [pscustomobject]@{ Valid = $false; Reason = 'too_large'; Warnings = @() } }
    if ($Bytes.Length -lt $MinBytes -or $Bytes.Length -lt 4) { return [pscustomobject]@{ Valid = $false; Reason = 'too_small'; Warnings = @() } }
    if ($Bytes[0] -ne 0x47 -or $Bytes[1] -ne 0x56 -or $Bytes[2] -ne 0x41 -or $Bytes[3] -ne 0x53) {
        return [pscustomobject]@{ Valid = $false; Reason = 'bad_magic'; Warnings = @() }
    }
    return [pscustomobject]@{ Valid = $true; Reason = ''; Warnings = @() }
}

function Test-MMOSavFile {
    param([string]$Path, [int]$MinBytes = $script:MMOSvMinSavBytes)
    try { $bytes = Read-MMOFileBytes $Path } catch {
        if ($_.Exception.Message -eq 'too_large') { return [pscustomobject]@{ Valid = $false; Reason = 'too_large'; Warnings = @() } }
        throw
    }
    return (Test-MMOSavBytes -Bytes $bytes -MinBytes $MinBytes)
}

function ConvertFrom-MMOUtf8Bytes {
    # Kati UTF-8 (gecersiz bayt = throw 'bad_utf8'). UTF-8 BOM'u soyar ve HasBom ile bildirir.
    param([byte[]]$Bytes)
    $offset = 0
    $bom = $false
    if ($Bytes.Length -ge 3 -and $Bytes[0] -eq 0xEF -and $Bytes[1] -eq 0xBB -and $Bytes[2] -eq 0xBF) { $offset = 3; $bom = $true }
    $enc = New-Object System.Text.UTF8Encoding($false, $true)
    try { $text = $enc.GetString($Bytes, $offset, $Bytes.Length - $offset) } catch { throw 'bad_utf8' }
    return [pscustomobject]@{ Text = $text; HasBom = $bom }
}

function Read-MMOUtf8Text {
    param([string]$Path)
    return (ConvertFrom-MMOUtf8Bytes (Read-MMOFileBytes $Path))
}

function Get-MMOJsonSchema {
    # Dizin (tur) basina zorunlu ust duzey alanlar ve tipleri; kaynak: yazan C++ kodu (Achievement/Daily/MailStore/RaceStore).
    param([string]$Kind)
    switch ($Kind) {
        'achievement' { return [ordered]@{ unlocked = 'array' } }
        'daily' { return [ordered]@{ day = 'number'; bounties = 'array' } }
        'mail' { return [ordered]@{ mails = 'array' } }
        'race' { return [ordered]@{ race = 'number' } }
        default { return [ordered]@{} }
    }
}

function Test-MMOJsonBytes {
    # JSON nesnesi + tam sayi "version" (1..1000) + tur sozlesmesi. UTF-8 BOM = gecerli ama uyari.
    param([byte[]]$Bytes, [string]$Kind = '')
    $warn = New-Object System.Collections.ArrayList
    if ($Bytes.Length -gt $script:MMOSvMaxFileBytes) { return [pscustomobject]@{ Valid = $false; Reason = 'too_large'; Warnings = @() } }
    try { $r = ConvertFrom-MMOUtf8Bytes $Bytes } catch {
        return [pscustomobject]@{ Valid = $false; Reason = 'bad_utf8'; Warnings = @() }
    }
    if ($r.HasBom) { [void]$warn.Add('utf8_bom') }
    $text = $r.Text
    if ($text -match '^\s*$') { return [pscustomobject]@{ Valid = $false; Reason = 'empty'; Warnings = @($warn.ToArray()) } }
    if ($text.IndexOf([char]0) -ge 0) { return [pscustomobject]@{ Valid = $false; Reason = 'nul_char'; Warnings = @($warn.ToArray()) } }
    if ($text.TrimStart()[0] -ne '{') { return [pscustomobject]@{ Valid = $false; Reason = 'not_object'; Warnings = @($warn.ToArray()) } }
    try { $obj = $text | ConvertFrom-Json -ErrorAction Stop } catch {
        return [pscustomobject]@{ Valid = $false; Reason = 'parse_error'; Warnings = @($warn.ToArray()) }
    }
    $prop = $obj.PSObject.Properties['version']
    if ($null -eq $prop) { return [pscustomobject]@{ Valid = $false; Reason = 'no_version'; Warnings = @($warn.ToArray()) } }
    $v = $prop.Value
    $isInt = ($v -is [int]) -or ($v -is [long])
    if (-not $isInt -and (($v -is [double]) -or ($v -is [decimal]))) { $isInt = ([math]::Floor([double]$v) -eq [double]$v) }
    if (-not $isInt) { return [pscustomobject]@{ Valid = $false; Reason = 'version_not_integer'; Warnings = @($warn.ToArray()) } }
    if ([double]$v -lt $script:MMOSvMinVersion -or [double]$v -gt $script:MMOSvMaxVersion) {
        return [pscustomobject]@{ Valid = $false; Reason = 'version_out_of_range'; Warnings = @($warn.ToArray()) }
    }
    $schema = Get-MMOJsonSchema $Kind
    foreach ($name in $schema.Keys) {
        $fp = $obj.PSObject.Properties[$name]
        if ($null -eq $fp) { return [pscustomobject]@{ Valid = $false; Reason = ('missing_field:' + $name); Warnings = @($warn.ToArray()) } }
        $fv = $fp.Value
        $okType = $false
        if ($schema[$name] -eq 'array') { $okType = ($fv -is [array]) }
        elseif ($schema[$name] -eq 'number') { $okType = ($fv -is [int]) -or ($fv -is [long]) -or ($fv -is [double]) -or ($fv -is [decimal]) }
        if (-not $okType) { return [pscustomobject]@{ Valid = $false; Reason = ('bad_field_type:' + $name); Warnings = @($warn.ToArray()) } }
    }
    return [pscustomobject]@{ Valid = $true; Reason = ''; Warnings = @($warn.ToArray()) }
}

function Test-MMOJsonFile {
    param([string]$Path, [string]$Kind = '')
    try { $bytes = Read-MMOFileBytes $Path } catch {
        $m = $_.Exception.Message
        if ($m -ne 'too_large') { $m = 'read_error' }
        return [pscustomobject]@{ Valid = $false; Reason = $m; Warnings = @() }
    }
    return (Test-MMOJsonBytes -Bytes $bytes -Kind $Kind)
}

function Get-MMOBytesIntegrity {
    # Sinif bilgisine gore butunluk karari, bellekteki diziden. Donus: Valid (bool), Notes (string).
    param([byte[]]$Bytes, $ClassInfo)
    if ($ClassInfo.Class -eq 'character') {
        $r = Test-MMOSavBytes -Bytes $Bytes
        return [pscustomobject]@{ Valid = $r.Valid; Notes = $r.Reason }
    }
    if ($ClassInfo.Class -eq 'side-lock') {
        return [pscustomobject]@{ Valid = $true; Notes = 'lock_file' }
    }
    $r = Test-MMOJsonBytes -Bytes $Bytes -Kind $ClassInfo.Kind
    $notes = New-Object System.Collections.ArrayList
    if ($r.Reason -ne '') { [void]$notes.Add($r.Reason) }
    foreach ($w in $r.Warnings) { [void]$notes.Add($w) }
    $valid = $r.Valid
    if (-not $valid -and $ClassInfo.Suffix -eq '.tmp') { $valid = $true; $notes.Clear(); [void]$notes.Add('tmp_unparsable') }
    return [pscustomobject]@{ Valid = $valid; Notes = ($notes -join ',') }
}

function Get-MMOFileIntegrity {
    # Yol ile cagri: dosyayi tek kez okur. Kilit dosyalari okunmaz (icerik onemsiz). Okuma hatasi = Valid=false.
    param([string]$Path, $ClassInfo)
    if ($ClassInfo.Class -eq 'side-lock') { return [pscustomobject]@{ Valid = $true; Notes = 'lock_file' } }
    try { $bytes = Read-MMOFileBytes $Path } catch {
        $m = $_.Exception.Message
        if ($m -ne 'too_large') { $m = 'read_error' }
        return [pscustomobject]@{ Valid = $false; Notes = $m }
    }
    return (Get-MMOBytesIntegrity -Bytes $bytes -ClassInfo $ClassInfo)
}

function Get-MMOFileSnapshotInfo {
    # Dosyayi TEK KEZ okur; boyut, SHA-256, gecerlilik ve notlar o diziden. Bytes de doner (B3 yazimi ayni diziyi kullanir).
    param([string]$Path, $ClassInfo)
    $bytes = Read-MMOFileBytes $Path
    $integ = Get-MMOBytesIntegrity -Bytes $bytes -ClassInfo $ClassInfo
    return [pscustomobject]@{
        Bytes = $bytes; Size = [long]$bytes.Length; Sha256 = (Get-MMOBytesSha256 $bytes)
        Valid = [bool]$integ.Valid; Notes = [string]$integ.Notes
    }
}

# ---- agac -------------------------------------------------------------------------------------------------------------
function Get-MMOTreeFiles {
    # Kok altindaki tum dosyalar (goreli, '/' ayracli, ordinal sirali). Reparse point'i izlemez: rastlarsa throw.
    # Listeleme hatasi (izin vb.) = throw 'tree_unreadable' (sessizce atlanmaz).
    param([string]$Root, [switch]$AllowRootAncestorReparse)
    $rootFull = Assert-MMORootSafe -Root $Root -AllowRootAncestorReparse:$AllowRootAncestorReparse
    $skip = $rootFull.Length + 1
    $stack = New-Object System.Collections.Stack
    $stack.Push($rootFull)
    $out = New-Object System.Collections.ArrayList
    while ($stack.Count -gt 0) {
        $dir = [string]$stack.Pop()
        try { $items = @(Get-ChildItem -LiteralPath $dir -Force -ErrorAction Stop) } catch { throw 'tree_unreadable' }
        foreach ($item in $items) {
            if (Test-MMOReparse $item.FullName) { throw ('reparse_entry:' + $item.Name) }
            if ($item.PSIsContainer) { $stack.Push($item.FullName) }
            else { [void]$out.Add($item.FullName.Substring($skip).Replace('\', '/')) }
        }
    }
    $arr = [string[]]@($out.ToArray())
    [Array]::Sort($arr, [StringComparer]::Ordinal)
    return , $arr
}

# ---- manifest ---------------------------------------------------------------------------------------------------------
function Get-MMOMember {
    # Nesne/sozluk uyesi; yoksa $null (StrictMode'da bile hata vermez).
    param($Obj, [string]$Name)
    if ($null -eq $Obj) { return $null }
    if ($Obj -is [System.Collections.IDictionary]) { if ($Obj.Contains($Name)) { return $Obj[$Name] }; return $null }
    $p = $Obj.PSObject.Properties[$Name]
    if ($null -eq $p) { return $null }
    return $p.Value
}

function ConvertTo-MMOUtcString {
    # PowerShell 7 ConvertFrom-Json ISO tarihini DateTime yapabilir; kanonik dizeye geri cevirir. Digger tipler [string].
    param($Value)
    if ($null -eq $Value) { return '' }
    if ($Value -is [datetime]) { return $Value.ToUniversalTime().ToString("yyyy-MM-dd'T'HH:mm:ss'Z'", [Globalization.CultureInfo]::InvariantCulture) }
    return [string]$Value
}

function Test-MMOUtcStamp {
    param($Value)
    $s = ConvertTo-MMOUtcString $Value
    if ($s -cnotmatch '^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z\z') { return $false }
    $dt = [datetime]::MinValue
    return [datetime]::TryParseExact($s, "yyyy-MM-dd'T'HH:mm:ss'Z'", [Globalization.CultureInfo]::InvariantCulture, [Globalization.DateTimeStyles]::None, [ref]$dt)
}

function Test-MMOShutdownProof {
    # $null = gecerli (null kanit da gecerli), aksi halde hata kodu. Kalip: MMOShutdownResult.json (run_id, exit_code, time_utc).
    param($Proof)
    if ($null -eq $Proof) { return $null }
    if (-not (($Proof -is [pscustomobject]) -or ($Proof -is [System.Collections.IDictionary]))) { return 'bad_shutdown_proof' }
    $rid = Get-MMOMember $Proof 'run_id'
    if ($rid -isnot [string] -or $rid -cnotmatch '^[0-9TZ-]{1,40}\z') { return 'bad_shutdown_proof' }
    $ec = Get-MMOMember $Proof 'exit_code'
    if (-not (($ec -is [int]) -or ($ec -is [long])) -or $ec -lt 0 -or $ec -gt 255) { return 'bad_shutdown_proof' }
    if (-not (Test-MMOUtcStamp (Get-MMOMember $Proof 'time_utc'))) { return 'bad_shutdown_proof' }
    return $null
}

function Get-MMOEntriesDigest {
    # entries_digest = SHA-256( sirali  path NUL class NUL sha256 NUL size NUL valid NUL notes LF  satirlari ), ordinal sira, UTF-8.
    # O(n log n): [Array]::Sort ordinal. Girislerin alti alani da (path,class,sha256,size,valid,notes) bulunmak zorunda (yoksa throw).
    param($Entries)
    $list = @($Entries)
    $n = $list.Count
    $keys = New-Object 'string[]' $n
    $lines = New-Object 'string[]' $n
    $nul = [string][char]0
    $i = 0
    foreach ($e in $list) {
        $v = $e.valid
        if ($v -is [bool]) { if ($v) { $v = 'true' } else { $v = 'false' } } else { $v = [string]$v }
        $keys[$i] = [string]$e.path
        $lines[$i] = [string]$e.path + $nul + [string]$e.class + $nul + [string]$e.sha256 + $nul + [string]$e.size + $nul + $v + $nul + [string]$e.notes + "`n"
        $i++
    }
    [Array]::Sort($keys, $lines, [StringComparer]::Ordinal)
    $sb = New-Object System.Text.StringBuilder
    foreach ($l in $lines) { [void]$sb.Append($l) }
    $bytes = (New-Object System.Text.UTF8Encoding($false)).GetBytes($sb.ToString())
    return (Get-MMOBytesSha256 $bytes)
}

function Get-MMOHeaderDigest {
    # header_digest = SHA-256 ust duzey alanlar: format_version, tool, created_utc, source_saved_dir, mode, shutdown_proof.
    param($Manifest)
    $proof = Get-MMOMember $Manifest 'shutdown_proof'
    $proofText = 'null'
    if ($null -ne $proof) {
        $proofText = 'run_id=' + [string](Get-MMOMember $proof 'run_id') + ';exit_code=' + [string](Get-MMOMember $proof 'exit_code') + ';time_utc=' + (ConvertTo-MMOUtcString (Get-MMOMember $proof 'time_utc'))
    }
    $text = 'format_version=' + [string](Get-MMOMember $Manifest 'format_version') + "`n" +
        'tool=' + [string](Get-MMOMember $Manifest 'tool') + "`n" +
        'created_utc=' + (ConvertTo-MMOUtcString (Get-MMOMember $Manifest 'created_utc')) + "`n" +
        'source_saved_dir=' + [string](Get-MMOMember $Manifest 'source_saved_dir') + "`n" +
        'mode=' + [string](Get-MMOMember $Manifest 'mode') + "`n" +
        'shutdown_proof=' + $proofText + "`n"
    # B1 sertlestirme: istege bagli ust duzey alanlar yalniz VARSA ozete girer (eski manifestlerin ozeti degismez).
    foreach ($ok in 'proof_stale', 'ignored_count', 'ignored_summary', 'process_check') {
        $ov = Get-MMOMember $Manifest $ok
        if ($null -eq $ov) { continue }
        if ($ov -is [bool]) { if ($ov) { $ov = 'true' } else { $ov = 'false' } }
        $text += $ok + '=' + [string]$ov + "`n"
    }
    $cv = Get-MMOMember $Manifest 'collisions'
    if ($null -ne $cv -and @($cv).Count -gt 0) {
        $cl = [string[]]@(@($cv) | ForEach-Object { [string]$_ })
        [Array]::Sort($cl, [StringComparer]::Ordinal)
        $text += 'collisions=' + ($cl -join ';') + "`n"
    }
    return (Get-MMOBytesSha256 ((New-Object System.Text.UTF8Encoding($false)).GetBytes($text)))
}

function New-MMOManifest {
    # $FilesRoot altindaki (yalniz beyaz liste) dosyalardan manifest nesnesi uretir; beyaz liste disi dosya, cakisma
    # (Ali Baba / Ali_Baba), 8 MB ustu dosya = throw. Her dosya tek kez okunur.
    param(
        [string]$FilesRoot,
        [string]$SourceSavedDir,
        [ValidateSet('quiesced', 'live')][string]$Mode = 'quiesced',
        $ShutdownProof = $null,
        [string]$CreatedUtc = '',
        [string[]]$UnstablePaths = @(),
        # B1 sertlestirme (hepsi istege bagli; verilmezse manifest eski bicimde kalir):
        #  ExpectedEntries = {yol -> @{Size;Sha256;Valid;Notes}} YAZILAN bayttan turetilmis beklenen tablo; goruntudeki dosyadan uretilen giris
        #    farkliysa (ya da tabloda olmayan/eksik dosya) throw 'copy_drift:<yol>' (O1: ozet kopyadan sonra yeniden okunmus bayttan DEGIL yazilandan).
        #  SkippedEntries = {yol -> 'read_error'|'too_large'} icerigi alinamayan dosyalar: goruntude dosya YOK; size=0, sha256=sifir, valid=false.
        #  AllowCollisions: cakisan yollar hata degil; manifestte 'collisions' olarak bildirilir (geri yukleme B3'te reddeder).
        $ExpectedEntries = $null,
        $SkippedEntries = $null,
        $ProofStale = $null,
        [int]$IgnoredCount = -1,
        [string]$IgnoredSummary = '',
        [string]$ProcessCheck = '',
        [switch]$AllowCollisions,
        [switch]$AllowRootAncestorReparse
    )
    if ($CreatedUtc -eq '') { $CreatedUtc = (Get-Date).ToUniversalTime().ToString("yyyy-MM-dd'T'HH:mm:ss'Z'", [Globalization.CultureInfo]::InvariantCulture) }
    if (-not (Test-MMOUtcStamp $CreatedUtc)) { throw 'bad_created_utc' }
    if ($null -ne (Test-MMOShutdownProof $ShutdownProof)) { throw 'bad_shutdown_proof' }
    $rels = Get-MMOTreeFiles -Root $FilesRoot -AllowRootAncestorReparse:$AllowRootAncestorReparse
    foreach ($rel in $rels) {
        $c0 = Get-MMOSaveFileClass $rel
        if ($c0.Class -eq 'ignored') { throw ('not_whitelisted:' + $rel + ':' + $c0.Reason) }
    }
    # Icerigi alinamayan dosyalar (O5): goruntude dosya olmamali; yol sozdizimi/sinif gecerli olmali.
    $skipMap = [ordered]@{}
    if ($null -ne $SkippedEntries) {
        foreach ($sk in @($SkippedEntries.Keys)) {
            $why = [string]$SkippedEntries[$sk]
            if ($why -cne 'read_error' -and $why -cne 'too_large') { throw ('skipped_reason_invalid:' + $sk) }
            if ($null -ne (Get-MMORelPathProblem ([string]$sk))) { throw ('skipped_path_invalid:' + $sk) }
            if ((Get-MMOSaveFileClass ([string]$sk)).Class -eq 'ignored') { throw ('skipped_path_not_whitelisted:' + $sk) }
            $skipMap[[string]$sk] = $why
        }
    }
    $allRels = New-Object System.Collections.ArrayList
    foreach ($rel in $rels) { [void]$allRels.Add($rel) }
    foreach ($sk in $skipMap.Keys) {
        foreach ($rel in $rels) { if ($rel -ceq $sk) { throw ('skipped_path_present:' + $sk) } }
        [void]$allRels.Add($sk)
    }
    $coll = @(Find-MMOCollisions ([string[]]$allRels.ToArray()))
    $collStrings = @()
    if ($coll.Count -gt 0) {
        if (-not $AllowCollisions) { throw ('collision:' + (@($coll[0]) -join '|')) }
        $collStrings = [string[]]@($coll | ForEach-Object { (@($_) -join '|') })
        [Array]::Sort($collStrings, [StringComparer]::Ordinal)
    }
    # B1: canli yedekte kararsiz kalan dosyalar (UnstablePaths) valid=false notes='unstable' olarak isaretlenir (iceriginden turetilemez;
    # Test-MMOSnapshot bunu yalniz mode=live manifestte ve yalniz "gecersiz" yonunde kabul eder). Goruntude olmayan yol = throw.
    $unstableSet = New-Object 'System.Collections.Generic.HashSet[string]' ([StringComparer]::Ordinal)
    foreach ($u in @($UnstablePaths)) { [void]$unstableSet.Add([string]$u) }
    if ($unstableSet.Count -gt 0 -and $Mode -cne 'live') { throw 'unstable_requires_live' }
    $relSet = New-Object 'System.Collections.Generic.HashSet[string]' ([StringComparer]::Ordinal)
    foreach ($rel in $rels) { [void]$relSet.Add($rel) }
    foreach ($u in $unstableSet) { if (-not $relSet.Contains($u)) { throw ('unstable_path_unknown:' + $u) } }
    $entries = New-Object System.Collections.ArrayList
    foreach ($rel in $rels) {
        $c = Get-MMOSaveFileClass $rel
        $full = Resolve-MMOSafePath -Root $FilesRoot -RelPath $rel -MustExist -AllowRootAncestorReparse:$AllowRootAncestorReparse
        try { $info = Get-MMOFileSnapshotInfo -Path $full -ClassInfo $c } catch { throw ('file_unreadable:' + $rel + ':' + $_.Exception.Message) }
        $eValid = $info.Valid
        $eNotes = $info.Notes
        if ($unstableSet.Contains($rel)) { $eValid = $false; $eNotes = 'unstable' }
        if ($null -ne $ExpectedEntries) {
            # O1: beklenen = kopyalanirken YAZILAN bayttan; kopyadan sonra goruntude degisen/bozulan dosya manifeste "temiz" girmez.
            if (-not $ExpectedEntries.ContainsKey($rel)) { throw ('copy_drift:' + $rel) }
            $x = $ExpectedEntries[$rel]
            if ([long]$x.Size -ne [long]$info.Size -or [string]$x.Sha256 -cne [string]$info.Sha256 -or [bool]$x.Valid -ne [bool]$eValid -or [string]$x.Notes -cne [string]$eNotes) {
                throw ('copy_drift:' + $rel)
            }
        }
        [void]$entries.Add([ordered]@{
            path = $rel
            class = $c.Class
            size = $info.Size
            sha256 = $info.Sha256
            valid = $eValid
            notes = $eNotes
        })
    }
    if ($null -ne $ExpectedEntries) {
        foreach ($xk in @($ExpectedEntries.Keys)) { if (-not $relSet.Contains([string]$xk)) { throw ('copy_drift:' + [string]$xk) } }
    }
    foreach ($sk in $skipMap.Keys) {
        [void]$entries.Add([ordered]@{
            path = $sk
            class = (Get-MMOSaveFileClass $sk).Class
            size = 0L
            sha256 = $script:MMOSvZeroSha
            valid = $false
            notes = $skipMap[$sk]
        })
    }
    $arr = $entries.ToArray()
    $ekeys = New-Object 'string[]' $arr.Length
    for ($ei = 0; $ei -lt $arr.Length; $ei++) { $ekeys[$ei] = [string]$arr[$ei]['path'] }
    [Array]::Sort($ekeys, $arr, [StringComparer]::Ordinal)
    $m = [ordered]@{
        format_version = $script:MMOSvManifestFormat
        created_utc = $CreatedUtc
        tool = $script:MMOSvToolName
        source_saved_dir = $SourceSavedDir
        mode = $Mode
        shutdown_proof = $ShutdownProof
        entries = $arr
        entries_digest = (Get-MMOEntriesDigest $arr)
    }
    if ($null -ne $ProofStale) { $m['proof_stale'] = [bool]$ProofStale }
    if ($IgnoredCount -ge 0) { $m['ignored_count'] = $IgnoredCount; $m['ignored_summary'] = $IgnoredSummary }
    if ($ProcessCheck -ne '') { $m['process_check'] = $ProcessCheck }
    if ($collStrings.Count -gt 0) { $m['collisions'] = [string[]]$collStrings }
    $m['header_digest'] = (Get-MMOHeaderDigest $m)
    return $m
}

function Write-MMOManifest {
    # manifest.json'u SON adim olarak yazar: .tmp (CreateNew: var olana yazmaz) -> Move. Var olan manifest uzerine yazmaz.
    # Goruntu dizini var olmali, reparse olmamali (atalar dahil; bilincli istisna -AllowRootAncestorReparse). UTF-8 BOM'suz.
    param($Manifest, [string]$SnapshotDir, [switch]$AllowRootAncestorReparse)
    $dir = (ConvertTo-MMOFullPath $SnapshotDir).TrimEnd('\')
    $a = Get-MMOAttributes $dir
    if ($a.State -eq 'missing') { throw 'snapshot_missing' }
    if ($a.State -ne 'ok') { throw 'snapshot_inaccessible' }
    if (($a.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) { throw 'reparse_snapshot' }
    if (-not $AllowRootAncestorReparse) {
        $anc = Get-MMOReparseAncestor $dir
        if ($null -ne $anc) { throw ('reparse_snapshot_ancestor:' + $anc) }
    }
    $final = $dir + '\manifest.json'
    $tmp = $final + '.tmp'
    if (Test-MMOReparse $final) { throw 'manifest_exists' }
    if ([IO.File]::Exists($final)) { throw 'manifest_exists' }
    if (Test-MMOReparse $tmp) { throw 'manifest_tmp_exists' }
    $json = $Manifest | ConvertTo-Json -Depth 6
    $bytes = (New-Object System.Text.UTF8Encoding($false)).GetBytes($json)
    if ($bytes.Length -gt $script:MMOSvMaxManifestBytes) { throw 'manifest_too_large' }
    $fs = $null
    try { $fs = [IO.File]::Open($tmp, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write, [IO.FileShare]::None) } catch {
        if ([IO.File]::Exists($tmp)) { throw 'manifest_tmp_exists' }
        # .NET Framework 4.x: yolun bir atasi junction ise CreateNew "zaten var" diye basarisiz olur (bu makinede dogrulandi). Yalniz bilincli
        # -AllowRootAncestorReparse ile ve .tmp yokken, bos dosya kosuluyla OpenOrCreate'e dusulur (atomik degil; belgelenmis sinir).
        if (-not $AllowRootAncestorReparse) { throw 'manifest_tmp_unwritable' }
        try {
            $fs = [IO.File]::Open($tmp, [IO.FileMode]::OpenOrCreate, [IO.FileAccess]::Write, [IO.FileShare]::None)
            if ($fs.Length -ne 0) { $fs.Dispose(); throw 'manifest_tmp_exists' }
        } catch {
            if ($_.Exception.Message -eq 'manifest_tmp_exists') { throw 'manifest_tmp_exists' }
            throw 'manifest_tmp_unwritable'
        }
    }
    try {
        $fs.Write($bytes, 0, $bytes.Length)
        $fs.Flush($true)
    } finally { $fs.Dispose() }
    try { [IO.File]::Move($tmp, $final) } catch {
        try { [IO.File]::Delete($tmp) } catch { }
        throw 'manifest_move_failed'
    }
}

function Read-MMOManifest {
    # manifest.json'u ayristirir ve sozlesmeyi denetler (dosyalara BAKMAZ; bkz. Test-MMOSnapshot).
    # Donus: Ok, Incomplete (manifest yok = yarim goruntu), Errors[], Manifest.
    param([string]$SnapshotDir, [switch]$AllowRootAncestorReparse)
    $errs = New-Object System.Collections.ArrayList
    try { $dir = (ConvertTo-MMOFullPath $SnapshotDir).TrimEnd('\') } catch {
        return [pscustomobject]@{ Ok = $false; Incomplete = $false; Errors = @('snapshot_path_invalid'); Manifest = $null }
    }
    if (Test-MMOReparse $dir) {
        return [pscustomobject]@{ Ok = $false; Incomplete = $false; Errors = @('reparse_snapshot'); Manifest = $null }
    }
    if (-not $AllowRootAncestorReparse) {
        $anc = Get-MMOReparseAncestor $dir
        if ($null -ne $anc) {
            return [pscustomobject]@{ Ok = $false; Incomplete = $false; Errors = @('reparse_snapshot_ancestor'); Manifest = $null }
        }
    }
    $path = $dir + '\manifest.json'
    if (-not [IO.File]::Exists($path)) {
        return [pscustomobject]@{ Ok = $false; Incomplete = $true; Errors = @('manifest_missing'); Manifest = $null }
    }
    if (Test-MMOReparse $path) {
        return [pscustomobject]@{ Ok = $false; Incomplete = $false; Errors = @('manifest_reparse'); Manifest = $null }
    }
    try {
        $bytes = Read-MMOFileBytes -Path $path -MaxBytes $script:MMOSvMaxManifestBytes
    } catch {
        $code = 'manifest_unreadable'
        if ($_.Exception.Message -eq 'too_large') { $code = 'manifest_too_large' }
        return [pscustomobject]@{ Ok = $false; Incomplete = $false; Errors = @($code); Manifest = $null }
    }
    try {
        $r = ConvertFrom-MMOUtf8Bytes $bytes
        $m = $r.Text | ConvertFrom-Json -ErrorAction Stop
    } catch {
        return [pscustomobject]@{ Ok = $false; Incomplete = $false; Errors = @('manifest_unreadable'); Manifest = $null }
    }
    if ($null -eq $m -or $m -isnot [pscustomobject]) {
        return [pscustomobject]@{ Ok = $false; Incomplete = $false; Errors = @('manifest_not_object'); Manifest = $null }
    }
    $props = $m.PSObject.Properties
    foreach ($name in 'format_version', 'created_utc', 'tool', 'source_saved_dir', 'mode', 'entries', 'entries_digest', 'header_digest') {
        if ($null -eq $props[$name]) { [void]$errs.Add('missing_field:' + $name) }
    }
    if ($errs.Count -gt 0) {
        return [pscustomobject]@{ Ok = $false; Incomplete = $false; Errors = @($errs.ToArray()); Manifest = $m }
    }
    $fv = $m.format_version
    if (-not ((($fv -is [int]) -or ($fv -is [long])) -and $fv -eq $script:MMOSvManifestFormat)) { [void]$errs.Add('bad_format_version') }
    if ($m.mode -isnot [string] -or ($m.mode -cne 'quiesced' -and $m.mode -cne 'live')) { [void]$errs.Add('bad_mode') }
    if ($m.source_saved_dir -isnot [string] -or $m.source_saved_dir.Length -eq 0) { [void]$errs.Add('bad_source_saved_dir') }
    if ($m.tool -isnot [string] -or $m.tool.Length -eq 0) { [void]$errs.Add('bad_tool') }
    if (-not (Test-MMOUtcStamp $m.created_utc)) { [void]$errs.Add('bad_created_utc') }
    $proof = $props['shutdown_proof']
    if ($null -ne $proof -and $null -ne $proof.Value) {
        if ($null -ne (Test-MMOShutdownProof $proof.Value)) { [void]$errs.Add('bad_shutdown_proof') }
    }
    # Istege bagli B1 alanlari: varsa tipi dogru olmali (header_digest'e girerler)
    if ($null -ne $props['proof_stale'] -and $props['proof_stale'].Value -isnot [bool]) { [void]$errs.Add('bad_proof_stale') }
    if ($null -ne $props['ignored_count']) {
        $icv = $props['ignored_count'].Value
        if (-not ((($icv -is [int]) -or ($icv -is [long])) -and $icv -ge 0)) { [void]$errs.Add('bad_ignored_count') }
    }
    if ($null -ne $props['ignored_summary'] -and $props['ignored_summary'].Value -isnot [string]) { [void]$errs.Add('bad_ignored_summary') }
    if ($null -ne $props['process_check']) {
        $pcv = $props['process_check'].Value
        if ($pcv -isnot [string] -or ($pcv -cne 'cim' -and $pcv -cne 'injected' -and $pcv -cne 'skipped')) { [void]$errs.Add('bad_process_check') }
    }
    $declaredColl = New-Object 'System.Collections.Generic.HashSet[string]' ([StringComparer]::Ordinal)
    if ($null -ne $props['collisions']) {
        $cvv = $props['collisions'].Value
        if ($cvv -isnot [array] -or @($cvv).Count -eq 0) { [void]$errs.Add('bad_collisions') }
        else {
            foreach ($cs in $cvv) {
                if ($cs -isnot [string] -or $cs.IndexOf('|') -lt 0) { [void]$errs.Add('bad_collisions'); break }
                [void]$declaredColl.Add($cs)
            }
        }
    }
    if ($m.entries -isnot [array]) {
        [void]$errs.Add('entries_not_array')
        return [pscustomobject]@{ Ok = $false; Incomplete = $false; Errors = @($errs.ToArray()); Manifest = $m }
    }
    $seen = New-Object 'System.Collections.Generic.HashSet[string]' ([StringComparer]::OrdinalIgnoreCase)
    $allPaths = New-Object System.Collections.ArrayList
    $i = 0
    foreach ($e in $m.entries) {
        $i++
        $tag = 'entry' + $i
        if ($e -isnot [pscustomobject]) { [void]$errs.Add($tag + ':not_object'); continue }
        $ep = $e.PSObject.Properties
        $complete = $true
        foreach ($name in 'path', 'class', 'size', 'sha256', 'valid', 'notes') {
            if ($null -eq $ep[$name]) { [void]$errs.Add($tag + ':missing_field:' + $name); $complete = $false }
        }
        if (-not $complete) { continue }
        if ($e.path -isnot [string]) { [void]$errs.Add($tag + ':bad_path'); continue }
        $problem = Get-MMORelPathProblem $e.path
        if ($null -ne $problem) { [void]$errs.Add($tag + ':unsafe_path:' + $problem); continue }
        [void]$allPaths.Add($e.path)
        if (-not $seen.Add($e.path)) { [void]$errs.Add($tag + ':duplicate_path') }
        $c = Get-MMOSaveFileClass $e.path
        if ($c.Class -eq 'ignored') { [void]$errs.Add($tag + ':not_whitelisted:' + $c.Reason) }
        elseif ($e.class -isnot [string] -or $e.class -cne $c.Class) { [void]$errs.Add($tag + ':class_mismatch') }
        if ($e.sha256 -isnot [string] -or $e.sha256 -cnotmatch '^[0-9a-f]{64}\z') { [void]$errs.Add($tag + ':bad_sha256') }
        if (-not ((($e.size -is [int]) -or ($e.size -is [long])) -and $e.size -ge 0 -and $e.size -le $script:MMOSvMaxFileBytes)) { [void]$errs.Add($tag + ':bad_size') }
        if ($e.valid -isnot [bool]) { [void]$errs.Add($tag + ':bad_valid') }
        if ($e.notes -isnot [string]) { [void]$errs.Add($tag + ':bad_notes') }
        elseif ($e.notes -ceq 'read_error' -or $e.notes -ceq 'too_large') {
            # icerigi alinamayan giris: "icerik yok" bicimi (goruntude dosya yok); aksi halde bu notlar baska girislerde kullanilamaz
            if ($e.valid -isnot [bool] -or $e.valid -ne $false -or $e.sha256 -cne $script:MMOSvZeroSha -or $e.size -ne 0) { [void]$errs.Add($tag + ':bad_skipped_entry') }
        }
    }
    # Cakisma: hesaplanan gruplar manifestteki 'collisions' bildirimiyle BIREBIR ayniysa kabul (uyari; geri yukleme B3'te reddeder), aksi hata
    $coll = @(Find-MMOCollisions ([string[]]$allPaths.ToArray()))
    $gotColl = New-Object 'System.Collections.Generic.HashSet[string]' ([StringComparer]::Ordinal)
    foreach ($cg in $coll) { [void]$gotColl.Add((@($cg) -join '|')) }
    $collOk = ($gotColl.Count -eq $declaredColl.Count)
    if ($collOk) { foreach ($gc in $gotColl) { if (-not $declaredColl.Contains($gc)) { $collOk = $false } } }
    if (-not $collOk) {
        if ($coll.Count -gt 0) { [void]$errs.Add('collision:' + (@($coll[0]) -join '|')) }
        else { [void]$errs.Add('collision_declared_but_absent') }
    }
    if ($errs.Count -eq 0) {
        $digest = Get-MMOEntriesDigest $m.entries
        if ($m.entries_digest -isnot [string] -or $m.entries_digest -cne $digest) { [void]$errs.Add('digest_mismatch') }
        $hd = Get-MMOHeaderDigest $m
        if ($m.header_digest -isnot [string] -or $m.header_digest -cne $hd) { [void]$errs.Add('header_digest_mismatch') }
    }
    return [pscustomobject]@{ Ok = ($errs.Count -eq 0); Incomplete = $false; Errors = @($errs.ToArray()); Manifest = $m }
}

function Test-MMOSnapshot {
    # Goruntu dizinini (manifest.json + files\) salt okuma ile dogrular: boyut, SHA-256, valid/notes yeniden hesabi, fazladan dosya, reparse.
    # Her dosya TEK KEZ okunur. Donus: Ok, Incomplete, Errors[], Extra[], InvalidEntries[] (valid=false olanlar; hata sayilmaz),
    # Mode ('quiesced'|'live'|''), HasInvalid, HasUnstable (kararsiz girisler; yalniz mode=live'da kabul), ProofStale, Collisions[] (bildirilmis cakismalar).
    param([string]$SnapshotDir, [switch]$AllowRootAncestorReparse)
    $errs = New-Object System.Collections.ArrayList
    $extra = New-Object System.Collections.ArrayList
    $invalid = New-Object System.Collections.ArrayList
    $r = Read-MMOManifest -SnapshotDir $SnapshotDir -AllowRootAncestorReparse:$AllowRootAncestorReparse
    $snapMode = ''
    $snapStale = $false
    $snapColl = @()
    if ($null -ne $r.Manifest) {
        $mv = Get-MMOMember $r.Manifest 'mode'
        if ($mv -is [string]) { $snapMode = $mv }
        if ((Get-MMOMember $r.Manifest 'proof_stale') -eq $true) { $snapStale = $true }
        $cvx = Get-MMOMember $r.Manifest 'collisions'
        if ($null -ne $cvx) { $snapColl = [string[]]@(@($cvx) | ForEach-Object { [string]$_ }) }
    }
    if (-not $r.Ok) {
        return [pscustomobject]@{ Ok = $false; Incomplete = $r.Incomplete; Errors = $r.Errors; Extra = @(); InvalidEntries = @()
            Mode = $snapMode; HasInvalid = $false; HasUnstable = $false; ProofStale = $snapStale; Collisions = $snapColl }
    }
    $filesRoot = (ConvertTo-MMOFullPath $SnapshotDir).TrimEnd('\') + '\files'
    if (-not [IO.Directory]::Exists($filesRoot)) {
        [void]$errs.Add('files_missing')
        return [pscustomobject]@{ Ok = $false; Incomplete = $false; Errors = @($errs.ToArray()); Extra = @(); InvalidEntries = @()
            Mode = $snapMode; HasInvalid = $false; HasUnstable = $false; ProofStale = $snapStale; Collisions = $snapColl }
    }
    $declared = New-Object 'System.Collections.Generic.HashSet[string]' ([StringComparer]::OrdinalIgnoreCase)
    $hasUnstable = $false
    foreach ($e in $r.Manifest.entries) {
        [void]$declared.Add($e.path)
        $isInvalid = ($e.valid -eq $false)
        if (($e.notes -ceq 'read_error' -or $e.notes -ceq 'too_large') -and $isInvalid) {
            # Icerigi alinamayan giris (O5): goruntude dosya OLMAMALI (varsa manifest yalan soyluyor); hash/boyut karsilastirilmaz, her zaman gecersiz.
            $skFull = $filesRoot + '\' + ([string]$e.path).Replace('/', '\')
            if ((Get-MMOAttributes $skFull).State -ne 'missing') { [void]$errs.Add('skipped_entry_has_file:' + $e.path) }
            [void]$invalid.Add($e.path)
            continue
        }
        try { $full = Resolve-MMOSafePath -Root $filesRoot -RelPath $e.path -MustExist -AllowRootAncestorReparse:$AllowRootAncestorReparse } catch {
            [void]$errs.Add('entry_unusable:' + $e.path + ':' + $_.Exception.Message)
            if ($isInvalid) { [void]$invalid.Add($e.path) }
            continue
        }
        try { $bytes = Read-MMOFileBytes $full } catch {
            [void]$errs.Add('entry_unusable:' + $e.path + ':' + $_.Exception.Message)
            if ($isInvalid) { [void]$invalid.Add($e.path) }
            continue
        }
        if ([long]$bytes.Length -ne [long]$e.size) {
            [void]$errs.Add('size_mismatch:' + $e.path)
            if ($isInvalid) { [void]$invalid.Add($e.path) }
            continue
        }
        $sha = Get-MMOBytesSha256 $bytes
        if ($sha -cne $e.sha256) {
            [void]$errs.Add('hash_mismatch:' + $e.path)
            if ($isInvalid) { [void]$invalid.Add($e.path) }
            continue
        }
        if (($e.notes -ceq 'unstable') -and ($e.valid -eq $false)) {
            # B1 canli yedek: kararsizlik icerikten yeniden hesaplanamaz; yalniz "gecersiz" yonunde (valid=true + unstable = uyusmazlik) ve
            # yalniz mode=live manifestte kabul edilir (O6): quiesced bir goruntude kararsiz not = sahte/kurcalanmis manifest.
            [void]$invalid.Add($e.path)
            if ($snapMode -ceq 'live') { $hasUnstable = $true } else { [void]$errs.Add('unstable_in_quiesced:' + $e.path) }
            continue
        }
        $integ = Get-MMOBytesIntegrity -Bytes $bytes -ClassInfo (Get-MMOSaveFileClass $e.path)
        if ([bool]$integ.Valid -ne [bool]$e.valid) { [void]$errs.Add('valid_mismatch:' + $e.path) }
        elseif ([string]$integ.Notes -cne [string]$e.notes) { [void]$errs.Add('notes_mismatch:' + $e.path) }
        if (-not $integ.Valid) { [void]$invalid.Add($e.path) }
    }
    try { $actual = Get-MMOTreeFiles -Root $filesRoot -AllowRootAncestorReparse:$AllowRootAncestorReparse } catch {
        [void]$errs.Add('tree_unusable:' + $_.Exception.Message)
        $actual = @()
    }
    foreach ($rel in $actual) {
        if (-not $declared.Contains($rel)) { [void]$extra.Add($rel); [void]$errs.Add('extra_file:' + $rel) }
    }
    return [pscustomobject]@{
        Ok = ($errs.Count -eq 0); Incomplete = $false
        Errors = @($errs.ToArray()); Extra = @($extra.ToArray()); InvalidEntries = @($invalid.ToArray())
        Mode = $snapMode; HasInvalid = ($invalid.Count -gt 0); HasUnstable = $hasUnstable; ProofStale = $snapStale; Collisions = $snapColl
    }
}

# ---- B1: yedek alma yardimcilari (BackupSaves.ps1) ----------------------------------------------------------------------
function Get-MMOBlockingProcesses {
    # Yedegi engelleyen UE/derleme surecleri (kalip: build.ps1 Assert-NoUnrealRunning). Girdi: Name/ProcessId/CommandLine uyeli nesneler
    # (Win32_Process ya da test saglayicisi). dotnet.exe: komut satiri bos (okunamadi) ya da UBT/UAT/UE ise engeller.
    param($Processes)
    $out = New-Object System.Collections.ArrayList
    foreach ($p in @($Processes)) {
        if ($null -eq $p) { continue }
        $name = [string](Get-MMOMember $p 'Name')
        $cmd = [string](Get-MMOMember $p 'CommandLine')
        $hit = $false
        if ($name -match '^(UnrealEditor.*|LiveCoding.*|UnrealBuildTool.*|AutomationTool.*|MSBuild|UAT|MyProject(?:Server|Client)?(?:-.*)?)\.exe$') { $hit = $true }
        elseif ($name -eq 'dotnet.exe') {
            if ([string]::IsNullOrWhiteSpace($cmd) -or $cmd -match 'UnrealBuildTool|AutomationTool|RunUAT|Epic Games[\\/]UE_') { $hit = $true }
        }
        if ($hit) { [void]$out.Add([pscustomobject]@{ Name = $name; ProcessId = (Get-MMOMember $p 'ProcessId') }) }
    }
    return $out.ToArray()
}

function Read-MMOShutdownResult {
    # Saved\Logs\MMOShutdownResult.json (yalniz OKUNUR): run_id ^[0-9TZ-]{1,40}$, exit_code tam sayi 0..255, time_utc (UE ISO8601, kesirli saniye
    # olabilir; kanonik saniyeye indirilir). Donus: Ok, Reason ('' | shutdown_result_missing | shutdown_result_invalid | shutdown_not_clean), Proof.
    # Ok yalniz exit_code=0 iken $true. Dosya tek basina "sunucu simdi kapali" kaniti DEGILDIR (surec denetimi ayri kapi).
    param([string]$SavedDir)
    try { $saved = (ConvertTo-MMOFullPath $SavedDir).TrimEnd('\') } catch { return [pscustomobject]@{ Ok = $false; Reason = 'shutdown_result_invalid'; Proof = $null } }
    $logs = $saved + '\Logs'
    $file = $logs + '\MMOShutdownResult.json'
    $la = Get-MMOAttributes $logs
    if ($la.State -eq 'missing') { return [pscustomobject]@{ Ok = $false; Reason = 'shutdown_result_missing'; Proof = $null } }
    if ($la.State -ne 'ok' -or (($la.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0)) { return [pscustomobject]@{ Ok = $false; Reason = 'shutdown_result_invalid'; Proof = $null } }
    $fa = Get-MMOAttributes $file
    if ($fa.State -eq 'missing') { return [pscustomobject]@{ Ok = $false; Reason = 'shutdown_result_missing'; Proof = $null } }
    if ($fa.State -ne 'ok' -or (($fa.Attributes -band ([IO.FileAttributes]::ReparsePoint -bor [IO.FileAttributes]::Directory)) -ne 0)) { return [pscustomobject]@{ Ok = $false; Reason = 'shutdown_result_invalid'; Proof = $null } }
    try { $bytes = Read-MMOFileBytes -Path $file -MaxBytes 65536 } catch { return [pscustomobject]@{ Ok = $false; Reason = 'shutdown_result_invalid'; Proof = $null } }
    try {
        $r = ConvertFrom-MMOUtf8Bytes $bytes
        $o = $r.Text | ConvertFrom-Json -ErrorAction Stop
    } catch { return [pscustomobject]@{ Ok = $false; Reason = 'shutdown_result_invalid'; Proof = $null } }
    if ($null -eq $o -or $o -isnot [pscustomobject]) { return [pscustomobject]@{ Ok = $false; Reason = 'shutdown_result_invalid'; Proof = $null } }
    $rid = Get-MMOMember $o 'run_id'
    if ($rid -isnot [string] -or $rid -cnotmatch '^[0-9TZ-]{1,40}\z') { return [pscustomobject]@{ Ok = $false; Reason = 'shutdown_result_invalid'; Proof = $null } }
    $ec = Get-MMOMember $o 'exit_code'
    if (-not (($ec -is [int]) -or ($ec -is [long])) -or $ec -lt 0 -or $ec -gt 255) { return [pscustomobject]@{ Ok = $false; Reason = 'shutdown_result_invalid'; Proof = $null } }
    $tv = Get-MMOMember $o 'time_utc'
    if ($null -eq $tv -or (($tv -isnot [string]) -and ($tv -isnot [datetime]))) { return [pscustomobject]@{ Ok = $false; Reason = 'shutdown_result_invalid'; Proof = $null } }
    $ts = ConvertTo-MMOUtcString $tv
    $ts = [regex]::Replace($ts, '^([0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2})\.[0-9]{1,7}Z\z', '$1Z')
    if (-not (Test-MMOUtcStamp $ts)) { return [pscustomobject]@{ Ok = $false; Reason = 'shutdown_result_invalid'; Proof = $null } }
    $proof = [ordered]@{ run_id = $rid; exit_code = [int]$ec; time_utc = $ts }
    if ([int]$ec -ne 0) { return [pscustomobject]@{ Ok = $false; Reason = 'shutdown_not_clean'; Proof = $proof } }
    return [pscustomobject]@{ Ok = $true; Reason = ''; Proof = $proof }
}
