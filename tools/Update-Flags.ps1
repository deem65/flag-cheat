#Requires -Version 5.1
<#
Downloads a complete Flagpedia snapshot before switching the catalog.
The previous assets directory is preserved as assets-backup-<unique id>.
This script updates source assets only; rebuild drek_flag_cheat afterward.
#>
[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12

$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$runId = [Guid]::NewGuid().ToString('N')
$stagingRoot = Join-Path $projectRoot ('.flags-stage-' + $runId)
$stagedAssets = Join-Path $stagingRoot 'assets'
$stagedFlags = Join-Path $stagedAssets 'flags'
$currentAssets = Join-Path $projectRoot 'assets'
$backupAssets = Join-Path $projectRoot ('assets-backup-' + $runId)
$utf8 = [Text.UTF8Encoding]::new($false)

function ConvertTo-LuaString([string]$Value) {
    if ($Value -match '[\x00-\x1f]') {
        throw 'A country name contains an unexpected control character.'
    }
    return '"' + $Value.Replace('\', '\\').Replace('"', '\"') + '"'
}

New-Item -ItemType Directory -Path $stagedFlags -Force | Out-Null
try {
    $response = Invoke-RestMethod -Uri 'https://flagcdn.com/en/codes.json' -TimeoutSec 30
    $entries = @($response.PSObject.Properties | Where-Object {
        ($_.Name -match '^[a-z]{2}$' -or $_.Name -match '^gb-(eng|sct|wls)$') -and
        $_.Name -notin @('eu', 'un')
    } | Sort-Object Name)
    if ($entries.Count -lt 240) {
        throw 'The country catalog is unexpectedly small; the existing catalog was not changed.'
    }

    $names = [ordered]@{}
    $hashes = [ordered]@{}
    $luaLines = New-Object 'System.Collections.Generic.List[string]'
    $luaLines.Add('-- Generated from https://flagcdn.com/en/codes.json. Lowercase UTF-8 names.')
    $luaLines.Add('return {')
    $index = 0
    foreach ($entry in $entries) {
        $code = [string]$entry.Name
        $name = [string]$entry.Value
        if ([string]::IsNullOrWhiteSpace($name)) { throw "Missing country name for $code." }
        $filename = $code + '.png'
        $destination = Join-Path $stagedFlags $filename
        Write-Progress -Activity 'Downloading Flagpedia references' -Status $name `
            -PercentComplete ([int](100 * $index / $entries.Count))
        Invoke-WebRequest -UseBasicParsing -Uri "https://flagcdn.com/w640/$filename" `
            -OutFile $destination -TimeoutSec 30
        $bytes = [IO.File]::ReadAllBytes($destination)
        if ($bytes.Length -lt 24 -or [BitConverter]::ToString($bytes, 0, 8) -ne '89-50-4E-47-0D-0A-1A-0A') {
            throw "The response for $code is not a PNG image."
        }
        $names[$code] = $name
        $hashes[$filename] = (Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash.ToLowerInvariant()
        $line = '    { code = ' + (ConvertTo-LuaString $code) +
            ', name = ' + (ConvertTo-LuaString ($name.ToLowerInvariant())) +
            ', file = ' + (ConvertTo-LuaString $filename) + ' },'
        $luaLines.Add($line)
        $index++
    }
    $luaLines.Add('}')
    [IO.File]::WriteAllText((Join-Path $stagedAssets 'flags.lua'), ($luaLines -join "`n") + "`n", $utf8)
    [IO.File]::WriteAllText((Join-Path $stagedAssets 'codes.json'), ($names | ConvertTo-Json -Depth 3) + "`n", $utf8)
    $provenance = [ordered]@{
        provider = 'Flagpedia.net / flagcdn.com'
        documentation = 'https://flagpedia.net/download/api'
        names_url = 'https://flagcdn.com/en/codes.json'
        image_url_template = 'https://flagcdn.com/w640/{code}.png'
        retrieved_on = ([DateTime]::UtcNow).ToString('yyyy-MM-dd')
        reference_count = $entries.Count
        excluded = @('us-*', 'eu', 'un')
        sha256 = $hashes
    }
    [IO.File]::WriteAllText((Join-Path $stagedAssets 'SOURCE.json'),
        ($provenance | ConvertTo-Json -Depth 5) + "`n", $utf8)

    if (Test-Path -LiteralPath $currentAssets) {
        Move-Item -LiteralPath $currentAssets -Destination $backupAssets
    }
    try {
        Move-Item -LiteralPath $stagedAssets -Destination $currentAssets
    } catch {
        if ((Test-Path -LiteralPath $backupAssets) -and -not (Test-Path -LiteralPath $currentAssets)) {
            Move-Item -LiteralPath $backupAssets -Destination $currentAssets
        }
        throw
    }
    Write-Host "Updated $($entries.Count) flags. Rebuild and restart drek_flag_cheat."
    if (Test-Path -LiteralPath $backupAssets) {
        Write-Host "Previous catalog preserved at $backupAssets"
    }
} finally {
    Write-Progress -Activity 'Downloading Flagpedia references' -Completed
    if (Test-Path -LiteralPath $stagingRoot) {
        Remove-Item -LiteralPath $stagingRoot -Recurse -Force
    }
}
