[CmdletBinding()]
param(
    [string]$GameRoot = 'C:\Users\kakut\Games\Ballance',
    [string]$ProfileRoot,
    [string]$PlayerExe = 'C:\Users\kakut\Works\Ballance\BallancePlayer\build\src\Release\Player.exe',
    [string]$BallanceTasMod,
    [string]$BmlPlusBuildingBlock,
    [string[]]$DependencyMods = @(),
    [string]$Target,
    [switch]$Autoplay,
    [switch]$ExitOnComplete,
    [switch]$Launch
)

$ErrorActionPreference = 'Stop'

if (-not $ProfileRoot) {
    $ProfileRoot = Join-Path $PSScriptRoot '..\build\TASProfile'
}
if (-not $BallanceTasMod) {
    $BallanceTasMod = Join-Path $PSScriptRoot '..\build\src\Release\BallanceTAS.bmodp'
}

function Resolve-RequiredPath([string]$Path, [string]$Description) {
    if (-not $Path -or -not (Test-Path -LiteralPath $Path)) {
        throw "$Description was not found: $Path"
    }
    return (Resolve-Path -LiteralPath $Path).Path
}

$game = Resolve-RequiredPath $GameRoot 'Game root'
$player = Resolve-RequiredPath $PlayerExe 'TAS Player executable'
$mod = Resolve-RequiredPath $BallanceTasMod 'BallanceTAS mod'
$profile = [IO.Path]::GetFullPath($ProfileRoot)
if ($profile.TrimEnd('\') -ieq $game.TrimEnd('\')) {
    throw 'ProfileRoot must not be the original game directory.'
}

if (-not $BmlPlusBuildingBlock) {
    $BmlPlusBuildingBlock = Join-Path $game 'BuildingBlocks\AngelScript.dll'
}
$bmlPlus = Resolve-RequiredPath $BmlPlusBuildingBlock 'BMLPlus building block'

$profileBin = Join-Path $profile 'Bin'
$profileBlocks = Join-Path $profile 'BuildingBlocks'
$profileLoader = Join-Path $profile 'ModLoader'
$profileMods = Join-Path $profileLoader 'Mods'
foreach ($directory in @(
    $profileBin, $profileBlocks, $profileMods,
    (Join-Path $profileLoader 'Configs'),
    (Join-Path $profileLoader 'TAS'),
    (Join-Path $profileLoader 'Fonts'),
    (Join-Path $profileLoader 'Themes'),
    (Join-Path $profileLoader 'Updater')
)) {
    New-Item -ItemType Directory -Path $directory -Force | Out-Null
}

# Preserve isolation: refuse an existing profile containing unapproved mods.
$allowedModNames = @('BallanceTAS.bmodp') + @($DependencyMods | ForEach-Object { Split-Path $_ -Leaf })
$unexpectedMods = Get-ChildItem -LiteralPath $profileMods -File -ErrorAction SilentlyContinue |
    Where-Object { $_.Name -notin $allowedModNames }
if ($unexpectedMods) {
    throw "Profile contains non-whitelisted mods: $($unexpectedMods.Name -join ', ')"
}

Copy-Item -Path (Join-Path $game 'BuildingBlocks\*') -Destination $profileBlocks -Force
Copy-Item -LiteralPath $bmlPlus -Destination (Join-Path $profileBlocks 'AngelScript.dll') -Force
Copy-Item -LiteralPath $player -Destination (Join-Path $profileBin 'PlayerTAS.exe') -Force
Copy-Item -LiteralPath $mod -Destination (Join-Path $profileMods 'BallanceTAS.bmodp') -Force
foreach ($runtimeDll in @('CK2.dll', 'VxMath.dll', 'CKZlib.dll')) {
    $runtimePath = Resolve-RequiredPath (Join-Path $game "Bin\$runtimeDll") "Player runtime $runtimeDll"
    Copy-Item -LiteralPath $runtimePath -Destination (Join-Path $profileBin $runtimeDll) -Force
}

foreach ($dependency in $DependencyMods) {
    $resolvedDependency = Resolve-RequiredPath $dependency 'Whitelisted dependency mod'
    Copy-Item -LiteralPath $resolvedDependency -Destination (Join-Path $profileMods (Split-Path $resolvedDependency -Leaf)) -Force
}

# Copy only BMLPlus support assets and the TAS library; never copy the original Mods directory.
foreach ($supportDirectory in @('Fonts', 'Themes', 'Updater')) {
    $source = Join-Path $game "ModLoader\$supportDirectory"
    if (Test-Path -LiteralPath $source) {
        Copy-Item -Path (Join-Path $source '*') -Destination (Join-Path $profileLoader $supportDirectory) -Recurse -Force
    }
}
$sourceTas = Join-Path $game 'ModLoader\TAS'
if (Test-Path -LiteralPath $sourceTas) {
    Copy-Item -Path (Join-Path $sourceTas '*') -Destination (Join-Path $profileLoader 'TAS') -Recurse -Force
}

$playerTas = Join-Path $profileBin 'PlayerTAS.exe'
$arguments = @(
    '--root-path', $game,
    '--plugin-path', (Join-Path $game 'Plugins'),
    '--render-engine-path', (Join-Path $game 'RenderEngines'),
    '--manager-path', (Join-Path $game 'Managers'),
    '--building-block-path', $profileBlocks,
    '--sound-path', (Join-Path $game 'Sounds'),
    '--bitmap-path', (Join-Path $game 'Textures'),
    '--data-path', $game,
    '--cmo', (Join-Path $game 'base.cmo'),
    '--verbose'
)

if ($Target) {
    $resolvedTarget = $Target
    if (Test-Path -LiteralPath $Target) {
        $resolvedTarget = (Resolve-Path -LiteralPath $Target).Path
    }
    $arguments += @('--tas', $resolvedTarget)
    if ($Autoplay) { $arguments += '--tas-autoplay' }
    if ($ExitOnComplete) { $arguments += '--tas-exit-on-complete' }
}

Write-Host "TAS profile ready: $profile"
Write-Host "Player: $playerTas"
if ($Launch) {
    if (-not $Target) { throw '-Launch requires -Target.' }
    $quotedArguments = $arguments | ForEach-Object {
        if ($_ -match '[\s"]') { '"' + ($_ -replace '"', '\"') + '"' } else { $_ }
    }
    $process = Start-Process -FilePath $playerTas -ArgumentList $quotedArguments `
        -WorkingDirectory $game -Wait -PassThru
    exit $process.ExitCode
}

[pscustomobject]@{
    ProfileRoot = $profile
    Player = $playerTas
    Arguments = $arguments
}
