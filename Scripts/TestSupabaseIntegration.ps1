# TestSupabaseIntegration.ps1 - DESIGN_SUPABASE.md 7.2 Dilim 6
# supabase/tests/vectors.json (contract_version 2) senaryolarini GERCEK Supabase DEV projesine karsi HTTPS ile kosturur.
# Windows PowerShell 5.1 uyumlu, ASCII.
#
# YALNIZ DEV PROJESI ICINDIR. Alfa/uretim projesine KOSTURMA. Betik dev'e 'zz-test-<8hex>-*' anahtarli test karakterleri yazar
# (silme RPC'si yok); temizlik: supabase/tests/zz_cleanup.sql (SQL Editor, postgres rolu).
#
# Gizli bilgi kurali: URL ve anahtar YALNIZ ortam degiskenlerinden okunur (MMO_SUPABASE_URL, MMO_SUPABASE_SECRET_KEY).
# Komut satirinda anahtar parametresi YOKTUR. Anahtar hicbir ciktiya/loga/hata metnine yazilmaz (Redact).
#
# Kullanim:
#   powershell -NoProfile -File Scripts\TestSupabaseIntegration.ps1            # gercek kosu (onay ister: 'dev' yaz)
#   powershell -NoProfile -File Scripts\TestSupabaseIntegration.ps1 -Yes       # onay sorma (otomasyon)
#   powershell -NoProfile -File Scripts\TestSupabaseIntegration.ps1 -DryRun    # ag yok; vectors.json okunur, plan yazilir
#   ... -Scenario happy_path,stale_revision                                    # yalniz secilen senaryolar
#
# Cikis kodu: 0 = hepsi PASS (ya da yalniz SKIP), 1 = FAIL, 3 = calismadi (anahtar/URL yok, onay yok, on kosul basarisiz).
param(
    [switch]$DryRun,
    [switch]$Yes,
    [string]$VectorsPath = "",
    [string[]]$Scenario = @()
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 2

$UserAgent = 'MMOGameServer-IntegrationTest/0.1'
$TimeoutMs = 30000
$ExpectedContract = 2
$OpPattern = New-Object System.Text.RegularExpressions.Regex('^00000000-0000-4000-8000-[0-9]{12}$')
# Kira suresi admin adimiyla doldurulamazsa bu RPC'ler (ayni anahtar icin) SKIP edilir; salt okuma RPC'leri calisir.
$LeaseDependent = @('save_character', 'renew_lease', 'acquire_lease', 'release_lease')

$script:Key = ''
$script:BaseUrl = ''
$script:Hex = [guid]::NewGuid().ToString('N').Substring(0, 8)
$script:OpMap = @{}
$script:Fixtures = $null
$script:Written = New-Object 'System.Collections.Generic.List[string]'
$script:DryRun = [bool]$DryRun

# ---------------------------------------------------------------- yardimcilar

function Redact([string]$s) {
    if ($null -eq $s) { return '' }
    if ($script:Key.Length -ge 8) { $s = $s.Replace($script:Key, '***') }
    $s = [regex]::Replace($s, 'sb_(secret|publishable)_[A-Za-z0-9_\-]+', 'sb_$1_***')
    $s = [regex]::Replace($s, 'eyJ[A-Za-z0-9_\-]{10,}(\.[A-Za-z0-9_\-]+){0,2}', 'eyJ***')
    return $s
}

function Short([string]$s, [int]$n = 300) {
    $s = Redact $s
    if ($s.Length -gt $n) { return $s.Substring(0, $n) + '...' }
    return $s
}

function Is-Num($v) {
    return ($v -is [int] -or $v -is [long] -or $v -is [double] -or $v -is [decimal] -or $v -is [single] -or $v -is [int16] -or $v -is [byte] -or $v -is [uint32] -or $v -is [uint64])
}

function ConvertTo-JsonText($v) {
    if ($null -eq $v) { return 'null' }
    if ($v -is [string]) {
        $sb = New-Object System.Text.StringBuilder
        [void]$sb.Append('"')
        foreach ($ch in $v.ToCharArray()) {
            $c = [int]$ch
            if ($ch -eq '"') { [void]$sb.Append('\"') }
            elseif ($ch -eq '\') { [void]$sb.Append('\\') }
            elseif ($c -eq 8) { [void]$sb.Append('\b') }
            elseif ($c -eq 9) { [void]$sb.Append('\t') }
            elseif ($c -eq 10) { [void]$sb.Append('\n') }
            elseif ($c -eq 12) { [void]$sb.Append('\f') }
            elseif ($c -eq 13) { [void]$sb.Append('\r') }
            elseif ($c -lt 32 -or $c -gt 126) { [void]$sb.Append(('\u{0:x4}' -f $c)) }
            else { [void]$sb.Append($ch) }
        }
        [void]$sb.Append('"')
        return $sb.ToString()
    }
    if ($v -is [bool]) { if ($v) { return 'true' } else { return 'false' } }
    if (Is-Num $v) { return ([string]::Format([System.Globalization.CultureInfo]::InvariantCulture, '{0}', $v)) }
    if ($v -is [System.Collections.IDictionary]) {
        $parts = @()
        foreach ($k in $v.Keys) { $parts += ((ConvertTo-JsonText ([string]$k)) + ':' + (ConvertTo-JsonText $v[$k])) }
        return '{' + ($parts -join ',') + '}'
    }
    if ($v -is [System.Management.Automation.PSCustomObject]) {
        $parts = @()
        foreach ($p in $v.PSObject.Properties) { $parts += ((ConvertTo-JsonText ([string]$p.Name)) + ':' + (ConvertTo-JsonText $p.Value)) }
        return '{' + ($parts -join ',') + '}'
    }
    if ($v -is [System.Collections.IEnumerable]) {
        $parts = @()
        foreach ($e in $v) { $parts += (ConvertTo-JsonText $e) }
        return '[' + ($parts -join ',') + ']'
    }
    return (ConvertTo-JsonText ([string]$v))
}

function Show-Val($v) { return (Short (ConvertTo-JsonText $v) 220) }

# vectors.json yer tutucularini bu kosuya ozgu degerlere cevirir:
#   '@isim'              -> fixtures[isim]
#   '0000...-NNNN' op_id -> kosu basina yeni GUID (ayni dize hep ayni GUID)
#   'zz-v-<x>' (ad/anahtar/onek) -> 'zz-test-<8hex>-<x>' (buyuk harfli ONEK 'ZZ-TEST-' olarak kalir: bad_key testi bozulmaz)
#   'srvA:boot1' / 'srvB:boot2'  -> 'itest:<8hex>-a' / 'itest:<8hex>-b'
function Resolve-Value($v) {
    if ($null -eq $v) { return $null }
    if ($v -is [string]) {
        if ($v.Length -gt 1 -and $v[0] -eq '@') {
            $fp = $script:Fixtures.PSObject.Properties[$v.Substring(1)]
            if ($null -ne $fp) { return (Resolve-Value $fp.Value) }
        }
        if ($OpPattern.IsMatch($v)) {
            if (-not $script:OpMap.ContainsKey($v)) { $script:OpMap[$v] = [guid]::NewGuid().ToString() }
            return $script:OpMap[$v]
        }
        if ($v -ceq 'srvA:boot1') { return ('itest:' + $script:Hex + '-a') }
        if ($v -ceq 'srvB:boot2') { return ('itest:' + $script:Hex + '-b') }
        $m = [regex]::Match($v, '^([Zz][Zz])-[Vv]-(.*)$')
        if ($m.Success) {
            if ($m.Groups[1].Value -ceq 'ZZ') { $pre = 'ZZ-TEST-' } else { $pre = 'zz-test-' }
            return ($pre + $script:Hex + '-' + $m.Groups[2].Value)
        }
        return $v
    }
    if ($v -is [System.Management.Automation.PSCustomObject]) {
        $o = [ordered]@{}
        foreach ($p in $v.PSObject.Properties) { $o[$p.Name] = Resolve-Value $p.Value }
        return [pscustomobject]$o
    }
    if ($v -is [array]) {
        $list = New-Object System.Collections.ArrayList
        foreach ($e in $v) { [void]$list.Add((Resolve-Value $e)) }
        return ,($list.ToArray())
    }
    return $v
}

# jsonb @> benzeri karsilastirma; exact=true ise tam esitlik. $null = eslesti, aksi halde aciklama.
function Compare-Json($exp, $act, [bool]$exact, [string]$path) {
    if ($null -eq $exp) {
        if ($null -eq $act) { return $null }
        return "$path beklenen null, gelen $(Show-Val $act)"
    }
    if ($null -eq $act) { return "$path beklenen $(Show-Val $exp), gelen null/eksik" }
    if ($exp -is [string]) {
        if (($act -is [string]) -and ($act -ceq $exp)) { return $null }
        return "$path beklenen $(Show-Val $exp), gelen $(Show-Val $act)"
    }
    if ($exp -is [bool]) {
        if (($act -is [bool]) -and ($act -eq $exp)) { return $null }
        return "$path beklenen $(Show-Val $exp), gelen $(Show-Val $act)"
    }
    if (Is-Num $exp) {
        if ((Is-Num $act) -and ([double]$act -eq [double]$exp)) { return $null }
        return "$path beklenen sayi $(Show-Val $exp), gelen $(Show-Val $act) (bigint JSON SAYI olmali, metin degil)"
    }
    if ($exp -is [System.Management.Automation.PSCustomObject]) {
        if (-not ($act -is [System.Management.Automation.PSCustomObject])) { return "$path beklenen nesne, gelen $(Show-Val $act)" }
        $actNames = @($act.PSObject.Properties | ForEach-Object { $_.Name })
        $expNames = @($exp.PSObject.Properties | ForEach-Object { $_.Name })
        foreach ($p in $exp.PSObject.Properties) {
            if ($actNames -notcontains $p.Name) { return "$path.$($p.Name) eksik" }
            $r = Compare-Json $p.Value $act.PSObject.Properties[$p.Name].Value $exact "$path.$($p.Name)"
            if ($null -ne $r) { return $r }
        }
        if ($exact) {
            foreach ($n in $actNames) {
                if ($expNames -notcontains $n) { return "$path fazladan anahtar '$n' (exact: anahtar kumesi esit olmali)" }
            }
        }
        return $null
    }
    if ($exp -is [array]) {
        if (-not ($act -is [array])) { return "$path beklenen dizi, gelen $(Show-Val $act)" }
        if ($exact) {
            if ($act.Count -ne $exp.Count) { return "$path dizi uzunlugu beklenen $($exp.Count), gelen $($act.Count)" }
            for ($i = 0; $i -lt $exp.Count; $i++) {
                $r = Compare-Json $exp[$i] $act[$i] $exact "$path[$i]"
                if ($null -ne $r) { return $r }
            }
            return $null
        }
        foreach ($e in $exp) {
            $found = $false
            foreach ($a in $act) { if ($null -eq (Compare-Json $e $a $false $path)) { $found = $true; break } }
            if (-not $found) { return "$path dizide beklenen oge yok: $(Show-Val $e)" }
        }
        return $null
    }
    return "$path desteklenmeyen beklenen tur"
}

function Read-ErrStream($resp) {
    try {
        $st = $resp.GetResponseStream()
        $sr = New-Object System.IO.StreamReader($st, [System.Text.Encoding]::UTF8)
        $t = $sr.ReadToEnd()
        $sr.Close()
        return $t
    } catch { return '' }
}

# Tek RPC cagrisi. Donus: @{ Status; Body; Error }. Status 0 = ag/zaman asimi hatasi (Error redakte).
function Invoke-Rpc([string]$fn, $rpcArgs) {
    $uri = $script:BaseUrl + '/rest/v1/rpc/' + $fn
    $body = ConvertTo-JsonText $rpcArgs
    $bytes = [System.Text.Encoding]::UTF8.GetBytes($body)
    $status = 0; $text = ''; $err = ''
    $resp = $null
    try {
        $req = [System.Net.HttpWebRequest]::Create($uri)
        $req.Method = 'POST'
        $req.UserAgent = $UserAgent
        $req.ContentType = 'application/json'
        $req.Accept = 'application/json'
        $req.Timeout = $TimeoutMs
        $req.ReadWriteTimeout = $TimeoutMs
        $req.AllowAutoRedirect = $false      # yonlendirmede anahtar baska yere gitmesin
        $req.Headers.Add('apikey', $script:Key)
        $req.Headers.Add('Content-Profile', 'mmo_api')
        $req.Headers.Add('Accept-Profile', 'mmo_api')
        if ($script:Key.StartsWith('eyJ') -and (($script:Key.Split('.')).Count -eq 3)) {
            $req.Headers.Add('Authorization', 'Bearer ' + $script:Key)     # yalniz legacy JWT anahtari
        }
        $req.ContentLength = $bytes.Length
        $rs = $req.GetRequestStream()
        $rs.Write($bytes, 0, $bytes.Length)
        $rs.Close()
        $resp = $req.GetResponse()
        $status = [int]$resp.StatusCode
        $text = Read-ErrStream $resp
    } catch [System.Net.WebException] {
        $we = $_.Exception
        if ($null -ne $we.Response) {
            $status = [int]$we.Response.StatusCode
            $text = Read-ErrStream $we.Response
        } else {
            $err = Redact ([string]$we.Message)
        }
    } catch {
        $err = Redact ([string]$_.Exception.Message)
    } finally {
        if ($null -ne $resp) { try { $resp.Close() } catch { } }
    }
    return [pscustomobject]@{ Status = $status; Body = (Redact $text); Error = $err }
}

function Parse-Json([string]$t) {
    if ([string]::IsNullOrWhiteSpace($t)) { return $null }
    try { return ($t | ConvertFrom-Json) } catch { return $null }
}

function Finish([int]$code) { exit $code }

# ---------------------------------------------------------------- vectors.json

if ([string]::IsNullOrEmpty($VectorsPath)) {
    $VectorsPath = Join-Path (Join-Path (Split-Path -Parent $PSScriptRoot) 'supabase') 'tests\vectors.json'
}
if ($UserAgent -match '(?i)Mozilla|Chrome|Safari|Gecko|AppleWebKit|Edge') {
    Write-Host 'HATA: User-Agent tarayici gibi gorunuyor (betik hatasi).'
    Finish 3
}
if (-not (Test-Path -LiteralPath $VectorsPath)) {
    Write-Host ('calismadi: vectors.json yok: ' + $VectorsPath)
    Write-Host '[MMO_SUPABASE_INTEGRATION] NOTRUN reason=vectors_missing'
    Finish 3
}
try {
    $vec = (Get-Content -LiteralPath $VectorsPath -Raw -Encoding UTF8) | ConvertFrom-Json
} catch {
    Write-Host ('calismadi: vectors.json ayrisamadi: ' + $_.Exception.Message)
    Write-Host '[MMO_SUPABASE_INTEGRATION] NOTRUN reason=vectors_parse'
    Finish 3
}
if ($vec.contract_version -ne $ExpectedContract) {
    Write-Host ('calismadi: vectors.json contract_version=' + $vec.contract_version + ', betik ' + $ExpectedContract + ' bekliyor')
    Write-Host '[MMO_SUPABASE_INTEGRATION] NOTRUN reason=contract_version'
    Finish 3
}
$script:Fixtures = $vec.fixtures
$allScenarios = @($vec.scenarios)
$scenarios = @($allScenarios | Where-Object { ($Scenario.Count -eq 0) -or ($Scenario -contains $_.id) })
if ($scenarios.Count -eq 0) {
    Write-Host 'calismadi: secilen senaryo yok.'
    Write-Host '[MMO_SUPABASE_INTEGRATION] NOTRUN reason=no_scenario'
    Finish 3
}
$totalSteps = 0; $adminSteps = 0; $errSteps = 0
foreach ($s in $scenarios) {
    foreach ($st in $s.steps) {
        $totalSteps++
        if ($null -ne $st.PSObject.Properties['admin']) { $adminSteps++ }
        elseif ($null -ne $st.PSObject.Properties['expect_error']) { $errSteps++ }
    }
}

# ---------------------------------------------------------------- ortam / onay (DryRun degil)

if (-not $script:DryRun) {
    $envUrl = [Environment]::GetEnvironmentVariable('MMO_SUPABASE_URL')
    $envKey = [Environment]::GetEnvironmentVariable('MMO_SUPABASE_SECRET_KEY')
    if ([string]::IsNullOrWhiteSpace($envUrl) -or [string]::IsNullOrWhiteSpace($envKey)) {
        Write-Host 'calismadi (anahtar yok): MMO_SUPABASE_URL ve/veya MMO_SUPABASE_SECRET_KEY ortam degiskeni tanimli degil.'
        Write-Host '[MMO_SUPABASE_INTEGRATION] NOTRUN reason=no_key'
        Finish 3
    }
    $script:Key = $envKey.Trim()
    $script:BaseUrl = $envUrl.Trim().TrimEnd('/')
    if (-not $script:BaseUrl.StartsWith('https://', [System.StringComparison]::OrdinalIgnoreCase)) {
        Write-Host 'calismadi: MMO_SUPABASE_URL HTTPS olmali (http reddedilir).'
        Write-Host '[MMO_SUPABASE_INTEGRATION] NOTRUN reason=url_not_https'
        Finish 3
    }
    try { $hostName = ([uri]$script:BaseUrl).Host } catch { $hostName = '' }
    if ([string]::IsNullOrEmpty($hostName)) {
        Write-Host 'calismadi: MMO_SUPABASE_URL ayrisamadi.'
        Write-Host '[MMO_SUPABASE_INTEGRATION] NOTRUN reason=url_parse'
        Finish 3
    }
    try { [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12 } catch { }
    Write-Host ('HEDEF HOST: ' + $hostName + '   (anahtar yazdirilmaz)')
    Write-Host 'Bu betik YALNIZ DEV projesi icindir; zz-test-* test karakterleri YAZAR (silme RPC yok; zz_cleanup.sql ile temizlenir).'
    if (-not $Yes) {
        if ([Console]::IsInputRedirected) {
            Write-Host 'calismadi: etkilesimli onay yok. Otomasyon icin -Yes ver (yalniz dev projesi icin).'
            Write-Host '[MMO_SUPABASE_INTEGRATION] NOTRUN reason=no_confirm'
            Finish 3
        }
        $ans = Read-Host ('Host "' + $hostName + '" gercekten DEV projesi mi? Onaylamak icin dev yaz')
        if ($ans.Trim().ToLowerInvariant() -ne 'dev') {
            Write-Host 'calismadi: onay verilmedi.'
            Write-Host '[MMO_SUPABASE_INTEGRATION] NOTRUN reason=not_confirmed'
            Finish 3
        }
    }
} else {
    Write-Host 'DRY RUN: ag yok, anahtar okunmaz.'
}

Write-Host ('vectors.json contract_version=' + $vec.contract_version + ' senaryo=' + $scenarios.Count + ' adim=' + $totalSteps + ' (admin=' + $adminSteps + ', expect_error=' + $errSteps + ')')
Write-Host ('test onek: zz-test-' + $script:Hex + '-   holder: itest:' + $script:Hex + '-a / -b')

# ---------------------------------------------------------------- on kosul: healthcheck

if (-not $script:DryRun) {
    $pre = Invoke-Rpc 'healthcheck' ([pscustomobject]@{})
    $pj = Parse-Json $pre.Body
    $ok = ($pre.Status -eq 200) -and ($null -ne $pj) -and ($null -ne $pj.PSObject.Properties['outcome']) -and ($pj.outcome -eq 'ok') -and ($pj.contract_version -eq $ExpectedContract)
    if (-not $ok) {
        Write-Host ('calismadi: on kosul healthcheck basarisiz. HTTP=' + $pre.Status + ' govde=' + (Short $pre.Body 300) + ' hata=' + (Short $pre.Error 200))
        Write-Host '  ipucu: "Forbidden use of secret API key in browser" = User-Agent; 401 = apikey yanlis; 404 PGRST202 = Content-Profile/Exposed schemas (mmo_api).'
        Write-Host '[MMO_SUPABASE_INTEGRATION] NOTRUN reason=healthcheck'
        Finish 3
    }
    Write-Host ('healthcheck ok: api_version=' + $pj.api_version + ' contract_version=' + $pj.contract_version + ' schema=' + $pj.schema)
}

# ---------------------------------------------------------------- senaryo kosusu

$script:KeyRegex = '^[a-z0-9_-]{1,32}$'

# Donus: $null = gecti, aksi halde hata metni
function Run-Step($st, $rpcArgs, $exp) {
    if ($script:DryRun) {
        $null = ConvertTo-JsonText $rpcArgs
        $kp = $rpcArgs.PSObject.Properties['p_key']
        if ($null -ne $kp -and ($kp.Value -is [string]) -and $kp.Value.StartsWith('zz-test-') -and ($kp.Value -cnotmatch $script:KeyRegex)) {
            return ('uretilen anahtar kurala uymuyor: ' + $kp.Value)
        }
        $dn = $rpcArgs.PSObject.Properties['p_display_name']
        if ($null -ne $dn -and ($dn.Value -is [string]) -and $dn.Value.ToLowerInvariant().StartsWith('zz-test-') -and ($dn.Value.Length -gt 32)) {
            return ('uretilen ad 32 karakteri asiyor: ' + $dn.Value)
        }
        return $null
    }
    $r = Invoke-Rpc $st.rpc $rpcArgs
    if ($null -ne $st.PSObject.Properties['expect_error']) {
        $ee = $st.expect_error
        $j = Parse-Json $r.Body
        $code = ''
        if ($null -ne $j -and ($j -is [System.Management.Automation.PSCustomObject]) -and ($null -ne $j.PSObject.Properties['code'])) { $code = [string]$j.code }
        if (($r.Status -eq [int]$ee.http_status) -and ($code -ceq [string]$ee.code)) { return $null }
        return ('beklenen HTTP ' + $ee.http_status + ' ' + $ee.code + ', gelen HTTP ' + $r.Status + ' code=' + $code + ' govde=' + (Short $r.Body 200) + ' hata=' + (Short $r.Error 160))
    }
    if ($r.Status -ne 200) {
        return ('beklenen HTTP 200, gelen HTTP ' + $r.Status + ' govde=' + (Short $r.Body 240) + ' hata=' + (Short $r.Error 160))
    }
    $j = Parse-Json $r.Body
    if ($null -eq $j -or -not ($j -is [System.Management.Automation.PSCustomObject])) {
        return ('govde JSON nesnesi degil: ' + (Short $r.Body 240))
    }
    $exact = ($null -ne $st.PSObject.Properties['exact']) -and ($st.exact -eq $true)
    $diff = Compare-Json $exp $j $exact '$'
    if ($null -ne $diff) {
        return ($diff + '  | beklenen=' + (Show-Val $exp) + ' gelen=' + (Short $r.Body 300))
    }
    return $null
}

$totChecks = 0; $totSkip = 0; $totFail = 0; $totNotRun = 0
$lines = New-Object System.Collections.ArrayList

foreach ($sc in $scenarios) {
    $skipKeys = @{}
    $nRun = 0; $nSkip = 0; $notRun = 0; $failMsg = $null; $idx = 0
    $detail = New-Object System.Collections.ArrayList
    foreach ($st in $sc.steps) {
        $idx++
        if ($null -ne $failMsg) { $notRun++; continue }
        if ($null -ne $st.PSObject.Properties['admin']) {
            $ak = ''
            if ($null -ne $st.PSObject.Properties['key']) { $ak = [string](Resolve-Value $st.key) }
            if ($st.admin -eq 'expire_lease') { $skipKeys[$ak] = $true }
            $nSkip++
            [void]$detail.Add(('    [SKIP] adim ' + $idx + ': admin ' + $st.admin + ' ' + $ak + ' - API ile yapilamaz (lease_until gecmise cekmek SQL Editor ister); ayni anahtarda save/renew/acquire/release adimlari atlanir'))
            continue
        }
        $rpcArgs = Resolve-Value $st.args
        $exp = $null
        if ($null -ne $st.PSObject.Properties['expect']) { $exp = Resolve-Value $st.expect }
        $k = ''
        $kp = $rpcArgs.PSObject.Properties['p_key']
        if ($null -ne $kp -and ($kp.Value -is [string])) { $k = $kp.Value }
        if (($k -ne '') -and $skipKeys.ContainsKey($k) -and ($LeaseDependent -contains $st.rpc)) {
            $nSkip++
            [void]$detail.Add(('    [SKIP] adim ' + $idx + ': ' + $st.rpc + ' ' + $k + ' - kira suresi doldurma (expire_lease) adimi atlandi; bu adim gecerli kirada anlamini yitirir'))
            continue
        }
        if (($st.rpc -eq 'create_character' -or $st.rpc -eq 'import_character') -and ($k -ne '') -and $k.StartsWith('zz-test-') -and (-not $script:Written.Contains($k))) {
            $script:Written.Add($k)
        }
        $msg = Run-Step $st $rpcArgs $exp
        $nRun++
        if ($null -ne $msg) {
            $failMsg = 'adim ' + $idx + ' (' + $st.rpc + ' ' + $k + '): ' + $msg
        }
    }
    $totChecks += $nRun; $totSkip += $nSkip; $totNotRun += $notRun
    if ($null -ne $failMsg) {
        $totFail++
        Write-Host ('[FAIL] ' + $sc.id + '  (calisan=' + $nRun + ', skip=' + $nSkip + ', calistirilmadi=' + $notRun + ')')
        Write-Host ('    ' + (Redact $failMsg))
    } elseif ($nRun -eq 0) {
        Write-Host ('[SKIP] ' + $sc.id + '  (' + $nSkip + ' adim atlandi)')
    } elseif ($nSkip -gt 0) {
        Write-Host ('[PASS] ' + $sc.id + '  (kismi: ' + $nRun + ' adim kosuldu, ' + $nSkip + ' adim SKIP)')
    } else {
        $tag = 'PASS'
        if ($script:DryRun) { $tag = 'PLAN' }
        Write-Host ('[' + $tag + '] ' + $sc.id + '  (' + $nRun + ' adim)')
    }
    foreach ($d in $detail) { Write-Host $d }
}

# ---------------------------------------------------------------- ozet / temizlik

Write-Host ''
if ($script:DryRun) {
    Write-Host ('DRY RUN ozeti: senaryo=' + $scenarios.Count + ' adim=' + $totalSteps + ' kosulacak=' + $totChecks + ' atlanacak(admin ve bagimli)=' + $totSkip + ' yerel-hata=' + $totFail)
    Write-Host ('Yazilacak test anahtari sayisi: ' + $script:Written.Count + ' (onek zz-test-' + $script:Hex + '-)')
    if ($totFail -gt 0) { Write-Host ('[MMO_SUPABASE_INTEGRATION] DRYRUN FAIL scenarios=' + $scenarios.Count); Finish 1 }
    Write-Host ('[MMO_SUPABASE_INTEGRATION] DRYRUN OK scenarios=' + $scenarios.Count + ' steps=' + $totalSteps + ' planned_skips=' + $totSkip)
    Finish 0
}

Write-Host 'Yazilan test anahtarlari (silme RPC yok):'
foreach ($w in $script:Written) { Write-Host ('  ' + $w) }
Write-Host ('  onek: zz-test-' + $script:Hex + '-   (op_id sayisi: ' + $script:OpMap.Count + ')')
Write-Host 'TEMIZLIK: supabase/tests/zz_cleanup.sql dosyasini Supabase SQL Editor''de (postgres rolu) calistir; yalniz zz-test-* satirlarini siler.'
Write-Host ''
if ($totFail -gt 0) {
    Write-Host ('[MMO_SUPABASE_INTEGRATION] FAIL checks=' + $totChecks + ' skipped=' + $totSkip)
    Finish 1
}
Write-Host ('[MMO_SUPABASE_INTEGRATION] PASS checks=' + $totChecks + ' skipped=' + $totSkip)
Finish 0
