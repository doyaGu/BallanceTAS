param(
    [string]$GameDir = $env:BALLANCE_GAME_DIR,
    [string]$BuildDir = $env:BALLANCE_TAS_BUILD_DIR,
    [int]$TimeoutSeconds = 120,
    [int]$StartupTimeoutSeconds = 20,
    [switch]$Visible,
    [switch]$ValidationRoundTrip,
    [switch]$KeepRunning,
    [switch]$PreserveDeployment,
    [string]$ArtifactDir
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Assert-PathInsideRoot {
    param(
        [Parameter(Mandatory)] [string]$Path,
        [Parameter(Mandatory)] [string]$Root,
        [Parameter(Mandatory)] [string]$Description
    )

    $fullPath = [System.IO.Path]::GetFullPath($Path).TrimEnd('\')
    $fullRoot = [System.IO.Path]::GetFullPath($Root).TrimEnd('\')
    if (!$fullPath.StartsWith($fullRoot + '\', [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to modify $Description outside root ${fullRoot}: $fullPath"
    }
}

function Invoke-BallanceTASBuild {
    param([Parameter(Mandatory)] [string]$Path)

    # Some process launchers expose both PATH and Path. .NET Framework MSBuild
    # cannot create CL.exe with duplicate case-insensitive environment keys.
    $pathKeyCount = @(
        [System.Environment]::GetEnvironmentVariables().Keys |
            Where-Object { $_ -ieq 'PATH' }
    ).Count

    if ($pathKeyCount -gt 1) {
        $command = "set PATH=& cmake --build `"$Path`" --config Release --target BallanceTAS"
        & cmd.exe /d /s /c $command
    } else {
        & cmake --build $Path --config Release --target BallanceTAS
    }
    if ($LASTEXITCODE -ne 0) {
        throw "BallanceTAS Release build failed with exit code $LASTEXITCODE"
    }
}

function Find-Dumpbin {
    param([Parameter(Mandatory)] [string]$CachePath)

    if (!(Test-Path -LiteralPath $CachePath)) {
        return $null
    }
    $linkerLine = Select-String -LiteralPath $CachePath -Pattern '^CMAKE_LINKER:FILEPATH=' |
        Select-Object -First 1
    if (!$linkerLine) {
        return $null
    }
    $linker = ($linkerLine.Line -split '=', 2)[1]
    $candidate = Join-Path (Split-Path -Parent $linker) 'dumpbin.exe'
    if (Test-Path -LiteralPath $candidate) {
        return $candidate
    }
    return $null
}

function Assert-BmlAbiCompatible {
    param(
        [Parameter(Mandatory)] [string]$Dumpbin,
        [Parameter(Mandatory)] [string]$Artifact,
        [Parameter(Mandatory)] [string]$BmlRuntime
    )

    $imports = & $Dumpbin /IMPORTS $Artifact
    if ($LASTEXITCODE -ne 0) {
        throw "dumpbin failed to inspect BallanceTAS imports"
    }
    $exports = & $Dumpbin /EXPORTS $BmlRuntime
    if ($LASTEXITCODE -ne 0) {
        throw "dumpbin failed to inspect BML+ exports"
    }

    $insideBml = $false
    $bmlImports = [System.Collections.Generic.List[string]]::new()
    foreach ($line in $imports) {
        if ($line -match '^\s+BMLPlus\.dll\s*$') {
            $insideBml = $true
            continue
        }
        if ($insideBml -and $line -match '^\s+CK2\.dll\s*$') {
            break
        }
        if ($insideBml -and
            $line -match '^\s+[0-9A-F]+\s+(\S.+)$' -and
            $Matches[1] -notmatch '^(Import |time date|Index of)') {
            $bmlImports.Add($Matches[1].Trim())
        }
    }

    $bmlExports = [System.Collections.Generic.HashSet[string]]::new(
        [System.StringComparer]::Ordinal)
    foreach ($line in $exports) {
        if ($line -match '^\s+\d+\s+[0-9A-F]+\s+[0-9A-F]+\s+(\S.+)$') {
            [void]$bmlExports.Add($Matches[1].Trim())
        }
    }

    $missing = @($bmlImports | Where-Object { !$bmlExports.Contains($_) })
    if ($missing.Count -gt 0) {
        throw "BallanceTAS/BML+ ABI mismatch. Runtime is missing imports:`n$($missing -join "`n")"
    }
    Write-Host "BML+ ABI preflight PASS ($($bmlImports.Count) imported symbols)"
}

function Get-LogText {
    param([Parameter(Mandatory)] [string]$Path)
    if (!(Test-Path -LiteralPath $Path)) {
        return ''
    }
    return [string](Get-Content -LiteralPath $Path -Raw -ErrorAction SilentlyContinue)
}

if ([string]::IsNullOrWhiteSpace($GameDir)) {
    throw "GameDir is required. Pass -GameDir <Ballance install dir> or set BALLANCE_GAME_DIR."
}
if ([string]::IsNullOrWhiteSpace($BuildDir)) {
    throw "BuildDir is required. Pass -BuildDir <CMake build dir> or set BALLANCE_TAS_BUILD_DIR."
}
if ($TimeoutSeconds -lt 10) {
    throw "TimeoutSeconds must be at least 10."
}
if ($StartupTimeoutSeconds -lt 5 -or $StartupTimeoutSeconds -gt $TimeoutSeconds) {
    throw "StartupTimeoutSeconds must be between 5 and TimeoutSeconds."
}

$repo = Split-Path -Parent $PSScriptRoot
$gameRoot = (Resolve-Path -LiteralPath $GameDir).Path.TrimEnd('\')
if ([System.IO.Path]::IsPathRooted($BuildDir)) {
    $buildPath = [System.IO.Path]::GetFullPath($BuildDir)
} else {
    $buildPath = [System.IO.Path]::GetFullPath((Join-Path $repo $BuildDir))
}
if (!(Test-Path -LiteralPath $buildPath)) {
    throw "Build directory not found: $buildPath"
}

$artifact = Join-Path $buildPath "src\Release\BallanceTAS.bmodp"
$modsDir = Join-Path $gameRoot "ModLoader\Mods"
$target = Join-Path $modsDir "BallanceTAS.bmodp"
$log = Join-Path $gameRoot "ModLoader\ModLoader.log"
$player = Join-Path $gameRoot "Bin\Player.exe"
$bmlRuntime = Join-Path $gameRoot "BuildingBlocks\BMLPlus.dll"
$smokeSource = Join-Path $repo "tests\ballance_smoke\LuaRuntimeSmoke"
$tasDir = Join-Path $gameRoot "ModLoader\TAS"
$smokeTarget = Join-Path $tasDir "LuaRuntimeSmoke"
$config = Join-Path $gameRoot "ModLoader\Configs\BallanceTAS.cfg"
$stamp = Get-Date -Format "yyyyMMdd-HHmmss"

if ([string]::IsNullOrWhiteSpace($ArtifactDir)) {
    $ArtifactDir = Join-Path $buildPath "test-artifacts\ballance-smoke-$stamp"
} elseif (![System.IO.Path]::IsPathRooted($ArtifactDir)) {
    $ArtifactDir = Join-Path $repo $ArtifactDir
}
$artifactRoot = [System.IO.Path]::GetFullPath($ArtifactDir)
$backupRoot = Join-Path $artifactRoot 'backup'
New-Item -ItemType Directory -Path $backupRoot -Force | Out-Null

foreach ($required in @($player, $bmlRuntime, $smokeSource)) {
    if (!(Test-Path -LiteralPath $required)) {
        throw "Required Ballance smoke dependency not found: $required"
    }
}
if (Test-Path -LiteralPath $config) {
    $configText = Get-Content -LiteralPath $config -Raw
    $startupSection = [regex]::Match($configText, '(?s)Startup\s*\{(.*?)\}')
    if ($startupSection.Success -and
        ($startupSection.Groups[1].Value -notmatch '(?m)^\s*B\s+Enabled\s+1\s*$' -or
         $startupSection.Groups[1].Value -notmatch '(?m)^\s*B\s+AutoLoad\s+1\s*$')) {
        throw "BallanceTAS Startup.Enabled and Startup.AutoLoad must both be 1 for the smoke project."
    }
}

Invoke-BallanceTASBuild -Path $buildPath
if (!(Test-Path -LiteralPath $artifact)) {
    throw "Build artifact not found: $artifact"
}

$dumpbin = Find-Dumpbin -CachePath (Join-Path $buildPath 'CMakeCache.txt')
if ($dumpbin) {
    Assert-BmlAbiCompatible -Dumpbin $dumpbin -Artifact $artifact -BmlRuntime $bmlRuntime
} else {
    Write-Warning "dumpbin not found from CMake cache; skipping BML+ ABI preflight"
}

$hadTarget = Test-Path -LiteralPath $target
$hadLog = Test-Path -LiteralPath $log
$hadSmokeTarget = Test-Path -LiteralPath $smokeTarget
$launchedProcess = $null
$completed = $false

try {
    $existingPlayers = @(Get-Process Player -ErrorAction SilentlyContinue)
    foreach ($existingPlayer in $existingPlayers) {
        Stop-Process -Id $existingPlayer.Id -Force
        $existingPlayer.WaitForExit(5000) | Out-Null
    }

    if (!(Test-Path -LiteralPath $modsDir)) {
        New-Item -ItemType Directory -Path $modsDir -Force | Out-Null
    }
    if (!(Test-Path -LiteralPath $tasDir)) {
        New-Item -ItemType Directory -Path $tasDir -Force | Out-Null
    }

    if ($hadTarget) {
        Copy-Item -LiteralPath $target -Destination (Join-Path $backupRoot 'BallanceTAS.bmodp') -Force
    }
    if ($hadLog) {
        Copy-Item -LiteralPath $log -Destination (Join-Path $backupRoot 'ModLoader.log') -Force
    }
    if ($hadSmokeTarget) {
        Copy-Item -LiteralPath $smokeTarget -Destination (Join-Path $backupRoot 'LuaRuntimeSmoke') -Recurse -Force
    }

    Assert-PathInsideRoot -Path $smokeTarget -Root $tasDir -Description 'smoke project'
    if (Test-Path -LiteralPath $smokeTarget) {
        Remove-Item -LiteralPath $smokeTarget -Recurse -Force
    }
    New-Item -ItemType Directory -Path $smokeTarget -Force | Out-Null
    Copy-Item -Path (Join-Path $smokeSource '*') -Destination $smokeTarget -Recurse -Force
    Copy-Item -LiteralPath $artifact -Destination $target -Force

    $runCount = if ($ValidationRoundTrip) { 2 } else { 1 }
    $validationReference = $null

    for ($run = 1; $run -le $runCount; ++$run) {
        $referenceControl = Join-Path $smokeTarget 'validation_reference.txt'
        if ($validationReference) {
            Set-Content -LiteralPath $referenceControl -Value $validationReference -Encoding ASCII
        } elseif (Test-Path -LiteralPath $referenceControl) {
            Remove-Item -LiteralPath $referenceControl -Force
        }

        if (Test-Path -LiteralPath $log) {
            Clear-Content -LiteralPath $log
        }

        if ($Visible) {
            $launchedProcess = Start-Process -FilePath $player -WorkingDirectory (Split-Path $player) -PassThru
        } else {
            $launchedProcess = Start-Process -FilePath $player -WorkingDirectory (Split-Path $player) -WindowStyle Hidden -PassThru
        }

        $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
        $startupDeadline = (Get-Date).AddSeconds($StartupTimeoutSeconds)
        $runPassed = $false
        $text = ''
        try {
            while ((Get-Date) -lt $deadline) {
                $launchedProcess.Refresh()
                if ($launchedProcess.HasExited) {
                    throw "Player.exe exited before smoke completion (exit code $($launchedProcess.ExitCode))."
                }

                $text = Get-LogText -Path $log
                if ($text -match '(?m)\[ModLoader/ERROR\]: Failed to load') {
                    throw "BML failed to load a native mod. Check BallanceTAS/BML+ ABI compatibility."
                }
                if ($text -match '(?m)\[BallanceTAS/ERROR\]:' -or
                    $text -match 'LuaRuntimeSmoke.*failed|LuaRuntimeSmoke.*FAIL|Full Smoke FAIL|Validation Smoke failed') {
                    throw "BallanceTAS LuaRuntime smoke reported a failure."
                }
                if ($text -match 'BallanceTAS LuaRuntime Full Smoke PASS') {
                    $runPassed = $true
                    break
                }

                if ((Get-Date) -ge $startupDeadline) {
                    if ($text -notmatch 'Initializing Mod Loader Plus') {
                        throw "BML+ did not initialize within $StartupTimeoutSeconds seconds."
                    }
                    if ($text -notmatch 'Loading Mod BallanceTAS|\[BallanceTAS/') {
                        throw "BallanceTAS did not load within $StartupTimeoutSeconds seconds."
                    }
                }
                Start-Sleep -Milliseconds 500
            }

            if (!$runPassed) {
                throw "Timed out waiting for BallanceTAS LuaRuntime Full Smoke PASS"
            }

            $requiredMarkers = @(
                'Core Smoke PASS',
                'Context Smoke PASS',
                'Shared Smoke PASS',
                'Message Smoke PASS',
                'Input Merge Smoke PASS',
                'TAS Menu UI Smoke PASS',
                'Async Smoke PASS',
                'Math Smoke PASS',
                'World Smoke PASS',
                'State Smoke PASS',
                'Validation Smoke PASS'
            )
            foreach ($marker in $requiredMarkers) {
                if ($text -notmatch [regex]::Escape($marker)) {
                    throw "Full smoke marker appeared without required module marker: $marker"
                }
            }
            if ($run -eq 2 -and $text -notmatch 'Validation Smoke MATCH') {
                throw "Validation round-trip run did not report MATCH."
            }

            $dumpMatch = [regex]::Matches($text, '(?m)Validation Smoke DUMP=(.+)$') |
                Select-Object -Last 1
            if (!$dumpMatch) {
                throw "Validation smoke did not report its dump path."
            }
            $reportedDump = $dumpMatch.Groups[1].Value.Trim()
            if ([System.IO.Path]::IsPathRooted($reportedDump)) {
                $validationDump = [System.IO.Path]::GetFullPath($reportedDump)
            } else {
                $validationDump = [System.IO.Path]::GetFullPath(
                    (Join-Path (Split-Path -Parent $player) $reportedDump))
            }
            Assert-PathInsideRoot -Path $validationDump -Root $smokeTarget `
                -Description 'validation smoke dump'
            if (!(Test-Path -LiteralPath $validationDump)) {
                throw "Validation smoke dump not found: $reportedDump (resolved to $validationDump)"
            }
            if ($run -eq 1 -and $ValidationRoundTrip) {
                $validationReference = $validationDump
            }

            Write-Host "BallanceTAS LuaRuntime Full Smoke PASS (run $run/$runCount)"
        } finally {
            if (Test-Path -LiteralPath $log) {
                Copy-Item -LiteralPath $log -Destination (Join-Path $artifactRoot "run-$run.log") -Force
            }
            if ($launchedProcess -and !$launchedProcess.HasExited -and
                (!$KeepRunning -or $run -lt $runCount -or !$runPassed)) {
                Stop-Process -Id $launchedProcess.Id -Force
                $launchedProcess.WaitForExit(5000) | Out-Null
            }
        }
    }

    $completed = $true
    Write-Host "BallanceTAS smoke artifacts: $artifactRoot"
} catch {
    if (Test-Path -LiteralPath $log) {
        $tail = Get-Content -LiteralPath $log -Tail 120 -ErrorAction SilentlyContinue
        if ($tail) {
            Write-Host $tail -Separator "`n"
        }
    }
    throw
} finally {
    if (!$PreserveDeployment) {
        if ($launchedProcess -and !$launchedProcess.HasExited) {
            Stop-Process -Id $launchedProcess.Id -Force
        }

        if ($hadTarget) {
            Copy-Item -LiteralPath (Join-Path $backupRoot 'BallanceTAS.bmodp') -Destination $target -Force
        } elseif (Test-Path -LiteralPath $target) {
            Remove-Item -LiteralPath $target -Force
        }

        Assert-PathInsideRoot -Path $smokeTarget -Root $tasDir -Description 'smoke project'
        if (Test-Path -LiteralPath $smokeTarget) {
            Remove-Item -LiteralPath $smokeTarget -Recurse -Force
        }
        if ($hadSmokeTarget) {
            Copy-Item -LiteralPath (Join-Path $backupRoot 'LuaRuntimeSmoke') -Destination $smokeTarget -Recurse -Force
        }

        if ($hadLog) {
            Copy-Item -LiteralPath (Join-Path $backupRoot 'ModLoader.log') -Destination $log -Force
        } elseif (Test-Path -LiteralPath $log) {
            Remove-Item -LiteralPath $log -Force
        }
    }
}

if (!$completed) {
    throw "BallanceTAS smoke did not complete."
}
