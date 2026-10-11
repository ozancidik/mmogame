# Offline fixture tests only. Run this file once in pwsh and once in Windows PowerShell.
# Retains all fixtures and evidence in TEMP; never launches Unreal or changes live mail.
[CmdletBinding()]
param(
    [string]$ExporterPath = '',
    [ValidateRange(2, 30)][int]$CaseTimeoutSeconds = 10,
    [ValidateRange(30, 1200)][int]$SuiteTimeoutSeconds = 600
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
if ([string]::IsNullOrEmpty($ExporterPath)) { $ExporterPath = Join-Path $PSScriptRoot 'ExportMigratedMailboxes.ps1' }
$script:Utf8 = New-Object Text.UTF8Encoding($false)
$script:Results = New-Object 'System.Collections.Generic.List[object]'
$script:Watch = [Diagnostics.Stopwatch]::StartNew()
$script:Repo = Split-Path -Parent $PSScriptRoot
$script:HostExe = (Get-Process -Id $PID).Path
$script:RunnerSha256 = (Get-FileHash -LiteralPath $PSCommandPath -Algorithm SHA256).Hash
$script:TempRoot = (Get-Item -LiteralPath ([IO.Path]::GetTempPath()) -Force).FullName
$script:Proof = Join-Path $script:TempRoot ('mmo_mail_export_qa_' + $PSVersionTable.PSVersion.ToString() + '_' + [guid]::NewGuid().ToString('N'))

function Write-Text([string]$Path, [string]$Text) {
    [IO.File]::WriteAllText($Path, $Text, $script:Utf8)
}
function Save-Json([string]$Path, $Value) {
    Write-Text $Path (ConvertTo-Json -InputObject $Value -Depth 40)
}
function Assert-True([bool]$Condition, [string]$Reason) {
    if (-not $Condition) { throw $Reason }
}
function Get-Inventory([string]$Root) {
    $rows = New-Object 'System.Collections.Generic.List[string]'
    if (-not (Test-Path -LiteralPath $Root)) { return '<absent>' }
    $Root = (Get-Item -LiteralPath $Root -Force).FullName.TrimEnd('\', '/')
    $pending = New-Object 'System.Collections.Generic.Queue[string]'
    $pending.Enqueue([IO.Path]::GetFullPath($Root))
    while ($pending.Count -gt 0) {
        $dir = $pending.Dequeue()
        foreach ($entry in @(Get-ChildItem -LiteralPath $dir -Force)) {
            $relative = $entry.FullName.Substring($Root.Length).TrimStart('\', '/')
            if (($entry.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
                $rows.Add('LINK|' + $relative + '|' + [string]$entry.Target)
            } elseif ($entry.PSIsContainer) {
                $rows.Add('DIR|' + $relative)
                $pending.Enqueue($entry.FullName)
            } else {
                $rows.Add('FILE|' + $relative + '|' + $entry.Length + '|' + (Get-FileHash -LiteralPath $entry.FullName -Algorithm SHA256).Hash)
                foreach ($stream in @(Get-Item -LiteralPath $entry.FullName -Stream *)) {
                    if ($stream.Stream -ne ':$DATA') {
                        if ($PSVersionTable.PSVersion.Major -ge 6) {
                            [byte[]]$adsBytes = Get-Content -LiteralPath $entry.FullName -Stream $stream.Stream -AsByteStream -Raw
                        } else {
                            [byte[]]$adsBytes = Get-Content -LiteralPath $entry.FullName -Stream $stream.Stream -Encoding Byte -ReadCount 0
                        }
                        $sha = [Security.Cryptography.SHA256]::Create()
                        try { $adsHash = ([BitConverter]::ToString($sha.ComputeHash($adsBytes))).Replace('-', '') }
                        finally { $sha.Dispose() }
                        $rows.Add('ADS|' + $relative + ':' + $stream.Stream + '|' + $stream.Length + '|' + $adsHash)
                    }
                }
            }
        }
    }
    return (@($rows | Sort-Object -CaseSensitive) -join "`n")
}
function Get-PeerInventory {
    $paths = @(& git -C $script:Repo ls-files --cached --others --exclude-standard)
    Assert-True ($LASTEXITCODE -eq 0) 'git inventory failed'
    $rows = foreach ($relative in $paths) {
        if ($relative -eq 'Scripts/TestMailMigrationExport.ps1') { continue }
        $absolute = Join-Path $script:Repo $relative
        if (Test-Path -LiteralPath $absolute -PathType Leaf) {
            $file = Get-Item -LiteralPath $absolute
            $relative + '|' + $file.Length + '|' + (Get-FileHash -LiteralPath $absolute -Algorithm SHA256).Hash
        } else { $relative + '|<absent>' }
    }
    return (@($rows | Sort-Object -CaseSensitive) -join "`n")
}
function New-Fixture([string]$Name) {
    $root = Join-Path $script:Proof $Name
    $null = [IO.Directory]::CreateDirectory($root)
    $source = Join-Path $root 'snapshot'
    $null = [IO.Directory]::CreateDirectory($source)
    return [pscustomobject]@{ Root = $root; Source = $source; Manifest = (Join-Path $root 'provenance.json'); Output = (Join-Path $root 'staging'); Entries = @() }
}
function New-Mail([string]$From = 'Sender', [string]$Text = 'Fixture', $Gold = 123, $Sent = 1700000000) {
    return [ordered]@{ from = $From; text = $Text; gold = $Gold; sent = $Sent }
}
function New-Box($Mails = @((New-Mail))) {
    return ConvertTo-Json -InputObject ([ordered]@{ version = 1; mails = @($Mails) }) -Depth 30
}
function Add-Box($Fixture, [string]$Filename, [string]$Format = 'legacy', [string]$Owner = 'Ali', [string]$Raw = (New-Box)) {
    $path = Join-Path $Fixture.Source $Filename
    Write-Text $path $Raw
    $Fixture.Entries += [ordered]@{ filename = $Filename; format = $Format; owner = $Owner; sha256 = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash }
}
function Write-Provenance($Fixture) {
    Save-Json $Fixture.Manifest ([ordered]@{ version = 1; files = @($Fixture.Entries) })
}
function Invoke-Export($Fixture, [switch]$Apply, [switch]$WhatIf, [switch]$NoAcknowledgement, [switch]$InterruptAfterMarker) {
    Assert-True ($script:Watch.Elapsed.TotalSeconds -lt $SuiteTimeoutSeconds) 'suite watchdog exceeded'
    $sourceBefore = Get-Inventory $Fixture.Source
    $provenanceBefore = if (Test-Path -LiteralPath $Fixture.Manifest -PathType Leaf) { (Get-FileHash -LiteralPath $Fixture.Manifest).Hash } else { '<absent>' }
    $id = [guid]::NewGuid().ToString('N')
    $logRoot = Join-Path (Join-Path $script:Proof 'invocations') (Split-Path -Leaf $Fixture.Root)
    $null = [IO.Directory]::CreateDirectory($logRoot)
    Write-Text (Join-Path $logRoot ($id + '.source-before.txt')) $sourceBefore
    $arguments = @('-NoLogo', '-NoProfile', '-NonInteractive', '-ExecutionPolicy', 'Bypass', '-File', $ExporterPath,
        '-SourceDirectory', $Fixture.Source, '-ProvenanceManifest', $Fixture.Manifest, '-StagingDirectory', $Fixture.Output)
    if (-not $NoAcknowledgement) { $arguments += '-AcknowledgeFrozenSnapshot' }
    if ($Apply) { $arguments += '-Apply' }
    if ($WhatIf) { $arguments += '-WhatIf' }
    $finished = $true
    $interrupted = $false
    $stdout = ''
    $stderr = ''
    $code = 0
    if (-not $InterruptAfterMarker -and -not $NoAcknowledgement) {
        # The frozen exporter uses bounded parsing and throws/returns, never exit.
        # A separate script scope in this host avoids hundreds of shell startups.
        # Interruption and mandatory-parameter tests below retain owned processes.
        $invokeArgs = @{ SourceDirectory = $Fixture.Source; ProvenanceManifest = $Fixture.Manifest;
            StagingDirectory = $Fixture.Output; AcknowledgeFrozenSnapshot = $true }
        if ($Apply) { $invokeArgs.Apply = $true }
        if ($WhatIf) { $invokeArgs.WhatIf = $true }
        $caseWatch = [Diagnostics.Stopwatch]::StartNew()
        try { $stdout = (@(& $ExporterPath @invokeArgs 2>&1 3>&1 4>&1 5>&1 6>&1) | Out-String -Width 4096) }
        catch { $code = 1; $stderr = ($_ | Out-String -Width 4096) }
        $finished = $caseWatch.Elapsed.TotalSeconds -le $CaseTimeoutSeconds
        Write-Text (Join-Path $logRoot ($id + '.stdout.log')) $stdout
        Write-Text (Join-Path $logRoot ($id + '.stderr.log')) $stderr
    } else {
    # Each value is passed as a quoted native argument, never evaluated as shell code.
    foreach ($value in $arguments) { Assert-True (-not $value.Contains('"')) 'unsupported quote in fixture argument' }
    $start = New-Object Diagnostics.ProcessStartInfo
    $start.FileName = $script:HostExe
    $start.Arguments = (($arguments | ForEach-Object { '"' + $_ + '"' }) -join ' ')
    $start.UseShellExecute = $false
    $start.CreateNoWindow = $true
    $start.RedirectStandardOutput = $true
    $start.RedirectStandardError = $true
    $process = New-Object Diagnostics.Process
    $process.StartInfo = $start
    try {
        $null = $process.Start()
        $stdoutTask = $process.StandardOutput.ReadToEndAsync()
        $stderrTask = $process.StandardError.ReadToEndAsync()
        if ($InterruptAfterMarker) {
            $deadline = [Diagnostics.Stopwatch]::StartNew()
            $marker = Join-Path $Fixture.Output 'EXPORT_INCOMPLETE.json'
            while (-not $process.HasExited -and $deadline.Elapsed.TotalSeconds -lt $CaseTimeoutSeconds) {
                if ([IO.File]::Exists($marker)) {
                    $process.Kill()
                    $interrupted = $true
                    break
                }
                [Threading.Thread]::Sleep(1)
            }
        }
        $finished = $process.WaitForExit($CaseTimeoutSeconds * 1000)
        if (-not $finished) {
            $process.Kill() # Only this runner's owned exporter process.
            $null = $process.WaitForExit(5000)
        }
        $stdout = $stdoutTask.GetAwaiter().GetResult()
        $stderr = $stderrTask.GetAwaiter().GetResult()
        Write-Text (Join-Path $logRoot ($id + '.stdout.log')) $stdout
        Write-Text (Join-Path $logRoot ($id + '.stderr.log')) $stderr
        $code = $process.ExitCode
    } finally { $process.Dispose() }
    }
    $sourceAfter = Get-Inventory $Fixture.Source
    Write-Text (Join-Path $logRoot ($id + '.source-after.txt')) $sourceAfter
    Assert-True ($sourceBefore -ceq $sourceAfter) 'SOURCE MUTATED: full inventory/hash mismatch'
    $provenanceAfter = if (Test-Path -LiteralPath $Fixture.Manifest -PathType Leaf) { (Get-FileHash -LiteralPath $Fixture.Manifest).Hash } else { '<absent>' }
    Assert-True ($provenanceBefore -ceq $provenanceAfter) 'PROVENANCE MUTATED'
    Assert-True $finished 'exporter case watchdog exceeded'
    return [pscustomobject]@{ Exit = $code; Stdout = $stdout; Stderr = $stderr; Interrupted = $interrupted }
}
function Assert-Rejected($Fixture, [switch]$ExistingOutput, [switch]$NoAcknowledgement, [string]$ExpectedError = '') {
    $before = Get-Inventory $Fixture.Output
    foreach ($apply in @($false, $true)) {
        $result = Invoke-Export $Fixture -Apply:$apply -NoAcknowledgement:$NoAcknowledgement
        Assert-True ($result.Exit -ne 0) ('unsafe input accepted; apply=' + $apply + '; ' + $result.Stdout)
        if ($ExpectedError) { Assert-True (($result.Stdout + $result.Stderr) -match $ExpectedError) ('rejected for a different reason than expected: ' + $result.Stderr) }
        Assert-True ($before -ceq (Get-Inventory $Fixture.Output)) 'rejection changed/created output'
    }
}
function Assert-Copied($Fixture, [string[]]$ExpectedNames) {
    $result = Invoke-Export $Fixture -Apply
    Assert-True ($result.Exit -eq 0) ('valid export rejected: ' + $result.Stderr)
    $actual = @((Get-ChildItem -LiteralPath $Fixture.Output -Force).Name | Sort-Object -CaseSensitive)
    $expected = @(@($ExpectedNames) + @('EXPORT_COMPLETE.json', 'EXPORT_INCOMPLETE.json') | Sort-Object -CaseSensitive)
    Assert-True (($actual -join '|') -ceq ($expected -join '|')) ('output inventory mismatch: ' + ($actual -join '|'))
    foreach ($entry in $Fixture.Entries) {
        $name = 'MMOMail_' + $entry.owner.Replace(' ', '_').ToLowerInvariant() + '.json'
        $target = Join-Path $Fixture.Output $name
        Assert-True ((Get-FileHash -LiteralPath $target).Hash -ceq $entry.sha256.ToUpperInvariant()) ('byte hash changed: ' + $name)
        $a = [IO.File]::ReadAllBytes((Join-Path $Fixture.Source $entry.filename))
        $b = [IO.File]::ReadAllBytes($target)
        Assert-True ([Convert]::ToBase64String($a) -ceq [Convert]::ToBase64String($b)) ('raw bytes changed: ' + $name)
    }
    $complete = Get-Content -LiteralPath (Join-Path $Fixture.Output 'EXPORT_COMPLETE.json') -Raw | ConvertFrom-Json
    Assert-True ($null -ne $complete) 'completion manifest not JSON'
    Assert-True ($complete.complete -eq $true -and $complete.activatable -eq $false) 'completion/activation flags wrong'
    Assert-True ($complete.version -eq 1 -and $complete.purpose -ceq 'offline-mailbox-export-staging-only') 'receipt purpose/version wrong'
    Assert-True ($complete.mailboxCount -eq $Fixture.Entries.Count -and @($complete.files).Count -eq $Fixture.Entries.Count) 'receipt mailbox inventory wrong'
    Assert-True ($complete.provenanceSha256 -ceq (Get-FileHash -LiteralPath $Fixture.Manifest).Hash.ToLowerInvariant()) 'receipt provenance hash wrong'
    [long]$expectedGold = 0
    [long]$expectedMails = 0
    foreach ($entry in $Fixture.Entries) {
        $record = @($complete.files | Where-Object { $_.filename -ceq $entry.filename })
        Assert-True ($record.Count -eq 1) 'receipt source mapping missing/duplicated'
        $record = $record[0]
        $targetName = 'MMOMail_' + $entry.owner.Replace(' ', '_').ToLowerInvariant() + '.json'
        Assert-True ($record.owner -ceq $entry.owner -and $record.format -ceq $entry.format -and $record.output -ceq $targetName) 'receipt changed source ownership mapping'
        Assert-True ($record.sourceSha256 -ceq $entry.sha256.ToLowerInvariant() -and $record.outputSha256 -ceq $entry.sha256.ToLowerInvariant()) 'receipt source/output hashes wrong'
        $sourceFile = Get-Item -LiteralPath (Join-Path $Fixture.Source $entry.filename)
        $box = Get-Content -LiteralPath $sourceFile.FullName -Raw -Encoding UTF8 | ConvertFrom-Json
        $mails = @($box.mails)
        [long]$gold = 0
        foreach ($mail in $mails) { $gold += $mail.gold }
        Assert-True ($record.byteLength -eq $sourceFile.Length -and $record.mailCount -eq $mails.Count -and $record.goldTotal -eq $gold) 'receipt length/mail/gold summaries wrong'
        $expectedGold += $gold
        $expectedMails += $mails.Count
    }
    Assert-True ($complete.goldTotal -eq $expectedGold -and $complete.mailCount -eq $expectedMails) 'receipt aggregate totals wrong'
    $incomplete = Get-Content -LiteralPath (Join-Path $Fixture.Output 'EXPORT_INCOMPLETE.json') -Raw | ConvertFrom-Json
    Assert-True ($incomplete.complete -eq $false -and $incomplete.activatable -eq $false) 'initial staging receipt wrong'
}
function Run-Case([string]$Name, [scriptblock]$Action) {
    try {
        & $Action
        $script:Results.Add([pscustomobject]@{ name = $Name; status = 'PASS'; reason = '' })
        Write-Output ('[MMO_MAIL_EXPORT_QA_CASE] PASS check=' + $Name)
    } catch {
        $script:Results.Add([pscustomobject]@{ name = $Name; status = 'FAIL'; reason = $_.Exception.Message })
        Write-Output ('[MMO_MAIL_EXPORT_QA_CASE] FAIL check=' + $Name + ' reason=' + $_.Exception.Message)
    }
}
function Skip-Case([string]$Name, [string]$Reason) {
    $script:Results.Add([pscustomobject]@{ name = $Name; status = 'SKIP'; reason = $Reason })
    Write-Output ('[MMO_MAIL_EXPORT_QA_CASE] SKIP check=' + $Name + ' reason=' + $Reason)
}
function Get-SubstTarget([string]$Drive) {
    foreach ($line in @(& "$env:SystemRoot/System32/subst.exe")) {
        if ($line -match '^\s*([A-Za-z]):\\:\s*=>\s*(.+)$' -and ($Matches[1] + ':') -ieq $Drive) {
            return $Matches[2].Trim()
        }
    }
    return $null
}
function Test-SubstFixture([string]$Kind) {
    $f = New-Fixture ('subst-' + $Kind); Add-Box $f 'Ali.json'; Write-Provenance $f
    $target = (Get-Item -LiteralPath $f.Root).FullName
    if ($Kind -eq 'checkout') { $target = (Get-Item -LiteralPath $script:Repo).FullName }
    if ($Kind -eq 'Saved') {
        $null = [IO.Directory]::CreateDirectory((Join-Path $f.Root 'Saved'))
        $target = (Get-Item -LiteralPath (Join-Path $f.Root 'Saved')).FullName
    }
    $savedBefore = if ($Kind -eq 'Saved') { Get-Inventory $target } else { $null }
    $drive = $null
    foreach ($number in (90..68)) {
        $candidate = ([string][char]$number) + ':'
        if ([IO.Directory]::Exists($candidate + '\') -or $null -ne (Get-SubstTarget $candidate) -or
            $null -ne (Get-PSDrive -Name ([string][char]$number) -ErrorAction SilentlyContinue)) { continue }
        $drive = $candidate
        break
    }
    Assert-True ($null -ne $drive) 'no unused drive letter for isolated SUBST test'
    $substExe = "$env:SystemRoot/System32/subst.exe"
    $created = $false
    try {
        & $substExe $drive $target
        Assert-True ($LASTEXITCODE -eq 0) 'cannot create owned SUBST alias'
        $created = $true
        Assert-True ((Get-SubstTarget $drive) -ieq $target) 'owned SUBST target verification failed'
        if ($Kind -eq 'overlap') { $f.Output = $drive + '\snapshot\staging' }
        elseif ($Kind -eq 'Saved') { $f.Output = $drive + '\staging' }
        else { $f.Output = $drive + '\qa_disallowed_' + [guid]::NewGuid().ToString('N') }
        Assert-Rejected $f -ExpectedError 'Physical'
        if ($Kind -eq 'Saved') { Assert-True ($savedBefore -ceq (Get-Inventory $target)) 'physical Saved target inventory changed' }
    } finally {
        if ($created) {
            # Remove the DOS drive alias only, and only while its target is still ours.
            Assert-True ((Get-SubstTarget $drive) -ieq $target) 'SUBST ownership changed; refusing to remove alias'
            & $substExe $drive /D
            Assert-True ($LASTEXITCODE -eq 0 -and $null -eq (Get-SubstTarget $drive)) 'owned SUBST alias removal failed'
        }
    }
}

Assert-True (Test-Path -LiteralPath $ExporterPath -PathType Leaf) 'Exporter missing; wait for author readiness before running'
$ExporterPath = (Resolve-Path -LiteralPath $ExporterPath).Path
$null = [IO.Directory]::CreateDirectory($script:Proof)
$peerBefore = Get-PeerInventory
Write-Text (Join-Path $script:Proof 'peer-before.txt') $peerBefore
Write-Output ('[MMO_MAIL_EXPORT_QA] host=' + $script:HostExe + ' version=' + $PSVersionTable.PSVersion + ' proof=' + $script:Proof)

Run-Case 'dryrun_and_whatif_no_output' {
    $f = New-Fixture 'dryrun'; Add-Box $f 'Ali.json'; Write-Provenance $f
    foreach ($whatIf in @($false, $true)) {
        $r = Invoke-Export $f -Apply:$whatIf -WhatIf:$whatIf
        Assert-True ($r.Exit -eq 0) ('dryrun/WhatIf failed: ' + $r.Stderr)
        Assert-True (-not (Test-Path -LiteralPath $f.Output)) 'dryrun created output'
    }
}
Run-Case 'acknowledgement_required' {
    $f = New-Fixture 'ack'; Add-Box $f 'Ali.json'; Write-Provenance $f; Assert-Rejected $f -NoAcknowledgement
}
Run-Case 'legacy_canonical_unicode_rawbytes_gold_fees' {
    $f = New-Fixture 'positive'
    $unicode = 'Posta ' + [char]0x015F + [char]0x0131 + [char]0x4E2D + [char]0x6587
    $mail = @((New-Mail 'Sender' $unicode 12345), (New-Mail 'Sender' '' 1000000), (New-Mail 'Sender' 'No attachment' 0))
    Add-Box $f 'Ali.json' 'legacy' 'Ali' ((New-Box $mail) + "`r`n")
    Add-Box $f 'bob.json' 'legacy' 'Bob' (New-Box @())
    Add-Box $f 'MMOMail_carol.json' 'canonical' 'Carol'
    Write-Provenance $f; Assert-Copied $f @('MMOMail_ali.json', 'MMOMail_bob.json', 'MMOMail_carol.json')
    $exported = Get-Content -LiteralPath (Join-Path $f.Output 'MMOMail_ali.json') -Raw -Encoding UTF8 | ConvertFrom-Json
    Assert-True (($exported.mails | Measure-Object -Property gold -Sum).Sum -eq 1012345) 'attachment gold total changed'
    $feesBefore = @($mail | ForEach-Object { 10 + [Math]::Floor($_.gold * 0.02) }) -join ','
    $feesAfter = @($exported.mails | ForEach-Object { 10 + [Math]::Floor($_.gold * 0.02) }) -join ','
    Assert-True ($feesBefore -ceq $feesAfter) 'gold-based fee calculation inputs changed'
}
Run-Case 'resolved_prefix_legacy_owner' {
    $f = New-Fixture 'prefix-legacy'; Add-Box $f 'MMOMail_Ali.json' 'legacy' 'MMOMail_Ali'; Write-Provenance $f
    Assert-Copied $f @('MMOMail_mmomail_ali.json')
}
Run-Case 'prefix_without_provenance_rejected' {
    $f = New-Fixture 'prefix-unmapped'; Add-Box $f 'MMOMail_Ali.json' 'legacy' 'MMOMail_Ali'
    $f.Entries = @(); Write-Provenance $f; Assert-Rejected $f
}
Run-Case 'resolved_prefix_canonical_owner' {
    $f = New-Fixture 'prefix-canonical'; Add-Box $f 'MMOMail_ali.json' 'canonical' 'Ali'; Write-Provenance $f
    Assert-Copied $f @('MMOMail_ali.json')
}
Run-Case 'both_owners_explicit_mapping_safe' {
    $f = New-Fixture 'prefix-both'; Add-Box $f 'Ali.json' 'legacy' 'Ali'; Add-Box $f 'MMOMail_Ali.json' 'legacy' 'MMOMail_Ali'; Write-Provenance $f
    Assert-Copied $f @('MMOMail_ali.json', 'MMOMail_mmomail_ali.json')
}
Run-Case 'spaces_sanitize_without_transliteration' {
    $f = New-Fixture 'spaces'; Add-Box $f 'A_B.json' 'legacy' 'A B'; Write-Provenance $f; Assert-Copied $f @('MMOMail_a_b.json')
}
Run-Case 'utf8_bom_preserved' {
    $f = New-Fixture 'utf8-bom'; Add-Box $f 'Ali.json'
    $path = Join-Path $f.Source 'Ali.json'
    [IO.File]::WriteAllBytes($path, ([byte[]]@(239, 187, 191) + [IO.File]::ReadAllBytes($path)))
    $f.Entries[0].sha256 = (Get-FileHash -LiteralPath $path).Hash
    Write-Provenance $f; Assert-Copied $f @('MMOMail_ali.json')
}
Run-Case 'twenty_records_boundary' {
    $f = New-Fixture 'twenty'; Add-Box $f 'Ali.json' 'legacy' 'Ali' (New-Box @(1..20 | ForEach-Object { New-Mail 'S' ('m' + $_) 1 })); Write-Provenance $f
    Assert-Copied $f @('MMOMail_ali.json')
}
Run-Case 'completion_receipt_deterministic' {
    $a = New-Fixture 'deterministic-a'; Add-Box $a 'Ali.json'; Write-Provenance $a; Assert-Copied $a @('MMOMail_ali.json')
    $b = New-Fixture 'deterministic-b'; Add-Box $b 'Ali.json'; Write-Provenance $b; Assert-Copied $b @('MMOMail_ali.json')
    Assert-True ((Get-FileHash -LiteralPath (Join-Path $a.Output 'EXPORT_COMPLETE.json')).Hash -ceq (Get-FileHash -LiteralPath (Join-Path $b.Output 'EXPORT_COMPLETE.json')).Hash) 'same snapshot/provenance produced different completion bytes'
}
Run-Case 'interrupted_export_no_complete_marker_and_no_reuse' {
    $f = New-Fixture 'interrupted'
    foreach ($index in 1..32) { Add-Box $f ('Owner' + $index + '.json') 'legacy' ('Owner' + $index) (New-Box @()) }
    Write-Provenance $f
    $r = Invoke-Export $f -Apply -InterruptAfterMarker
    Assert-True $r.Interrupted 'did not observe an interruption during staging creation'
    Assert-True ($r.Exit -ne 0) 'interrupted exporter returned success'
    Assert-True (Test-Path -LiteralPath (Join-Path $f.Output 'EXPORT_INCOMPLETE.json')) 'interrupted staging has no incomplete marker'
    Assert-True (-not (Test-Path -LiteralPath (Join-Path $f.Output 'EXPORT_COMPLETE.json'))) 'interrupted staging has completion marker'
    Assert-Rejected $f -ExistingOutput
}
Run-Case 'rerun_refused_after_simulated_collection' {
    $f = New-Fixture 'rerun'; Add-Box $f 'Ali.json'; Write-Provenance $f; Assert-Copied $f @('MMOMail_ali.json')
    Write-Text (Join-Path $f.Output 'MMOMail_ali.json') (New-Box @())
    Assert-Rejected $f -ExistingOutput
}
foreach ($kind in @('empty', 'partial', 'incomplete-marker', 'file')) {
    Run-Case ('existing_output_' + $kind) {
        $f = New-Fixture ('existing-' + $kind); Add-Box $f 'Ali.json'; Write-Provenance $f
        if ($kind -eq 'file') { Write-Text $f.Output 'occupied' } else {
            $null = [IO.Directory]::CreateDirectory($f.Output)
            if ($kind -eq 'partial') { Write-Text (Join-Path $f.Output 'MMOMail_ali.json') '{"version":1,"mails":[]}' }
            if ($kind -eq 'incomplete-marker') { Write-Text (Join-Path $f.Output 'EXPORT_INCOMPLETE.json') '{"state":"incomplete"}' }
        }
        Assert-Rejected $f -ExistingOutput
    }
}
foreach ($kind in @('both-paths', 'case-owner', 'space-alias', 'duplicate-entry', 'unmapped', 'sidecar-tmp', 'sidecar-bad', 'nested', 'ads')) {
    Run-Case ('inventory_' + $kind) {
        $f = New-Fixture ('inventory-' + $kind)
        if ($kind -ne 'space-alias') { Add-Box $f 'Ali.json' }
        switch ($kind) {
            'both-paths' { Add-Box $f 'MMOMail_ali.json' 'canonical' 'Ali' }
            'case-owner' { Add-Box $f 'MMOMail_ali.json' 'canonical' 'ali' }
            'space-alias' { Add-Box $f 'A_B.json' 'legacy' 'A B'; Add-Box $f 'MMOMail_a_b.json' 'canonical' 'A_B' }
            'duplicate-entry' { $f.Entries += $f.Entries[0] }
            'unmapped' { Write-Text (Join-Path $f.Source 'Bob.json') (New-Box) }
            'sidecar-tmp' { Write-Text (Join-Path $f.Source 'Ali.json.tmp') (New-Box) }
            'sidecar-bad' { Write-Text (Join-Path $f.Source 'Ali.json.bad') 'corrupt' }
            'nested' { $null = [IO.Directory]::CreateDirectory((Join-Path $f.Source 'nested')); Write-Text (Join-Path $f.Source 'nested/hidden.json') (New-Box) }
            'ads' { Set-Content -LiteralPath (Join-Path $f.Source 'Ali.json') -Stream sidecar -Value 'must not disappear' -Encoding UTF8 }
        }
        Write-Provenance $f; Assert-Rejected $f
    }
}
foreach ($kind in @('wrong-owner', 'wrong-format', 'wrong-hash', 'traversal', 'absolute', 'ads', 'unicode-owner', 'punctuation-owner', 'long-owner', 'empty-owner', 'version', 'missing-entry', 'tamper', 'reserved-owner', 'default-owner', 'format-case', 'filename-case', 'unicode-basename')) {
    Run-Case ('manifest_' + $kind) {
        $f = New-Fixture ('manifest-' + $kind)
        if ($kind -eq 'reserved-owner') { Add-Box $f 'MMOMail_con.json' 'canonical' 'con' }
        elseif ($kind -eq 'default-owner') { Add-Box $f 'MMOMail_default.json' 'canonical' 'Default' }
        else { Add-Box $f 'Ali.json' }
        switch ($kind) {
            'wrong-owner' { $f.Entries[0].owner = 'Bob' }
            'wrong-format' { $f.Entries[0].format = 'canonical' }
            'wrong-hash' { $f.Entries[0].sha256 = '0' * 64 }
            'traversal' { $f.Entries[0].filename = '../Ali.json' }
            'absolute' { $f.Entries[0].filename = Join-Path $f.Source 'Ali.json' }
            'ads' { $f.Entries[0].filename = 'Ali.json:hidden' }
            'unicode-owner' { $f.Entries[0].owner = 'Al' + [char]0x0131 }
            'punctuation-owner' { $f.Entries[0].owner = 'Ali!' }
            'long-owner' { $f.Entries[0].owner = 'A' * 33 }
            'empty-owner' { $f.Entries[0].owner = '' }
            'missing-entry' { $f.Entries = @() }
            'format-case' { $f.Entries[0].format = 'Legacy' }
            'filename-case' { $f.Entries[0].filename = 'ALI.json' }
            'unicode-basename' { $f.Entries[0].filename = 'Al' + [char]0x0131 + '.json' }
        }
        Write-Provenance $f
        if ($kind -eq 'version') { Save-Json $f.Manifest ([ordered]@{ version = 2; files = @($f.Entries) }) }
        if ($kind -eq 'tamper') { Write-Text (Join-Path $f.Source 'Ali.json') (New-Box @((New-Mail 'S' 'changed' 999))) }
        Assert-Rejected $f
    }
}
foreach ($kind in @('root-key', 'entry-key', 'case-key', 'trailing-json', 'comment', 'trailing-comma')) {
    Run-Case ('manifest_json_' + $kind) {
        $f = New-Fixture ('manifest-json-' + $kind); Add-Box $f 'Ali.json'; Write-Provenance $f
        $raw = [IO.File]::ReadAllText($f.Manifest)
        $original = $raw
        switch ($kind) {
            'root-key' { $raw = $raw -replace '("version"\s*:\s*1\b)', '$1, "version": 1' }
            'entry-key' { $raw = $raw -replace '("owner"\s*:\s*"Ali")', '$1, "owner": "Ali"' }
            'case-key' { $raw = $raw -replace '("owner"\s*:\s*"Ali")', '$1, "Owner": "Ali"' }
            'trailing-json' { $raw += '{}' }
            'comment' { $raw = '/*fixture*/' + $raw }
            'trailing-comma' { $raw = $raw.TrimEnd().Substring(0, $raw.TrimEnd().Length - 1) + ',}' }
        }
        Assert-True ($raw -cne $original) 'malicious JSON fixture mutation did not change input'
        Write-Text $f.Manifest $raw; Assert-Rejected $f
    }
}
$invalidBoxes = [ordered]@{
    corrupt = '{'
    missing_mails = '{"version":1}'
    wrong_version = '{"version":2,"mails":[]}'
    mails_object = '{"version":1,"mails":{}}'
    null_record = '{"version":1,"mails":[null]}'
    scalar_record = '{"version":1,"mails":[7]}'
    missing_from = '{"version":1,"mails":[{"text":"x","gold":1,"sent":1}]}'
    missing_text = '{"version":1,"mails":[{"from":"S","gold":1,"sent":1}]}'
    missing_gold = '{"version":1,"mails":[{"from":"S","text":"x","sent":1}]}'
    missing_sent = '{"version":1,"mails":[{"from":"S","text":"x","gold":1}]}'
    string_gold = '{"version":1,"mails":[{"from":"S","text":"x","gold":"1","sent":1}]}'
    boolean_gold = '{"version":1,"mails":[{"from":"S","text":"x","gold":true,"sent":1}]}'
    null_text = '{"version":1,"mails":[{"from":"S","text":null,"gold":1,"sent":1}]}'
    nul_sender = '{"version":1,"mails":[{"from":"S\u0000X","text":"x","gold":1,"sent":1}]}'
    nul_text = '{"version":1,"mails":[{"from":"S","text":"x\u0000y","gold":1,"sent":1}]}'
    duplicate_root = '{"version":1,"version":1,"mails":[]}'
    duplicate_record = '{"version":1,"mails":[{"from":"S","text":"x","gold":1,"gold":1,"sent":1}]}'
    duplicate_escaped = '{"version":1,"mails":[{"from":"S","text":"x","gold":1,"\u0067old":1,"sent":1}]}'
    duplicate_case = '{"version":1,"mails":[{"from":"S","text":"x","gold":1,"Gold":1,"sent":1}]}'
    empty_from = (New-Box @((New-Mail '' 'x' 1)))
    filtered_record = (New-Box @((New-Mail 'S' '' 0)))
    negative_gold = (New-Box @((New-Mail 'S' 'x' -1)))
    clamp_gold = (New-Box @((New-Mail 'S' 'x' 1000001)))
    fractional_gold = (New-Box @((New-Mail 'S' 'x' 1.5)))
    negative_sent = (New-Box @((New-Mail 'S' 'x' 1 -1)))
    fractional_sent = (New-Box @((New-Mail 'S' 'x' 1 1.5)))
    int64_overflow_sent = '{"version":1,"mails":[{"from":"S","text":"x","gold":1,"sent":9223372036854775808}]}'
    imprecise_double_sent = '{"version":1,"mails":[{"from":"S","text":"x","gold":1,"sent":9007199254740993}]}'
    unpaired_surrogate = '{"version":1,"mails":[{"from":"S","text":"\ud800","gold":1,"sent":1}]}'
    truncate_from = (New-Box @((New-Mail ('S' * 33) 'x' 1)))
    truncate_text = (New-Box @((New-Mail 'S' ('x' * 201) 1)))
    twenty_one_records = (New-Box @(1..21 | ForEach-Object { New-Mail 'S' 'x' 1 }))
    huge_string = (New-Box @((New-Mail 'S' ('x' * 1048577) 1)))
    deep_object = ('{"version":1,"mails":[],"deep":' + ('[' * 90) + '0' + (']' * 90) + '}')
    trailing_json = '{"version":1,"mails":[]}{}'
    comment = '/*fixture*/{"version":1,"mails":[]}'
    trailing_comma = '{"version":1,"mails":[],}'
}
foreach ($kind in $invalidBoxes.Keys) {
    Run-Case ('record_' + $kind) {
        $f = New-Fixture ('record-' + $kind); Add-Box $f 'Ali.json' 'legacy' 'Ali' $invalidBoxes[$kind]; Write-Provenance $f; Assert-Rejected $f
    }
}
foreach ($kind in @('invalid-utf8', 'utf16')) {
    Run-Case ('encoding_' + $kind) {
        $f = New-Fixture ('encoding-' + $kind); Add-Box $f 'Ali.json'
        $path = Join-Path $f.Source 'Ali.json'
        if ($kind -eq 'invalid-utf8') { [IO.File]::WriteAllBytes($path, [byte[]]@(255, 254, 255, 0)) }
        else { [IO.File]::WriteAllText($path, (New-Box), [Text.Encoding]::Unicode) }
        $f.Entries[0].sha256 = (Get-FileHash -LiteralPath $path).Hash
        Write-Provenance $f; Assert-Rejected $f
    }
}
foreach ($kind in @('same-source', 'inside-source', 'source-inside-output', 'inside-Saved', 'missing-parent', 'traversal')) {
    Run-Case ('path_' + $kind) {
        $f = New-Fixture ('path-' + $kind); Add-Box $f 'Ali.json'; Write-Provenance $f
        switch ($kind) {
            'same-source' { $f.Output = $f.Source }
            'inside-source' { $f.Output = Join-Path $f.Source 'staging' }
            'source-inside-output' { $f.Output = $f.Root }
            'inside-Saved' { $saved = Join-Path $f.Root 'Saved'; $null = [IO.Directory]::CreateDirectory($saved); $f.Output = Join-Path $saved 'staging' }
            'missing-parent' { $f.Output = Join-Path $f.Root 'missing/staging' }
            'traversal' { $f.Output = $f.Root + '\target\..\staging' }
        }
        Assert-Rejected $f
    }
}
foreach ($kind in @('overlap', 'Saved', 'checkout')) {
    Run-Case ('subst_alias_' + $kind) { Test-SubstFixture $kind }
}
# Reparse fixtures are retained. Inventory traversal above never follows a reparse target.
foreach ($kind in @('source-junction', 'output-parent-junction', 'source-file-symlink', 'manifest-symlink')) {
    $f = New-Fixture ('reparse-' + $kind); Add-Box $f 'Ali.json'; Write-Provenance $f
    $target = Join-Path $f.Root 'target'; $null = [IO.Directory]::CreateDirectory($target)
    Write-Text (Join-Path $target 'sentinel.txt') 'Do not change'
    $targetBefore = Get-Inventory $target
    $link = Join-Path $f.Root 'link'
    try {
        if ($kind -like '*junction') { $null = New-Item -ItemType Junction -Path $link -Target $target -ErrorAction Stop }
        elseif ($kind -eq 'source-file-symlink') {
            $link = Join-Path $f.Source 'linked.json'
            $null = New-Item -ItemType SymbolicLink -Path $link -Target (Join-Path $f.Source 'Ali.json') -ErrorAction Stop
        } else { $null = New-Item -ItemType SymbolicLink -Path $link -Target $f.Manifest -ErrorAction Stop }
    } catch { Skip-Case ('reparse_' + $kind) ('host cannot create link: ' + $_.Exception.Message); continue }
    Run-Case ('reparse_' + $kind) {
        if ($kind -eq 'source-junction') { $f.Source = $link }
        elseif ($kind -eq 'output-parent-junction') { $f.Output = Join-Path $link 'staging' }
        elseif ($kind -eq 'manifest-symlink') { $f.Manifest = $link }
        Assert-Rejected $f
        Assert-True ($targetBefore -ceq (Get-Inventory $target)) 'reparse target changed'
    }
}

Run-Case 'all_peer_files_preserved' {
    $peerAfter = Get-PeerInventory
    Write-Text (Join-Path $script:Proof 'peer-after.txt') $peerAfter
    Assert-True ($peerBefore -ceq $peerAfter) 'assigned worktree peer inventory changed during suite; inspect before/after'
}
$failed = @($script:Results | Where-Object { $_.status -eq 'FAIL' }).Count
$passed = @($script:Results | Where-Object { $_.status -eq 'PASS' }).Count
$skipped = @($script:Results | Where-Object { $_.status -eq 'SKIP' }).Count
$summary = [ordered]@{ host = $script:HostExe; version = $PSVersionTable.PSVersion.ToString(); runnerSha256 = $script:RunnerSha256; exporter = $ExporterPath; exporterSha256 = (Get-FileHash -LiteralPath $ExporterPath).Hash; passed = $passed; failed = $failed; skipped = $skipped; elapsedSeconds = $script:Watch.Elapsed.TotalSeconds; cases = @($script:Results.ToArray()) }
Save-Json (Join-Path $script:Proof 'results.json') $summary
Write-Output ('[MMO_MAIL_EXPORT_QA] passed=' + $passed + ' failed=' + $failed + ' skipped=' + $skipped + ' proof=' + $script:Proof)
if ($failed -gt 0) { exit 1 }
exit 0
