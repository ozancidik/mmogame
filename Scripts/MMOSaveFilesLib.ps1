# Kayit dosyasi yedekleme/dogrulama kutuphanesi (docs/DESIGN_BACKUP.md, dilim B0). Dot-source edilir:
#   . (Join-Path $PSScriptRoot 'MMOSaveFilesLib.ps1')
# Saf mantik + dosya sistemi yardimcilari; oyuna/Saved'e baglanti YOK, kendiliginden hicbir sey yazmaz
# (yalniz Write-MMOManifest verilen goruntu dizinine manifest.json yazar). Windows PowerShell 5.1 ve PowerShell 7 uyumlu,
# dosya yalniz ASCII (5.1 BOM'suz UTF-8'i ANSI okur). Dosya ICERIKLERI asla yeniden yazilmaz: bayt okuma + hash.
#
# Dosya sinifi (Get-MMOSaveFileClass):
#   character = Saved/SaveGames/MMOSave_<ad>.sav
#   side      = basarim/gunluk/posta/irk .json (+ .tmp, posta icin .bak)
#   side-lock = .bad / .tmp.bad (karantina; irk .bad = secim kilidi, korunur)
#   ignored   = beyaz liste disi (neden: Reason)
# Butunluk (Get-MMOFileIntegrity): .sav = boyut + GVAS basligi; .json/.bak = kati UTF-8 + JSON nesnesi + tam sayi "version";
# .tmp = ayni denetim ama bozuksa valid=true notes=tmp_unparsable (yarim yazim beklenir); kilit dosyalari valid=true notes=lock_file.
# NOT: .sav icin yalniz baslik denetlenir; oyun anlamli yukleme dogrulamasi YOK (DESIGN_BACKUP R6).

$script:MMOSvMinSavBytes = 64          # [PLACEHOLDER] DESIGN_BACKUP 4.3
$script:MMOSvMaxNameLen = 32           # MMONameRules::SlotKey MaxLen
$script:MMOSvMaxRelPath = 200
$script:MMOSvManifestFormat = 1
$script:MMOSvToolName = 'MMOSaveFilesLib/1'

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
        if ($base.TrimEnd(' ') -match '^(CON|PRN|AUX|NUL|COM[0-9]|LPT[0-9])$') { return 'device_name' }
    }
    return $null
}

function Test-MMOSafeRelPath {
    param([AllowNull()][AllowEmptyString()][string]$RelPath)
    return ($null -eq (Get-MMORelPathProblem $RelPath))
}

function Test-MMOPathUnder {
    # $Path kanonik olarak $Root'un ALTINDA mi (ayrac sinirli; 'C:\ab' , 'C:\a' altinda degil). -AllowEqual: esitlik de kabul.
    param([string]$Path, [string]$Root, [switch]$AllowEqual)
    try {
        $p = [IO.Path]::GetFullPath($Path).TrimEnd('\')
        $r = [IO.Path]::GetFullPath($Root).TrimEnd('\')
    } catch { return $false }
    if ($p.Equals($r, [StringComparison]::OrdinalIgnoreCase)) { return [bool]$AllowEqual }
    return $p.StartsWith($r + '\', [StringComparison]::OrdinalIgnoreCase)
}

function Get-MMOAttributes {
    param([string]$Path)
    try { return [IO.File]::GetAttributes($Path) } catch { return $null }
}

function Test-MMOReparse {
    # Junction/sembolik bag/baska reparse point mi. Var olmayan yol = $false (varlik ayrica denetlenir).
    param([string]$Path)
    $a = Get-MMOAttributes $Path
    if ($null -eq $a) { return $false }
    return (($a -band [IO.FileAttributes]::ReparsePoint) -ne 0)
}

function Resolve-MMOSafePath {
    # Kok altinda guvenli tam yol dondurur ya da throw eder (mesaj on eki = neden kodu).
    param([string]$Root, [string]$RelPath, [switch]$MustExist)
    $problem = Get-MMORelPathProblem $RelPath
    if ($null -ne $problem) { throw ('unsafe_relpath:' + $problem) }
    $rootFull = [IO.Path]::GetFullPath($Root).TrimEnd('\')
    if ($null -eq (Get-MMOAttributes $rootFull)) { throw 'root_missing' }
    if (Test-MMOReparse $rootFull) { throw 'reparse_root' }
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
    # Posta/irk dosya anahtari: zaten kanonik (ASCII kucuk harf, '_' '-').
    param([AllowNull()][string]$Key)
    if ($null -eq $Key) { return $false }
    return ($Key -cmatch '^[a-z0-9_-]{1,32}\z')
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
        $nameMode = 'key'
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
    } else {
        if (-not (Test-MMOFileKey $name)) { return (New-MMOClassResult 'ignored' $kind '' '' 'name_rule') }
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
    # farkli yol (MMOSave_ALI.sav + MMOSave_ali.sav) ya da beyaz liste disinda yalniz harf farkli yollar.
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

function Get-MMOFileSha256 {
    param([string]$Path)
    $fs = [IO.File]::Open($Path, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::ReadWrite)
    try {
        $sha = [Security.Cryptography.SHA256]::Create()
        try { $hash = $sha.ComputeHash($fs) } finally { $sha.Dispose() }
    } finally { $fs.Dispose() }
    return (ConvertTo-MMOHex $hash)
}

function ConvertTo-MMOHex {
    param([byte[]]$Bytes)
    $sb = New-Object System.Text.StringBuilder
    foreach ($b in $Bytes) { [void]$sb.Append($b.ToString('x2')) }
    return $sb.ToString()
}

function Test-MMOSavFile {
    # .sav: boyut >= MinBytes ve ilk 4 bayt 'GVAS'. Oyun anlamli dogrulama DEGIL.
    param([string]$Path, [int]$MinBytes = $script:MMOSvMinSavBytes)
    $fs = [IO.File]::Open($Path, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::ReadWrite)
    try {
        $len = $fs.Length
        if ($len -lt $MinBytes) { return [pscustomobject]@{ Valid = $false; Reason = 'too_small'; Warnings = @() } }
        $buf = New-Object byte[] 4
        $n = $fs.Read($buf, 0, 4)
        if ($n -ne 4 -or $buf[0] -ne 0x47 -or $buf[1] -ne 0x56 -or $buf[2] -ne 0x41 -or $buf[3] -ne 0x53) {
            return [pscustomobject]@{ Valid = $false; Reason = 'bad_magic'; Warnings = @() }
        }
    } finally { $fs.Dispose() }
    return [pscustomobject]@{ Valid = $true; Reason = ''; Warnings = @() }
}

function Read-MMOUtf8Text {
    # Kati UTF-8 (gecersiz bayt = throw 'bad_utf8'). UTF-8 BOM'u soyar ve HasBom ile bildirir.
    param([string]$Path)
    $bytes = [IO.File]::ReadAllBytes($Path)
    $offset = 0
    $bom = $false
    if ($bytes.Length -ge 3 -and $bytes[0] -eq 0xEF -and $bytes[1] -eq 0xBB -and $bytes[2] -eq 0xBF) { $offset = 3; $bom = $true }
    $enc = New-Object System.Text.UTF8Encoding($false, $true)
    try { $text = $enc.GetString($bytes, $offset, $bytes.Length - $offset) } catch { throw 'bad_utf8' }
    return [pscustomobject]@{ Text = $text; HasBom = $bom }
}

function Test-MMOJsonFile {
    # JSON nesnesi + tam sayi "version". UTF-8 BOM = gecerli ama uyari.
    param([string]$Path)
    $warn = New-Object System.Collections.ArrayList
    try { $r = Read-MMOUtf8Text $Path } catch {
        $m = $_.Exception.Message
        if ($m -ne 'bad_utf8') { $m = 'read_error' }
        return [pscustomobject]@{ Valid = $false; Reason = $m; Warnings = @() }
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
    return [pscustomobject]@{ Valid = $true; Reason = ''; Warnings = @($warn.ToArray()) }
}

function Get-MMOFileIntegrity {
    # Sinif bilgisine gore butunluk karari. Donus: Valid (bool), Notes (string).
    param([string]$Path, $ClassInfo)
    if ($ClassInfo.Class -eq 'character') {
        $r = Test-MMOSavFile -Path $Path
        return [pscustomobject]@{ Valid = $r.Valid; Notes = $r.Reason }
    }
    if ($ClassInfo.Class -eq 'side-lock') {
        return [pscustomobject]@{ Valid = $true; Notes = 'lock_file' }
    }
    $r = Test-MMOJsonFile -Path $Path
    $notes = New-Object System.Collections.ArrayList
    if ($r.Reason -ne '') { [void]$notes.Add($r.Reason) }
    foreach ($w in $r.Warnings) { [void]$notes.Add($w) }
    $valid = $r.Valid
    if (-not $valid -and $ClassInfo.Suffix -eq '.tmp') { $valid = $true; $notes.Clear(); [void]$notes.Add('tmp_unparsable') }
    return [pscustomobject]@{ Valid = $valid; Notes = ($notes -join ',') }
}

function Get-MMOTreeFiles {
    # Kok altindaki tum dosyalar (goreli, '/' ayracli, ordinal sirali). Reparse point'i izlemez: rastlarsa throw.
    param([string]$Root)
    $rootFull = [IO.Path]::GetFullPath($Root).TrimEnd('\')
    if (Test-MMOReparse $rootFull) { throw 'reparse_root' }
    $skip = $rootFull.Length + 1
    $stack = New-Object System.Collections.Stack
    $stack.Push($rootFull)
    $out = New-Object System.Collections.ArrayList
    while ($stack.Count -gt 0) {
        $dir = [string]$stack.Pop()
        foreach ($item in (Get-ChildItem -LiteralPath $dir -Force)) {
            if (Test-MMOReparse $item.FullName) { throw ('reparse_entry:' + $item.Name) }
            if ($item.PSIsContainer) { $stack.Push($item.FullName) }
            else { [void]$out.Add($item.FullName.Substring($skip).Replace('\', '/')) }
        }
    }
    $arr = [string[]]@($out.ToArray())
    [Array]::Sort($arr, [StringComparer]::Ordinal)
    return , $arr
}

function Get-MMOEntriesDigest {
    # entries_digest = SHA-256( sirali  path NUL sha256 NUL size LF  satirlari ), ordinal sira, UTF-8.
    param($Entries)
    $sorted = New-Object System.Collections.ArrayList
    foreach ($e in @($Entries)) {
        $idx = 0
        while ($idx -lt $sorted.Count -and [string]::CompareOrdinal([string]$sorted[$idx].path, [string]$e.path) -le 0) { $idx++ }
        $sorted.Insert($idx, $e)
    }
    $sb = New-Object System.Text.StringBuilder
    foreach ($e in $sorted) {
        [void]$sb.Append([string]$e.path).Append([char]0).Append([string]$e.sha256).Append([char]0).Append([string]$e.size).Append("`n")
    }
    $bytes = (New-Object System.Text.UTF8Encoding($false)).GetBytes($sb.ToString())
    $sha = [Security.Cryptography.SHA256]::Create()
    try { return (ConvertTo-MMOHex ($sha.ComputeHash($bytes))) } finally { $sha.Dispose() }
}

function New-MMOManifest {
    # $FilesRoot altindaki (yalniz beyaz liste) dosyalardan manifest nesnesi uretir; beyaz liste disi dosya = throw.
    param(
        [string]$FilesRoot,
        [string]$SourceSavedDir,
        [ValidateSet('quiesced', 'live')][string]$Mode = 'quiesced',
        $ShutdownProof = $null,
        [string]$CreatedUtc = ''
    )
    if ($CreatedUtc -eq '') { $CreatedUtc = (Get-Date).ToUniversalTime().ToString("yyyy-MM-dd'T'HH:mm:ss'Z'", [Globalization.CultureInfo]::InvariantCulture) }
    $rels = Get-MMOTreeFiles -Root $FilesRoot
    $entries = New-Object System.Collections.ArrayList
    foreach ($rel in $rels) {
        $c = Get-MMOSaveFileClass $rel
        if ($c.Class -eq 'ignored') { throw ('not_whitelisted:' + $rel + ':' + $c.Reason) }
        $full = Resolve-MMOSafePath -Root $FilesRoot -RelPath $rel -MustExist
        $integrity = Get-MMOFileIntegrity -Path $full -ClassInfo $c
        [void]$entries.Add([ordered]@{
            path = $rel
            class = $c.Class
            size = [long](New-Object IO.FileInfo $full).Length
            sha256 = (Get-MMOFileSha256 $full)
            valid = [bool]$integrity.Valid
            notes = [string]$integrity.Notes
        })
    }
    $arr = $entries.ToArray()
    return [ordered]@{
        format_version = $script:MMOSvManifestFormat
        created_utc = $CreatedUtc
        tool = $script:MMOSvToolName
        source_saved_dir = $SourceSavedDir
        mode = $Mode
        shutdown_proof = $ShutdownProof
        entries = $arr
        entries_digest = (Get-MMOEntriesDigest $arr)
    }
}

function Write-MMOManifest {
    # manifest.json'u SON adim olarak yazar: .tmp -> Move. Var olan manifest uzerine yazmaz. UTF-8 BOM'suz.
    param($Manifest, [string]$SnapshotDir)
    $final = Join-Path $SnapshotDir 'manifest.json'
    $tmp = $final + '.tmp'
    if ([IO.File]::Exists($final)) { throw 'manifest_exists' }
    $json = $Manifest | ConvertTo-Json -Depth 6
    [IO.File]::WriteAllBytes($tmp, (New-Object System.Text.UTF8Encoding($false)).GetBytes($json))
    [IO.File]::Move($tmp, $final)
}

function Read-MMOManifest {
    # manifest.json'u ayristirir ve sozlesmeyi denetler (dosyalara BAKMAZ; bkz. Test-MMOSnapshot).
    # Donus: Ok, Incomplete (manifest yok = yarim goruntu), Errors[], Manifest.
    param([string]$SnapshotDir)
    $errs = New-Object System.Collections.ArrayList
    $path = Join-Path $SnapshotDir 'manifest.json'
    if (-not [IO.File]::Exists($path)) {
        return [pscustomobject]@{ Ok = $false; Incomplete = $true; Errors = @('manifest_missing'); Manifest = $null }
    }
    if (Test-MMOReparse $path) {
        return [pscustomobject]@{ Ok = $false; Incomplete = $false; Errors = @('manifest_reparse'); Manifest = $null }
    }
    try {
        $r = Read-MMOUtf8Text $path
        $m = $r.Text | ConvertFrom-Json -ErrorAction Stop
    } catch {
        return [pscustomobject]@{ Ok = $false; Incomplete = $false; Errors = @('manifest_unreadable'); Manifest = $null }
    }
    if ($null -eq $m -or $m -isnot [pscustomobject]) {
        return [pscustomobject]@{ Ok = $false; Incomplete = $false; Errors = @('manifest_not_object'); Manifest = $null }
    }
    $props = $m.PSObject.Properties
    foreach ($name in 'format_version', 'created_utc', 'source_saved_dir', 'mode', 'entries', 'entries_digest') {
        if ($null -eq $props[$name]) { [void]$errs.Add('missing_field:' + $name) }
    }
    if ($errs.Count -gt 0) {
        return [pscustomobject]@{ Ok = $false; Incomplete = $false; Errors = @($errs.ToArray()); Manifest = $m }
    }
    $fv = $m.format_version
    if (-not ((($fv -is [int]) -or ($fv -is [long])) -and $fv -eq $script:MMOSvManifestFormat)) { [void]$errs.Add('bad_format_version') }
    if ($m.mode -isnot [string] -or ($m.mode -cne 'quiesced' -and $m.mode -cne 'live')) { [void]$errs.Add('bad_mode') }
    if ($m.source_saved_dir -isnot [string] -or $m.source_saved_dir.Length -eq 0) { [void]$errs.Add('bad_source_saved_dir') }
    $proof = $props['shutdown_proof']
    if ($null -ne $proof -and $null -ne $proof.Value) {
        $pv = $proof.Value
        $rid = $null
        if ($pv -is [pscustomobject] -and $null -ne $pv.PSObject.Properties['run_id']) { $rid = $pv.run_id }
        if ($rid -isnot [string] -or $rid -cnotmatch '^[0-9TZ-]{1,40}\z') { [void]$errs.Add('bad_shutdown_proof') }
    }
    if ($m.entries -isnot [array]) {
        [void]$errs.Add('entries_not_array')
        return [pscustomobject]@{ Ok = $false; Incomplete = $false; Errors = @($errs.ToArray()); Manifest = $m }
    }
    $seen = New-Object 'System.Collections.Generic.HashSet[string]' ([StringComparer]::OrdinalIgnoreCase)
    $i = 0
    foreach ($e in $m.entries) {
        $i++
        $tag = 'entry' + $i
        if ($e -isnot [pscustomobject]) { [void]$errs.Add($tag + ':not_object'); continue }
        $ep = $e.PSObject.Properties
        $complete = $true
        foreach ($name in 'path', 'class', 'size', 'sha256', 'valid') {
            if ($null -eq $ep[$name]) { [void]$errs.Add($tag + ':missing_field:' + $name); $complete = $false }
        }
        if (-not $complete) { continue }
        if ($e.path -isnot [string]) { [void]$errs.Add($tag + ':bad_path'); continue }
        $problem = Get-MMORelPathProblem $e.path
        if ($null -ne $problem) { [void]$errs.Add($tag + ':unsafe_path:' + $problem); continue }
        if (-not $seen.Add($e.path)) { [void]$errs.Add($tag + ':duplicate_path') }
        $c = Get-MMOSaveFileClass $e.path
        if ($c.Class -eq 'ignored') { [void]$errs.Add($tag + ':not_whitelisted:' + $c.Reason) }
        elseif ($e.class -isnot [string] -or $e.class -cne $c.Class) { [void]$errs.Add($tag + ':class_mismatch') }
        if ($e.sha256 -isnot [string] -or $e.sha256 -cnotmatch '^[0-9a-f]{64}\z') { [void]$errs.Add($tag + ':bad_sha256') }
        if (-not ((($e.size -is [int]) -or ($e.size -is [long])) -and $e.size -ge 0)) { [void]$errs.Add($tag + ':bad_size') }
        if ($e.valid -isnot [bool]) { [void]$errs.Add($tag + ':bad_valid') }
    }
    if ($errs.Count -eq 0) {
        $digest = Get-MMOEntriesDigest $m.entries
        if ($m.entries_digest -isnot [string] -or $m.entries_digest -cne $digest) { [void]$errs.Add('digest_mismatch') }
    }
    return [pscustomobject]@{ Ok = ($errs.Count -eq 0); Incomplete = $false; Errors = @($errs.ToArray()); Manifest = $m }
}

function Test-MMOSnapshot {
    # Goruntu dizinini (manifest.json + files\) salt okuma ile dogrular: boyut, SHA-256, fazladan dosya, reparse.
    # Donus: Ok, Incomplete, Errors[], Extra[], InvalidEntries[] (manifestte valid=false olanlar; hata sayilmaz).
    param([string]$SnapshotDir)
    $errs = New-Object System.Collections.ArrayList
    $extra = New-Object System.Collections.ArrayList
    $invalid = New-Object System.Collections.ArrayList
    $r = Read-MMOManifest -SnapshotDir $SnapshotDir
    if (-not $r.Ok) {
        return [pscustomobject]@{ Ok = $false; Incomplete = $r.Incomplete; Errors = $r.Errors; Extra = @(); InvalidEntries = @() }
    }
    $filesRoot = Join-Path $SnapshotDir 'files'
    if (Test-MMOReparse $SnapshotDir) { [void]$errs.Add('reparse_snapshot') }
    if (-not [IO.Directory]::Exists($filesRoot)) {
        [void]$errs.Add('files_missing')
        return [pscustomobject]@{ Ok = $false; Incomplete = $false; Errors = @($errs.ToArray()); Extra = @(); InvalidEntries = @() }
    }
    $declared = New-Object 'System.Collections.Generic.HashSet[string]' ([StringComparer]::OrdinalIgnoreCase)
    foreach ($e in $r.Manifest.entries) {
        [void]$declared.Add($e.path)
        if ($e.valid -eq $false) { [void]$invalid.Add($e.path) }
        try { $full = Resolve-MMOSafePath -Root $filesRoot -RelPath $e.path -MustExist } catch {
            [void]$errs.Add('entry_unusable:' + $e.path + ':' + $_.Exception.Message)
            continue
        }
        $len = (New-Object IO.FileInfo $full).Length
        if ($len -ne [long]$e.size) { [void]$errs.Add('size_mismatch:' + $e.path); continue }
        $sha = Get-MMOFileSha256 $full
        if ($sha -cne $e.sha256) { [void]$errs.Add('hash_mismatch:' + $e.path) }
    }
    try { $actual = Get-MMOTreeFiles -Root $filesRoot } catch {
        [void]$errs.Add('tree_unusable:' + $_.Exception.Message)
        $actual = @()
    }
    foreach ($rel in $actual) {
        if (-not $declared.Contains($rel)) { [void]$extra.Add($rel); [void]$errs.Add('extra_file:' + $rel) }
    }
    return [pscustomobject]@{
        Ok = ($errs.Count -eq 0); Incomplete = $false
        Errors = @($errs.ToArray()); Extra = @($extra.ToArray()); InvalidEntries = @($invalid.ToArray())
    }
}
