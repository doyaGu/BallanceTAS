param(
    [string]$GameDir = $env:BALLANCE_GAME_DIR,
    [string]$BuildDir = $env:BALLANCE_TAS_BUILD_DIR,
    [string]$PlayerPath = $env:BALLANCE_HARNESS_PLAYER,
    [string]$BmlRuntimeSource,
    [int]$TimeoutSeconds = 120,
    [int]$MaxTicks = 20000,
    [switch]$Visible,
    [switch]$PreserveDeployment,
    [string]$ArtifactDir
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Assert-InsideRoot {
    param([string]$Path, [string]$Root, [string]$Description)
    $fullPath = [System.IO.Path]::GetFullPath($Path).TrimEnd('\')
    $fullRoot = [System.IO.Path]::GetFullPath($Root).TrimEnd('\')
    if (!$fullPath.StartsWith($fullRoot + '\', [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to modify $Description outside ${fullRoot}: $fullPath"
    }
}

function Invoke-ModBuild {
    param([string]$Path)
    $pathKeyCount = @(
        [System.Environment]::GetEnvironmentVariables().Keys |
            Where-Object { $_ -ieq 'PATH' }
    ).Count
    if ($pathKeyCount -gt 1) {
        $cmake = (Get-Command cmake.exe).Source
        & cmd.exe /d /s /c "set PATH=& `"$cmake`" --build `"$Path`" --config Release --target BallanceTAS"
    } else {
        & cmake --build $Path --config Release --target BallanceTAS
    }
    if ($LASTEXITCODE -ne 0) {
        throw "BallanceTAS build failed with exit code $LASTEXITCODE"
    }
}

function Find-BmlRuntime {
    param([string]$GameRoot)
    $updater = Join-Path $GameRoot 'ModLoader\Updater'
    if (!(Test-Path -LiteralPath $updater)) { return $null }
    return Get-ChildItem -LiteralPath $updater -Recurse -File -Filter BMLPlus.dll |
        Sort-Object LastWriteTime -Descending |
        Select-Object -First 1 -ExpandProperty FullName
}

function Stop-MatchingPlayer {
    param([string[]]$Paths)
    $resolved = @($Paths | ForEach-Object {
        if ($_ -and (Test-Path -LiteralPath $_)) {
            [System.IO.Path]::GetFullPath($_)
        }
    })
    foreach ($process in @(Get-Process Player -ErrorAction SilentlyContinue)) {
        try {
            if ($resolved -contains [System.IO.Path]::GetFullPath($process.Path)) {
                Stop-Process -Id $process.Id -Force
                $process.WaitForExit(5000) | Out-Null
            }
        } catch {
            # Ignore processes whose executable path is inaccessible.
        }
    }
}

if ([string]::IsNullOrWhiteSpace($GameDir)) {
    throw 'GameDir is required.'
}
if ([string]::IsNullOrWhiteSpace($BuildDir)) {
    throw 'BuildDir is required.'
}
if ($TimeoutSeconds -lt 10) {
    throw 'TimeoutSeconds must be at least 10.'
}
if ($MaxTicks -lt 1 -or $MaxTicks -gt 10000000) {
    throw 'MaxTicks must be between 1 and 10000000.'
}

$repo = Split-Path -Parent $PSScriptRoot
$gameRoot = (Resolve-Path -LiteralPath $GameDir).Path.TrimEnd('\')
$buildPath = if ([System.IO.Path]::IsPathRooted($BuildDir)) {
    [System.IO.Path]::GetFullPath($BuildDir)
} else {
    [System.IO.Path]::GetFullPath((Join-Path $repo $BuildDir))
}
if ([string]::IsNullOrWhiteSpace($PlayerPath)) {
    $PlayerPath = Join-Path (Split-Path -Parent $repo) 'BallancePlayer\build\src\Release\Player.exe'
}
$player = (Resolve-Path -LiteralPath $PlayerPath).Path

$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
if ([string]::IsNullOrWhiteSpace($ArtifactDir)) {
    $ArtifactDir = Join-Path $buildPath "test-artifacts\harness-$stamp"
} elseif (![System.IO.Path]::IsPathRooted($ArtifactDir)) {
    $ArtifactDir = Join-Path $repo $ArtifactDir
}
$artifactRoot = [System.IO.Path]::GetFullPath($ArtifactDir)
$backupRoot = Join-Path $artifactRoot 'backup'
New-Item -ItemType Directory -Path $backupRoot -Force | Out-Null

$modArtifact = Join-Path $buildPath 'src\Release\BallanceTAS.bmodp'
$modTarget = Join-Path $gameRoot 'ModLoader\Mods\BallanceTAS.bmodp'
$smokeSource = Join-Path $repo 'tests\ballance_smoke\LuaRuntimeSmoke'
$tasRoot = Join-Path $gameRoot 'ModLoader\TAS'
$smokeTarget = Join-Path $tasRoot 'LuaRuntimeSmoke'
$bmlTarget = Join-Path $gameRoot 'BuildingBlocks\BMLPlus.dll'
$gamePlayer = Join-Path $gameRoot 'Bin\Player.exe'
$stagedPlayer = Join-Path $gameRoot 'Bin\PlayerHarness.exe'
$cmo = Join-Path $gameRoot 'base.cmo'
$modLog = Join-Path $gameRoot 'ModLoader\ModLoader.log'

foreach ($required in @($buildPath, $player, $smokeSource, $cmo)) {
    if (!(Test-Path -LiteralPath $required)) {
        throw "Required harness dependency not found: $required"
    }
}

Invoke-ModBuild -Path $buildPath
if (!(Test-Path -LiteralPath $modArtifact)) {
    throw "Built mod not found: $modArtifact"
}

$hadMod = Test-Path -LiteralPath $modTarget
$hadSmoke = Test-Path -LiteralPath $smokeTarget
$hadBml = Test-Path -LiteralPath $bmlTarget
$hadLog = Test-Path -LiteralPath $modLog
$hadStagedPlayer = Test-Path -LiteralPath $stagedPlayer
$process = $null
$completed = $false

try {
    Stop-MatchingPlayer -Paths @($player, $gamePlayer, $stagedPlayer)

    New-Item -ItemType Directory -Path (Split-Path -Parent $modTarget) -Force | Out-Null
    New-Item -ItemType Directory -Path $tasRoot -Force | Out-Null

    if ($hadMod) { Copy-Item -LiteralPath $modTarget -Destination (Join-Path $backupRoot 'BallanceTAS.bmodp') -Force }
    if ($hadSmoke) { Copy-Item -LiteralPath $smokeTarget -Destination (Join-Path $backupRoot 'LuaRuntimeSmoke') -Recurse -Force }
    if ($hadBml) { Copy-Item -LiteralPath $bmlTarget -Destination (Join-Path $backupRoot 'BMLPlus.dll') -Force }
    if ($hadLog) { Copy-Item -LiteralPath $modLog -Destination (Join-Path $backupRoot 'ModLoader.log') -Force }
    if ($hadStagedPlayer) { Copy-Item -LiteralPath $stagedPlayer -Destination (Join-Path $backupRoot 'PlayerHarness.exe') -Force }

    Copy-Item -LiteralPath $player -Destination $stagedPlayer -Force

    if (!$hadBml) {
        if ([string]::IsNullOrWhiteSpace($BmlRuntimeSource)) {
            $BmlRuntimeSource = Find-BmlRuntime -GameRoot $gameRoot
        }
        if ([string]::IsNullOrWhiteSpace($BmlRuntimeSource) -or !(Test-Path -LiteralPath $BmlRuntimeSource)) {
            throw 'BMLPlus.dll is absent and no usable BmlRuntimeSource was found.'
        }
        Copy-Item -LiteralPath $BmlRuntimeSource -Destination $bmlTarget -Force
    }

    Assert-InsideRoot -Path $smokeTarget -Root $tasRoot -Description 'harness project'
    if (Test-Path -LiteralPath $smokeTarget) {
        Remove-Item -LiteralPath $smokeTarget -Recurse -Force
    }
    New-Item -ItemType Directory -Path $smokeTarget -Force | Out-Null
    Copy-Item -Path (Join-Path $smokeSource '*') -Destination $smokeTarget -Recurse -Force
    Copy-Item -LiteralPath $modArtifact -Destination $modTarget -Force

    $requestPath = Join-Path $artifactRoot 'request.json'
    $resultPath = Join-Path $artifactRoot 'result.json'
    $request = [ordered]@{
        protocol = 1
        project = 'LuaRuntimeSmoke'
        max_ticks = $MaxTicks
    }
    $request | ConvertTo-Json | Set-Content -LiteralPath $requestPath -Encoding ASCII
    Assert-InsideRoot -Path $resultPath -Root $artifactRoot -Description 'harness result'
    if (Test-Path -LiteralPath $resultPath) {
        Remove-Item -LiteralPath $resultPath -Force
    }

    $arguments = @(
        '--harness-request', $requestPath,
        '--harness-result', $resultPath,
        '--harness-timeout-ms', ($TimeoutSeconds * 1000),
        '--root-path', $gameRoot,
        '--cmo', $cmo,
        '--config', (Join-Path $artifactRoot 'Player.ini'),
        '--log', (Join-Path $artifactRoot 'Player.log'),
        '--skip-opening', '--always-handle-input',
        '--width', '640', '--height', '480'
    )
    if ($Visible) { $arguments += '--harness-visible' }
    $arguments = @($arguments | ForEach-Object { '"' + ([string]$_).Replace('"', '\"') + '"' })

    $start = @{
        FilePath = $stagedPlayer
        ArgumentList = $arguments
        WorkingDirectory = Join-Path $gameRoot 'Bin'
        PassThru = $true
    }
    if (!$Visible) { $start.WindowStyle = 'Hidden' }
    $process = Start-Process @start

    if (!$process.WaitForExit(($TimeoutSeconds + 15) * 1000)) {
        throw 'Player did not exit after its harness timeout.'
    }
    if (!(Test-Path -LiteralPath $resultPath)) {
        throw "Harness produced no result (Player exit code $($process.ExitCode))."
    }

    $result = Get-Content -LiteralPath $resultPath -Raw | ConvertFrom-Json
    if ($process.ExitCode -ne [int]$result.exit_code) {
        throw "Player/result exit mismatch: $($process.ExitCode) != $($result.exit_code)"
    }
    if ($result.status -ne 'passed' -or [int]$result.exit_code -ne 0) {
        throw "Harness failed: $($result.message)"
    }

    if (Test-Path -LiteralPath $modLog) {
        Copy-Item -LiteralPath $modLog -Destination (Join-Path $artifactRoot 'ModLoader.log') -Force
    }
    $completed = $true
    Write-Host "BallanceTAS dedicated harness PASS: $artifactRoot"
} catch {
    if (Test-Path -LiteralPath $modLog) {
        Copy-Item -LiteralPath $modLog -Destination (Join-Path $artifactRoot 'ModLoader.failed.log') -Force
        Get-Content -LiteralPath $modLog -Tail 100 -ErrorAction SilentlyContinue | Write-Host
    }
    throw
} finally {
    if (!$PreserveDeployment) {
        if ($process -and !$process.HasExited) {
            Stop-Process -Id $process.Id -Force
            $process.WaitForExit(5000) | Out-Null
        }

        if ($hadMod) {
            Copy-Item -LiteralPath (Join-Path $backupRoot 'BallanceTAS.bmodp') -Destination $modTarget -Force
        } elseif (Test-Path -LiteralPath $modTarget) {
            Remove-Item -LiteralPath $modTarget -Force
        }

        Assert-InsideRoot -Path $smokeTarget -Root $tasRoot -Description 'harness project'
        if (Test-Path -LiteralPath $smokeTarget) { Remove-Item -LiteralPath $smokeTarget -Recurse -Force }
        if ($hadSmoke) { Copy-Item -LiteralPath (Join-Path $backupRoot 'LuaRuntimeSmoke') -Destination $smokeTarget -Recurse -Force }

        if ($hadBml) {
            Copy-Item -LiteralPath (Join-Path $backupRoot 'BMLPlus.dll') -Destination $bmlTarget -Force
        } elseif (Test-Path -LiteralPath $bmlTarget) {
            Remove-Item -LiteralPath $bmlTarget -Force
        }

        if ($hadLog) {
            Copy-Item -LiteralPath (Join-Path $backupRoot 'ModLoader.log') -Destination $modLog -Force
        } elseif (Test-Path -LiteralPath $modLog) {
            Remove-Item -LiteralPath $modLog -Force
        }

        if ($hadStagedPlayer) {
            Copy-Item -LiteralPath (Join-Path $backupRoot 'PlayerHarness.exe') -Destination $stagedPlayer -Force
        } elseif (Test-Path -LiteralPath $stagedPlayer) {
            Remove-Item -LiteralPath $stagedPlayer -Force
        }
    }
}

if (!$completed) { throw 'Harness did not complete.' }
