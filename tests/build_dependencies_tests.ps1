param(
    [Parameter(Mandatory = $true)][string]$CMake,
    [ValidateSet('Ninja', 'Visual Studio 18 2026')][string]$Generator = 'Ninja',
    [ValidateSet('Debug', 'Release')][string]$Configuration = 'Debug',
    [string]$Ninja,
    [string]$Compiler,
    [string]$BuildDirectory
)
$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $false
$root = Split-Path $PSScriptRoot -Parent
if ($Generator -eq 'Ninja') { $sandboxBase = [IO.Path]::GetTempPath() }
elseif ($BuildDirectory) { $sandboxBase = $BuildDirectory }
else { $sandboxBase = Join-Path $root 'build' }
if (!(Test-Path -LiteralPath $sandboxBase -PathType Container) -or
    ((Get-Item -LiteralPath $sandboxBase).Attributes -band [IO.FileAttributes]::ReparsePoint)) {
    throw 'Diretorio do teste de dependencias ausente ou redirecionado.'
}
$sandbox = Join-Path $sandboxBase ('albion-dependencies-' + [Guid]::NewGuid().ToString('N'))
$source = Join-Path $sandbox 'source'
$binary = Join-Path $sandbox 'build'
$utf8 = New-Object System.Text.UTF8Encoding($false)
function Run-Tool([string]$Tool, [string[]]$Arguments) {
    $output = & $Tool @Arguments 2>&1
    if ($LASTEXITCODE -ne 0) { throw "Ferramenta falhou ($LASTEXITCODE): $Tool`n$($output -join "`n")" }
    $output
}
function Assert-Dependency([string]$Directory, [string]$Object, [string]$Header) {
    $deps = Run-Tool $Ninja @('-C', $Directory, '-t', 'deps', $Object)
    if (($deps -join "`n") -notmatch ('(?m)[\\/]' + [regex]::Escape($Header) + '\r?$')) {
        throw "Dependencia ausente de $Header em $Object. Execute build-windows.ps1 com -Clean antes de usar este cache.`n$($deps -join "`n")"
    }
}
function Assert-MsbuildDependency([string]$Directory, [string]$Target, [string]$SourceFile, [string]$Header) {
    $folder = Join-Path $Directory "$Target.dir/$Configuration"
    if (!(Test-Path -LiteralPath $folder -PathType Container)) { throw "Compilacao do alvo ausente: $Target" }
    $logs = @(Get-ChildItem -LiteralPath $folder -Filter 'CL.read.1.tlog' -Recurse -File |
        Where-Object { $_.Directory.Name -notlike '*_MD.tlog' })
    if ($logs.Count -ne 1) { throw "Dependencias MSBuild ausentes ou ambiguas: $Target" }
    $bytes = [IO.File]::ReadAllBytes($logs[0].FullName)
    if ($bytes.Length -lt 2 -or $bytes[0] -ne 255 -or $bytes[1] -ne 254) { throw 'Formato inesperado do registro MSBuild.' }
    $fullSource = [IO.Path]::GetFullPath($SourceFile).ToUpperInvariant()
    $fullHeader = [IO.Path]::GetFullPath($Header).ToUpperInvariant()
    $sourceSeen = $false; $inside = $false
    foreach ($line in [IO.File]::ReadAllLines($logs[0].FullName, [Text.Encoding]::Unicode)) {
        if ($line.StartsWith('^')) {
            $inside = $line.Substring(1).Equals($fullSource, [StringComparison]::OrdinalIgnoreCase)
            if ($inside) { $sourceSeen = $true }
        } elseif ($inside -and $line.Equals($fullHeader, [StringComparison]::OrdinalIgnoreCase)) {
            return
        }
    }
    if (!$sourceSeen) { throw "Fonte nao registrada pelo MSBuild: $SourceFile" }
    throw "Dependencia ausente de $([IO.Path]::GetFileName($Header)) em $SourceFile"
}
try {
    if ($Generator -eq 'Ninja' -and (!$Ninja -or !$Compiler)) { throw 'Informe Ninja e compilador para conferir dependencias.' }
    if ($BuildDirectory) {
        if ($Generator -eq 'Ninja') {
            Assert-Dependency $BuildDirectory 'CMakeFiles/calibration_tests.dir/tests/calibration_tests.cpp.obj' 'recognition.h'
            Assert-Dependency $BuildDirectory 'CMakeFiles/assistant_rules.dir/src/workspace.cpp.obj' 'workspace.h'
            Assert-Dependency $BuildDirectory 'CMakeFiles/app_flow_tests.dir/tests/app_flow_tests.cpp.obj' 'app.h'
        } else {
            Assert-MsbuildDependency $BuildDirectory 'calibration_tests' (Join-Path $root 'tests/calibration_tests.cpp') (Join-Path $root 'src/recognition.h')
            Assert-MsbuildDependency $BuildDirectory 'assistant_rules' (Join-Path $root 'src/workspace.cpp') (Join-Path $root 'src/workspace.h')
            Assert-MsbuildDependency $BuildDirectory 'app_flow_tests' (Join-Path $root 'tests/app_flow_tests.cpp') (Join-Path $root 'src/app.h')
        }
    }
    New-Item -ItemType Directory -Path $source | Out-Null
    $helper = (Join-Path $root 'scripts/msvc-dependencies.cmake').Replace('\', '/')
    $project = @'
cmake_minimum_required(VERSION 3.24)
project(DependencyProbe LANGUAGES CXX)
include("@HELPER@")
add_executable(probe main.cpp untouched.cpp)
target_compile_options(probe PRIVATE /utf-8 /showIncludes)
'@
    [IO.File]::WriteAllText((Join-Path $source 'CMakeLists.txt'), $project.Replace('@HELPER@', $helper), $utf8)
    [IO.File]::WriteAllText((Join-Path $source 'main.cpp'), "#include `"value.hpp`"`nint untouched(); int main() { return value() + untouched(); }`n", $utf8)
    [IO.File]::WriteAllText((Join-Path $source 'untouched.cpp'), "int untouched() { return 0; }`n", $utf8)
    $header = Join-Path $source 'value.hpp'
    [IO.File]::WriteAllText($header, "inline int value() { return 1; }`n", $utf8)
    if ($Generator -eq 'Ninja') {
        Run-Tool $CMake @('-S', $source, '-B', $binary, '-G', 'Ninja', "-DCMAKE_MAKE_PROGRAM=$Ninja", "-DCMAKE_CXX_COMPILER=$Compiler", "-DCMAKE_BUILD_TYPE=$Configuration") | Out-Null
    } else {
        Run-Tool $CMake @('-S', $source, '-B', $binary, '-G', $Generator, '-A', 'x64') | Out-Null
    }
    Run-Tool $CMake @('--build', $binary, '--config', $Configuration) | Out-Null
    if ($Generator -eq 'Ninja') { $exe = Join-Path $binary 'probe.exe' }
    else { $exe = Join-Path $binary "$Configuration/probe.exe" }
    & $exe
    if ($LASTEXITCODE -ne 1) { throw 'Fixture inicial nao foi compilada com o header original.' }
    if ($Generator -eq 'Ninja') {
        $object = Join-Path $binary 'CMakeFiles/probe.dir/main.cpp.obj'
        $untouched = Join-Path $binary 'CMakeFiles/probe.dir/untouched.cpp.obj'
    } else {
        $object = Join-Path $binary "probe.dir/$Configuration/main.obj"
        $untouched = Join-Path $binary "probe.dir/$Configuration/untouched.obj"
    }
    $objectTime = (Get-Item -LiteralPath $object).LastWriteTimeUtc
    $untouchedTime = (Get-Item -LiteralPath $untouched).LastWriteTimeUtc
    if ($Generator -eq 'Ninja') { Assert-Dependency $binary 'CMakeFiles/probe.dir/main.cpp.obj' 'value.hpp' }
    else { Assert-MsbuildDependency $binary 'probe' (Join-Path $source 'main.cpp') $header }
    $rejected = $false
    try {
        if ($Generator -eq 'Ninja') { Assert-Dependency $binary 'CMakeFiles/probe.dir/untouched.cpp.obj' 'value.hpp' }
        else { Assert-MsbuildDependency $binary 'probe' (Join-Path $source 'untouched.cpp') $header }
    }
    catch { if ($_.Exception.Message -notlike 'Dependencia ausente*') { throw }; $rejected = $true }
    if (!$rejected) { throw 'Validador aceitou objeto sem a dependencia exigida.' }
    # Reconfigurar cobre o prefixo incorreto armazenado pelo primeiro project().
    Run-Tool $CMake @('-S', $source, '-B', $binary) | Out-Null
    [IO.File]::WriteAllText($header, "inline int value() { return 2; }`n", $utf8)
    Run-Tool $CMake @('--build', $binary, '--config', $Configuration) | Out-Null
    & $exe
    if ($LASTEXITCODE -ne 2 -or (Get-Item -LiteralPath $object).LastWriteTimeUtc -le $objectTime) {
        throw 'Alterar somente um header nao recompilou seu consumidor.'
    }
    if ((Get-Item -LiteralPath $untouched).LastWriteTimeUtc -ne $untouchedTime) {
        throw 'Build incremental recompilou fonte sem dependencia do header alterado.'
    }
    Write-Host 'Dependencia registrada; mudar header recompila consumidor e altera resultado; fonte independente preservada.'
} finally {
    if (Test-Path -LiteralPath $sandbox) {
        $resolved = (Resolve-Path -LiteralPath $sandbox).Path
        $permittedRoot = [IO.Path]::GetFullPath($sandboxBase).TrimEnd('\') + '\'
        if ($resolved -ne [IO.Path]::GetFullPath($sandbox) -or !$resolved.StartsWith($permittedRoot, [StringComparison]::OrdinalIgnoreCase) -or
            !(Split-Path -Leaf $resolved).StartsWith('albion-dependencies-')) { throw 'Limpeza temporaria recusada.' }
        Remove-Item -LiteralPath $resolved -Recurse -Force
    }
}
