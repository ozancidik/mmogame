#Requires -Version 5.1
<#
.SYNOPSIS
Validates an operator-mapped frozen mailbox snapshot and optionally exports staging bytes.
.DESCRIPTION
Offline preparation only. Never reads the live Saved tree, changes the source, or activates
mailboxes. See docs/tasks/MAIL_COMPATIBILITY_2026-10-11.md for the provenance contract.
#>
[CmdletBinding(SupportsShouldProcess = $true, ConfirmImpact = 'Medium')]
param(
    [Parameter(Mandatory)][string]$SourceDirectory,
    [Parameter(Mandatory)][string]$ProvenanceManifest,
    [Parameter(Mandatory)][string]$StagingDirectory,
    [Parameter(Mandatory)][switch]$AcknowledgeFrozenSnapshot,
    [switch]$Apply
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$ordinal = [StringComparison]::Ordinal
$ignoreCase = [StringComparison]::OrdinalIgnoreCase
$utf8 = New-Object System.Text.UTF8Encoding($false, $true)

function Assert-SafeLeaf {
    param([string]$Name)
    if ([string]::IsNullOrEmpty($Name) -or $Name -in @('.', '..') -or
        $Name -match '[\x00-\x1f<>:"/\\|?*\[\]~]' -or $Name -match '[. ]$' -or
        $Name -match '^(?i:CON|PRN|AUX|NUL|CLOCK\$|CONIN\$|CONOUT\$|COM[0-9\u00b9\u00b2\u00b3]|LPT[0-9\u00b9\u00b2\u00b3])(?:\.|$)') {
        throw "Unsafe/reserved path component: $Name"
    }
}

function Get-SafePath {
    param([string]$Path)
    # Deliberately Windows local-drive only. Reject traversal before normalization.
    if ($Path -notmatch '^[A-Za-z]:[\\/]' -or $Path -match '[\x00-\x1f<>"|?*]' -or
        $Path.Substring(2).Contains(':')) { throw "Expected a local absolute drive path: $Path" }
    $parts = $Path.Substring(3).Replace('/', '\').Split('\')
    foreach ($part in $parts) { Assert-SafeLeaf $part }
    $full = [IO.Path]::GetFullPath($Path)
    $cursor = $full
    while ($cursor) {
        if (Test-Path -LiteralPath $cursor) {
            $item = Get-Item -LiteralPath $cursor -Force
            if (($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
                throw "Reparse point/ancestor is forbidden: $cursor"
            }
        }
        $cursor = [IO.Path]::GetDirectoryName($cursor)
    }
    return $full
}

function Test-Within {
    param([string]$Path, [string]$Parent)
    return $Path.Equals($Parent, $ignoreCase) -or $Path.StartsWith($Parent + '\', $ignoreCase)
}

function Get-NativePathApi {
    # Reflection.Emit keeps P/Invoke declarations in memory on BOTH PS 5.1 and 7;
    # unlike Add-Type's desktop compiler, it creates no temporary source/DLL files.
    $native = 'MMOMailExport.NativePathsV1' -as [type]
    if ($null -ne $native) { return $native }
    $assemblyName = New-Object Reflection.AssemblyName('MMOMailExport.NativePathsV1')
    if ($PSVersionTable.PSEdition -eq 'Desktop') {
        $assembly = [AppDomain]::CurrentDomain.DefineDynamicAssembly($assemblyName, [Reflection.Emit.AssemblyBuilderAccess]::Run)
    } else {
        $assembly = [Reflection.Emit.AssemblyBuilder]::DefineDynamicAssembly($assemblyName, [Reflection.Emit.AssemblyBuilderAccess]::Run)
    }
    $module = $assembly.DefineDynamicModule('NativePaths')
    $typeBuilder = $module.DefineType('MMOMailExport.NativePathsV1', [Reflection.TypeAttributes]::Public)
    $methods = @(
        @{ Name = 'CreateFileW'; Return = [IntPtr]; Args = [type[]]@([string], [uint32], [uint32], [IntPtr], [uint32], [uint32], [IntPtr]) },
        @{ Name = 'GetFinalPathNameByHandleW'; Return = [uint32]; Args = [type[]]@([IntPtr], [Text.StringBuilder], [uint32], [uint32]) },
        @{ Name = 'CloseHandle'; Return = [bool]; Args = [type[]]@([IntPtr]) }
    )
    foreach ($spec in $methods) {
        $method = $typeBuilder.DefinePInvokeMethod($spec.Name, 'kernel32.dll', $spec.Name,
            ([Reflection.MethodAttributes]::Public -bor [Reflection.MethodAttributes]::Static -bor [Reflection.MethodAttributes]::PinvokeImpl),
            [Reflection.CallingConventions]::Standard, $spec.Return, $spec.Args,
            [Runtime.InteropServices.CallingConvention]::Winapi, [Runtime.InteropServices.CharSet]::Unicode)
        $method.SetImplementationFlags([Reflection.MethodImplAttributes]::PreserveSig)
    }
    return $typeBuilder.CreateType()
}

function Get-PhysicalExistingPath {
    param([string]$Path)
    $native = Get-NativePathApi
    # Metadata access, share read/write/delete, OPEN_EXISTING, BACKUP_SEMANTICS
    # permits directory handles. This never creates or modifies filesystem data.
    $handle = $native::CreateFileW($Path, 0, 7, [IntPtr]::Zero, 3, 0x02000000, [IntPtr]::Zero)
    if ($handle -eq [IntPtr]::Zero -or $handle -eq [IntPtr](-1)) { throw "Cannot open path for physical identity: $Path" }
    try {
        $buffer = New-Object Text.StringBuilder(1024)
        # FILE_NAME_NORMALIZED | VOLUME_NAME_GUID. SUBST/mapped drive spellings
        # cannot be used as containment identities; unresolved aliases fail closed.
        $length = $native::GetFinalPathNameByHandleW($handle, $buffer, $buffer.Capacity, 1)
        if ($length -ge $buffer.Capacity -and $length -lt 32768) {
            $buffer = New-Object Text.StringBuilder([int]($length + 1))
            $length = $native::GetFinalPathNameByHandleW($handle, $buffer, $buffer.Capacity, 1)
        }
        if ($length -eq 0 -or $length -ge $buffer.Capacity) { throw "Cannot resolve physical volume path: $Path" }
        $physical = $buffer.ToString().TrimEnd('\')
        if ($physical -notmatch '\A\\\\\?\\Volume\{[0-9a-fA-F-]{36}\}\\' -or
            $physical -match '(?:^|\\)\.\.?(?:\\|$)') { throw "Unsupported/unresolved physical path identity: $Path" }
        return $physical
    } finally { $null = $native::CloseHandle($handle) }
}

function Assert-PhysicalBoundaries {
    param([string]$Source, [string]$StageParent, [string]$StageLeaf, [string]$Manifest)
    $physicalSource = Get-PhysicalExistingPath $Source
    $physicalParent = Get-PhysicalExistingPath $StageParent
    $physicalStage = $physicalParent + '\' + $StageLeaf
    $physicalManifest = Get-PhysicalExistingPath $Manifest
    $physicalRepo = Get-PhysicalExistingPath (Split-Path -Parent $PSScriptRoot)
    foreach ($path in @($physicalSource, $physicalStage, $physicalManifest)) {
        if ($path -match '(?i)(?:^|\\)Saved(?:\\|$)' -or
            (Test-Within $path $physicalRepo) -or (Test-Within $physicalRepo $path)) {
            throw 'Physical path resolves into Saved/checkout or contains the checkout.'
        }
    }
    if ((Test-Within $physicalStage $physicalSource) -or (Test-Within $physicalSource $physicalStage) -or
        (Test-Within $physicalManifest $physicalSource) -or (Test-Within $physicalManifest $physicalStage)) {
        throw 'Physical source, staging and provenance paths overlap (including drive aliases).'
    }
    return @{ Source = $physicalSource; Parent = $physicalParent; Manifest = $physicalManifest }
}

function Assert-OfflinePath {
    param([string]$Path)
    if ($Path -match '(?i)(?:^|[\\/])Saved(?:[\\/]|$)') {
        throw "Saved paths are forbidden; use an external frozen snapshot/staging path: $Path"
    }
    $repo = [IO.Path]::GetFullPath((Split-Path -Parent $PSScriptRoot))
    if ((Test-Within $Path $repo) -or (Test-Within $repo $Path)) {
        throw "Snapshot/staging must be external to this checkout: $Path"
    }
}

function Read-LockedBytes {
    param([IO.FileStream]$Stream, [long]$Limit)
    if ($Stream.Length -gt $Limit) { throw "Input exceeds supported byte limit ($Limit)." }
    $Stream.Position = 0
    $memory = New-Object IO.MemoryStream
    try {
        $Stream.CopyTo($memory)
        if ($memory.Length -gt $Limit) { throw 'Input grew beyond its byte limit.' }
        return ,$memory.ToArray()
    } finally { $memory.Dispose() }
}

function Get-ByteHash {
    param([byte[]]$Bytes)
    $sha = [Security.Cryptography.SHA256]::Create()
    try { return ([BitConverter]::ToString($sha.ComputeHash($Bytes))).Replace('-', '').ToLowerInvariant() }
    finally { $sha.Dispose() }
}

function Convert-StrictUtf8 {
    param([byte[]]$Bytes)
    $offset = 0
    if ($Bytes.Length -ge 3 -and $Bytes[0] -eq 239 -and $Bytes[1] -eq 187 -and $Bytes[2] -eq 191) { $offset = 3 }
    return $utf8.GetString($Bytes, $offset, $Bytes.Length - $offset)
}

# Small bounded recursive-descent JSON reader. Nodes retain number tokens instead of
# coercing through double. No ConvertFrom-Json duplicate-key overwrite or type defaults.
function Read-StrictJson {
    param([string]$Text)
    $reader = @{ Text = $Text; Position = 0; Nodes = 0 }
    $node = Read-JsonNode $reader 0
    Skip-JsonSpace $reader
    if ($reader.Position -ne $Text.Length) { throw 'Trailing JSON content.' }
    return $node
}

function Skip-JsonSpace {
    param([hashtable]$Reader)
    while ($Reader.Position -lt $Reader.Text.Length -and
        [int][char]$Reader.Text[$Reader.Position] -in @(32, 9, 10, 13)) { $Reader.Position++ }
}

function Read-JsonString {
    param([hashtable]$Reader)
    if ($Reader.Position -ge $Reader.Text.Length -or $Reader.Text[$Reader.Position] -ne '"') { throw 'Expected JSON string.' }
    $Reader.Position++
    $builder = New-Object Text.StringBuilder
    $closed = $false
    while ($Reader.Position -lt $Reader.Text.Length) {
        $ch = $Reader.Text[$Reader.Position++]
        if ($ch -eq '"') { $closed = $true; break }
        if ([int][char]$ch -lt 32) { throw 'Unescaped JSON control character.' }
        if ($ch -eq '\') {
            if ($Reader.Position -ge $Reader.Text.Length) { throw 'Incomplete JSON escape.' }
            $escape = $Reader.Text[$Reader.Position++]
            switch -CaseSensitive ($escape) {
                '"' { $ch = '"' }
                '\' { $ch = '\' }
                '/' { $ch = '/' }
                'b' { $ch = [char]8 }
                'f' { $ch = [char]12 }
                'n' { $ch = [char]10 }
                'r' { $ch = [char]13 }
                't' { $ch = [char]9 }
                'u' {
                    if ($Reader.Position + 4 -gt $Reader.Text.Length) { throw 'Incomplete unicode escape.' }
                    $hex = $Reader.Text.Substring($Reader.Position, 4)
                    if ($hex -cnotmatch '\A[0-9a-fA-F]{4}\z') { throw 'Invalid unicode escape.' }
                    $ch = [char][Convert]::ToInt32($hex, 16)
                    $Reader.Position += 4
                }
                default { throw 'Invalid JSON escape.' }
            }
        }
        [void]$builder.Append($ch)
    }
    if (-not $closed) { throw 'Unterminated JSON string.' }
    $value = $builder.ToString()
    for ($i = 0; $i -lt $value.Length; $i++) {
        if ([char]::IsHighSurrogate($value[$i])) {
            if ($i + 1 -ge $value.Length -or -not [char]::IsLowSurrogate($value[$i + 1])) { throw 'Unpaired JSON surrogate.' }
            $i++
        } elseif ([char]::IsLowSurrogate($value[$i])) { throw 'Unpaired JSON surrogate.' }
    }
    return $value
}

function Read-JsonNode {
    param([hashtable]$Reader, [int]$Depth)
    $Reader.Nodes++
    if ($Depth -gt 8 -or $Reader.Nodes -gt 50000) { throw 'JSON depth/node bound exceeded.' }
    Skip-JsonSpace $Reader
    if ($Reader.Position -ge $Reader.Text.Length) { throw 'Unexpected end of JSON.' }
    $ch = $Reader.Text[$Reader.Position]
    if ($ch -eq '"') { return @{ Kind = 'string'; Value = (Read-JsonString $Reader) } }
    if ($ch -eq '{' -or $ch -eq '[') {
        $isObject = $ch -eq '{'
        $end = ']'
        $values = New-Object 'System.Collections.Generic.List[object]'
        $members = New-Object 'System.Collections.Generic.Dictionary[string,object]' ([StringComparer]::Ordinal)
        if ($isObject) { $end = '}' }
        $Reader.Position++
        Skip-JsonSpace $Reader
        if ($Reader.Position -lt $Reader.Text.Length -and $Reader.Text[$Reader.Position] -eq $end) {
            $Reader.Position++
        } else {
            while ($true) {
                if ($isObject) {
                    Skip-JsonSpace $Reader
                    $key = Read-JsonString $Reader
                    if ($members.ContainsKey($key)) { throw "Duplicate JSON key: $key" }
                    Skip-JsonSpace $Reader
                    if ($Reader.Position -ge $Reader.Text.Length -or $Reader.Text[$Reader.Position++] -ne ':') { throw 'Expected JSON colon.' }
                    $members.Add($key, (Read-JsonNode $Reader ($Depth + 1)))
                } else { $values.Add((Read-JsonNode $Reader ($Depth + 1))) }
                Skip-JsonSpace $Reader
                if ($Reader.Position -ge $Reader.Text.Length) { throw 'Unterminated JSON container.' }
                $delimiter = $Reader.Text[$Reader.Position++]
                if ($delimiter -eq $end) { break }
                if ($delimiter -ne ',') { throw 'Expected JSON comma.' }
            }
        }
        if ($isObject) { return @{ Kind = 'object'; Value = $members } }
        return @{ Kind = 'array'; Value = $values }
    }
    # Number/literal tokenization only; object/array/string grammar is parsed above.
    $match = [regex]::Match($Reader.Text.Substring($Reader.Position), '\A(?:-?(?:0|[1-9][0-9]*)(?:\.[0-9]+)?(?:[eE][+-]?[0-9]+)?|true|false|null)',
        [Text.RegularExpressions.RegexOptions]::CultureInvariant, [TimeSpan]::FromSeconds(2))
    if (-not $match.Success) { throw 'Invalid JSON value.' }
    $Reader.Position += $match.Length
    $kind = 'number'
    if ($match.Value -in @('true', 'false', 'null')) { $kind = 'literal' }
    return @{ Kind = $kind; Value = $match.Value }
}

function Assert-ObjectFields {
    param([hashtable]$Node, [string[]]$Fields)
    if ($Node.Kind -cne 'object' -or $Node.Value.Count -ne $Fields.Count) { throw 'Unexpected JSON object shape.' }
    foreach ($field in $Fields) { if (-not $Node.Value.ContainsKey($field)) { throw "Missing/case-mismatched JSON field: $field" } }
}

function Get-JsonString {
    param([hashtable]$Node)
    if ($Node.Kind -cne 'string') { throw 'Expected typed JSON string.' }
    return [string]$Node.Value
}

function Get-JsonInteger {
    param([hashtable]$Node, [long]$Maximum)
    # Narrow integer-token scope prevents fractional/defaulted/clamped runtime values.
    if ($Node.Kind -cne 'number' -or $Node.Value -cnotmatch '\A(?:0|[1-9][0-9]*)\z') { throw 'Expected nonnegative JSON integer token.' }
    [long]$value = 0
    if (-not [long]::TryParse($Node.Value, [Globalization.NumberStyles]::None,
        [Globalization.CultureInfo]::InvariantCulture, [ref]$value) -or $value -gt $Maximum) { throw "JSON integer outside 0..$Maximum." }
    return $value
}

function Get-OwnerKey {
    param([string]$Owner)
    if ($Owner -cnotmatch '\A[A-Za-z0-9 _-]{1,32}\z' -or $Owner -cne $Owner.Trim()) {
        throw "Unsupported/ambiguous owner (ASCII only, no outer spaces, max 32): $Owner"
    }
    $safe = $Owner.Replace(' ', '_')
    if ($safe.Equals('Default', $ignoreCase)) { throw 'Default and its case variants are unsupported owners.' }
    Assert-SafeLeaf $safe
    return $safe.ToLowerInvariant()
}

function Get-MailSummary {
    param([byte[]]$Bytes)
    $root = Read-StrictJson (Convert-StrictUtf8 $Bytes)
    Assert-ObjectFields $root @('version', 'mails')
    if ((Get-JsonInteger $root.Value['version'] 1) -ne 1) { throw 'Unsupported mailbox version.' }
    $mails = $root.Value['mails']
    if ($mails.Kind -cne 'array' -or $mails.Value.Count -gt 20) { throw 'Mailbox must be an array of at most 20 records.' }
    [long]$goldTotal = 0
    foreach ($mail in $mails.Value) {
        Assert-ObjectFields $mail @('from', 'text', 'gold', 'sent')
        $from = Get-JsonString $mail.Value['from']
        $body = Get-JsonString $mail.Value['text']
        $gold = Get-JsonInteger $mail.Value['gold'] 1000000
        $null = Get-JsonInteger $mail.Value['sent'] 9007199254740991
        if ($from.Length -lt 1 -or $from.Length -gt 32 -or $body.Length -gt 200 -or
            $from.Contains([string][char]0) -or $body.Contains([string][char]0) -or
            ($body.Length -eq 0 -and $gold -eq 0)) { throw 'Mail would be filtered/truncated by runtime loading.' }
        $goldTotal += $gold
    }
    return @{ MailCount = $mails.Value.Count; GoldTotal = $goldTotal }
}

function Get-SnapshotInventory {
    param([string]$Directory)
    $null = Get-SafePath $Directory
    $names = New-Object 'System.Collections.Generic.List[string]'
    $seen = New-Object 'System.Collections.Generic.HashSet[string]' ([StringComparer]::OrdinalIgnoreCase)
    foreach ($item in @(Get-ChildItem -LiteralPath $Directory -Force)) {
        if ($item.PSIsContainer -or ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) { throw 'Snapshot must contain only regular top-level mailbox files.' }
        if ($item.Name -cnotmatch '\A[A-Za-z0-9_-]+\.json\z') { throw "Unknown file/sidecar or unsupported basename: $($item.Name)" }
        Assert-SafeLeaf $item.Name
        if (-not $seen.Add($item.Name)) { throw 'Case-colliding snapshot filenames.' }
        # Alternate data streams are unmapped data; do not silently drop them.
        $streams = @(Get-Item -LiteralPath $item.FullName -Stream * -ErrorAction Stop)
        if (@($streams | Where-Object { $_.Stream -ne ':$DATA' }).Count -gt 0) { throw "Alternate data streams are unsupported: $($item.Name)" }
        $names.Add($item.Name)
    }
    $names.Sort([StringComparer]::Ordinal)
    return ,$names.ToArray()
}

function Assert-Inventory {
    param([string[]]$Expected, [string]$Directory)
    $actual = Get-SnapshotInventory $Directory
    if ($actual.Count -ne $Expected.Count) { throw 'Snapshot inventory changed.' }
    for ($i = 0; $i -lt $actual.Count; $i++) {
        if (-not $actual[$i].Equals($Expected[$i], $ordinal)) { throw 'Snapshot inventory changed.' }
    }
}

function Write-NewBytes {
    param([string]$Path, [byte[]]$Bytes)
    $null = Get-SafePath $Path
    $stream = [IO.File]::Open($Path, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write, [IO.FileShare]::None)
    try { $stream.Write($Bytes, 0, $Bytes.Length); $stream.Flush($true) }
    finally { $stream.Dispose() }
}

function Get-ReceiptBytes {
    param([object]$Receipt)
    # Ordered properties, ordinal file order, LF and UTF-8 without BOM across PS 5.1/7.
    $json = ($Receipt | ConvertTo-Json -Depth 8 -Compress).Replace("`r`n", "`n") + "`n"
    return ,$utf8.GetBytes($json)
}

$handles = New-Object 'System.Collections.Generic.List[System.IO.FileStream]'
$stageCreated = $false
try {
    if (-not $AcknowledgeFrozenSnapshot) { throw 'Explicit -AcknowledgeFrozenSnapshot is required (source frozen externally; never live Saved).' }
    $source = Get-SafePath $SourceDirectory
    $manifestPath = Get-SafePath $ProvenanceManifest
    $stage = Get-SafePath $StagingDirectory
    Assert-OfflinePath $source
    Assert-OfflinePath $stage
    Assert-OfflinePath $manifestPath
    if ((Test-Within $stage $source) -or (Test-Within $source $stage) -or
        (Test-Within $manifestPath $source) -or (Test-Within $manifestPath $stage)) { throw 'Source, staging and provenance paths overlap.' }
    if (-not (Test-Path -LiteralPath $source -PathType Container)) { throw 'Snapshot directory does not exist.' }
    if (-not (Test-Path -LiteralPath $manifestPath -PathType Leaf)) { throw 'Provenance manifest is not a regular file.' }
    if (Test-Path -LiteralPath $stage) { throw 'Staging already exists (including partial staging); choose a new absent directory.' }
    $stageParent = Split-Path -Parent $stage
    if (-not (Test-Path -LiteralPath $stageParent -PathType Container)) { throw 'Staging parent must already exist.' }
    $stageLeaf = Split-Path -Leaf $stage
    $physicalPaths = Assert-PhysicalBoundaries $source $stageParent $stageLeaf $manifestPath

    # Diagnostic only: external snapshot freeze/ownership cannot be proven by process names.
    $runtimeNames = @(Get-Process -ErrorAction Stop | Where-Object {
        $_.ProcessName -match '^(?:UnrealEditor(?:-Cmd)?|MyProject(?:Server)?(?:-Win64-(?:Development|Shipping|Test))?)$'
    } | ForEach-Object { $_.ProcessName } | Sort-Object -Unique)
    if ($runtimeNames.Count -gt 0) { Write-Warning "Runtime processes detected: $($runtimeNames -join ', '). Only the externally frozen snapshot is eligible; no process is stopped." }

    $manifestStream = [IO.File]::Open($manifestPath, 'Open', 'Read', 'Read')
    $handles.Add($manifestStream)
    $manifestBytes = Read-LockedBytes $manifestStream 4194304
    $manifestHash = Get-ByteHash $manifestBytes
    $manifest = Read-StrictJson (Convert-StrictUtf8 $manifestBytes)
    Assert-ObjectFields $manifest @('version', 'files')
    if ((Get-JsonInteger $manifest.Value['version'] 1) -ne 1 -or $manifest.Value['files'].Kind -cne 'array') { throw 'Unsupported provenance manifest schema.' }
    if ($manifest.Value['files'].Value.Count -gt 10000) { throw 'Snapshot exceeds supported 10000-file bound.' }
    $inventory = Get-SnapshotInventory $source
    $mapping = New-Object 'System.Collections.Generic.Dictionary[string,object]' ([StringComparer]::OrdinalIgnoreCase)
    $owners = New-Object 'System.Collections.Generic.HashSet[string]' ([StringComparer]::OrdinalIgnoreCase)
    $outputs = New-Object 'System.Collections.Generic.HashSet[string]' ([StringComparer]::OrdinalIgnoreCase)
    foreach ($entry in $manifest.Value['files'].Value) {
        Assert-ObjectFields $entry @('filename', 'format', 'owner', 'sha256')
        $name = Get-JsonString $entry.Value['filename']
        $format = Get-JsonString $entry.Value['format']
        $owner = Get-JsonString $entry.Value['owner']
        $hash = Get-JsonString $entry.Value['sha256']
        if ($name -cnotmatch '\A[A-Za-z0-9_-]+\.json\z' -or $hash -cnotmatch '\A[0-9a-fA-F]{64}\z' -or
            $format -cnotin @('legacy', 'canonical')) { throw 'Invalid provenance filename/format/SHA256.' }
        Assert-SafeLeaf $name
        $key = Get-OwnerKey $owner
        $target = 'MMOMail_' + $key + '.json'
        $oldOriginal = $owner.Replace(' ', '_') + '.json'
        $oldLower = $key + '.json'
        if (($format -ceq 'canonical' -and -not $name.Equals($target, $ordinal)) -or
            ($format -ceq 'legacy' -and -not $name.Equals($oldOriginal, $ordinal) -and -not $name.Equals($oldLower, $ordinal))) {
            throw "Provenance format/owner does not match an exact supported historical filename: $name"
        }
        if ($mapping.ContainsKey($name) -or -not $owners.Add($key) -or -not $outputs.Add($target)) { throw 'Duplicate file/owner, alias/case collision, both paths, or output collision.' }
        $mapping.Add($name, @{ Filename = $name; Format = $format; Owner = $owner; Target = $target; Hash = $hash.ToLowerInvariant() })
    }
    if ($mapping.Count -ne $inventory.Count) { throw 'Provenance must map every snapshot file exactly once.' }
    $validated = New-Object 'System.Collections.Generic.Dictionary[string,object]' ([StringComparer]::Ordinal)
    foreach ($name in $inventory) {
        if (-not $mapping.ContainsKey($name) -or -not $mapping[$name].Filename.Equals($name, $ordinal)) { throw "Unmapped/case-mismatched snapshot file: $name" }
        $entry = $mapping[$name]
        $filePath = Get-SafePath (Join-Path $source $name)
        $stream = [IO.File]::Open($filePath, 'Open', 'Read', 'Read')
        $handles.Add($stream)
        $bytes = Read-LockedBytes $stream 1048576
        if ((Get-ByteHash $bytes) -cne $entry.Hash) { throw "Source SHA256 mismatch: $name" }
        $summary = Get-MailSummary $bytes
        $entry.Stream = $stream
        $entry.ByteLength = $bytes.LongLength
        $entry.MailCount = $summary.MailCount
        $entry.GoldTotal = $summary.GoldTotal
        $validated.Add($entry.Target, $entry)
    }
    Assert-Inventory $inventory $source
    $targets = [string[]]@($validated.Keys)
    [Array]::Sort($targets, [StringComparer]::Ordinal)
    $files = New-Object 'System.Collections.Generic.List[object]'
    [long]$totalGold = 0
    [long]$totalMails = 0
    foreach ($target in $targets) {
        $entry = $validated[$target]
        $files.Add([ordered]@{ filename = $entry.Filename; format = $entry.Format; owner = $entry.Owner;
            output = $target; sourceSha256 = $entry.Hash; outputSha256 = $entry.Hash;
            byteLength = $entry.ByteLength; mailCount = $entry.MailCount; goldTotal = $entry.GoldTotal })
        $totalGold += $entry.GoldTotal
        $totalMails += $entry.MailCount
    }
    $receipt = [ordered]@{ version = 1; purpose = 'offline-mailbox-export-staging-only'; complete = $false;
        activatable = $false; provenanceSha256 = $manifestHash; mailboxCount = $files.Count;
        mailCount = $totalMails; goldTotal = $totalGold; files = @($files.ToArray()) }
    if (-not $Apply) {
        Write-Output ([pscustomobject]@{ Status = 'ValidatedPlan'; StagingDirectory = $stage; Receipt = $receipt })
        return
    }
    if (-not $PSCmdlet.ShouldProcess($stage, 'Create NEW offline staging with verified mailbox copies (no activation)')) {
        Write-Output ([pscustomobject]@{ Status = 'ValidatedNoWrite'; StagingDirectory = $stage; Receipt = $receipt })
        return
    }
    $null = Get-SafePath $source
    $null = Get-SafePath $stage
    if (Test-Path -LiteralPath $stage) { throw 'Staging appeared during validation; refusing reuse.' }
    $currentPhysical = Assert-PhysicalBoundaries $source $stageParent $stageLeaf $manifestPath
    foreach ($part in @('Source', 'Parent', 'Manifest')) {
        if (-not $currentPhysical[$part].Equals($physicalPaths[$part], $ignoreCase)) { throw 'Physical path identity changed during validation.' }
    }
    Assert-Inventory $inventory $source
    # New-Item without Force fails if a competing invocation created the directory.
    $null = New-Item -ItemType Directory -Path $stage -ErrorAction Stop
    $stageCreated = $true
    Write-NewBytes (Join-Path $stage 'EXPORT_INCOMPLETE.json') (Get-ReceiptBytes $receipt)
    foreach ($target in $targets) {
        $entry = $validated[$target]
        $null = Get-SafePath (Join-Path $source $entry.Filename)
        $bytes = Read-LockedBytes $entry.Stream 1048576
        if ((Get-ByteHash $bytes) -cne $entry.Hash) { throw "Source changed before copy: $($entry.Filename)" }
        $destination = Join-Path $stage $target
        Write-NewBytes $destination $bytes
        if ((Get-ByteHash ([IO.File]::ReadAllBytes($destination))) -cne $entry.Hash -or
            (Get-ByteHash (Read-LockedBytes $entry.Stream 1048576)) -cne $entry.Hash) { throw "Copy/source verification failed: $target" }
    }
    Assert-Inventory $inventory $source
    if ((Get-ByteHash (Read-LockedBytes $manifestStream 4194304)) -cne $manifestHash) { throw 'Provenance changed.' }
    foreach ($target in $targets) {
        $entry = $validated[$target]
        $destination = Get-SafePath (Join-Path $stage $target)
        if ((Get-ByteHash ([IO.File]::ReadAllBytes($destination))) -cne $entry.Hash -or
            (Get-ByteHash (Read-LockedBytes $entry.Stream 1048576)) -cne $entry.Hash) { throw 'Final whole-export hash verification failed.' }
    }
    $stagedNames = @(Get-ChildItem -LiteralPath $stage -Force)
    if ($stagedNames.Count -ne $targets.Count + 1 -or @($stagedNames | Where-Object {
        $_.PSIsContainer -or ($_.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0 -or
        ($_.Name -cne 'EXPORT_INCOMPLETE.json' -and -not $validated.ContainsKey($_.Name))
    }).Count -gt 0) { throw 'Unexpected staging inventory.' }
    $receipt.complete = $true
    $pending = Join-Path $stage 'EXPORT_COMPLETE.json.tmp'
    $complete = Join-Path $stage 'EXPORT_COMPLETE.json'
    $receiptBytes = Get-ReceiptBytes $receipt
    Write-NewBytes $pending $receiptBytes
    if ((Get-ByteHash ([IO.File]::ReadAllBytes($pending))) -cne (Get-ByteHash $receiptBytes)) { throw 'Completion receipt verification failed.' }
    $null = Get-SafePath $stage
    [IO.File]::Move($pending, $complete)
    Write-Output ([pscustomobject]@{ Status = 'ExportedStagingOnly'; StagingDirectory = $stage; Receipt = $receipt })
} catch {
    if ($stageCreated) { Write-Warning "FAILED export: leave partial staging for inspection; never activate/reuse/delete automatically: $StagingDirectory" }
    throw
} finally {
    foreach ($handle in $handles) { $handle.Dispose() }
}
