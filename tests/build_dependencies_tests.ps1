param(
    [Parameter(Mandatory = $true)][string]$CMake,
    [Parameter(Mandatory = $true)][string]$Ninja,
    [Parameter(Mandatory = $true)][string]$Compiler,
    [string]$BuildDirectory
)
$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $false
$root = Split-Path $PSScriptRoot -Parent
$sandbox = Join-Path ([IO.Path]::GetTempPath()) ('albion-dependencies-' + [Guid]::NewGuid().ToString('N'))
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
try {
    if ($BuildDirectory) {
        Assert-Dependency $BuildDirectory 'CMakeFiles/calibration_tests.dir/tests/calibration_tests.cpp.obj' 'recognition.h'
        Assert-Dependency $BuildDirectory 'CMakeFiles/assistant_rules.dir/src/workspace.cpp.obj' 'workspace.h'
        Assert-Dependency $BuildDirectory 'CMakeFiles/app_flow_tests.dir/tests/app_flow_tests.cpp.obj' 'app.h'
    }
    New-Item -ItemType Directory -Path $source | Out-Null
    $helper = (Join-Path $root 'scripts/msvc-dependencies.cmake').Replace('\', '/')
    $project = @'
cmake_minimum_required(VERSION 3.24)
project(DependencyProbe LANGUAGES CXX)
include("@HELPER@")
add_executable(probe main.cpp untouched.cpp)
target_compile_options(probe PRIVATE /utf-8)
'@
    [IO.File]::WriteAllText((Join-Path $source 'CMakeLists.txt'), $project.Replace('@HELPER@', $helper), $utf8)
    [IO.File]::WriteAllText((Join-Path $source 'main.cpp'), "#include `"value.hpp`"`nint untouched(); int main() { return value() + untouched(); }`n", $utf8)
    [IO.File]::WriteAllText((Join-Path $source 'untouched.cpp'), "int untouched() { return 0; }`n", $utf8)
    $header = Join-Path $source 'value.hpp'
    [IO.File]::WriteAllText($header, "inline int value() { return 1; }`n", $utf8)
    Run-Tool $CMake @('-S', $source, '-B', $binary, '-G', 'Ninja', "-DCMAKE_MAKE_PROGRAM=$Ninja", "-DCMAKE_CXX_COMPILER=$Compiler", '-DCMAKE_BUILD_TYPE=Debug') | Out-Null
    Run-Tool $CMake @('--build', $binary) | Out-Null
    $exe = Join-Path $binary 'probe.exe'
    & $exe
    if ($LASTEXITCODE -ne 1) { throw 'Fixture inicial nao foi compilada com o header original.' }
    $object = Join-Path $binary 'CMakeFiles/probe.dir/main.cpp.obj'
    $untouched = Join-Path $binary 'CMakeFiles/probe.dir/untouched.cpp.obj'
    $objectTime = (Get-Item -LiteralPath $object).LastWriteTimeUtc
    $untouchedTime = (Get-Item -LiteralPath $untouched).LastWriteTimeUtc
    Assert-Dependency $binary 'CMakeFiles/probe.dir/main.cpp.obj' 'value.hpp'
    $rejected = $false
    try { Assert-Dependency $binary 'CMakeFiles/probe.dir/untouched.cpp.obj' 'value.hpp' }
    catch { if ($_.Exception.Message -notlike 'Dependencia ausente*') { throw }; $rejected = $true }
    if (!$rejected) { throw 'Validador aceitou objeto sem a dependencia exigida.' }
    # Reconfigurar cobre o prefixo incorreto armazenado pelo primeiro project().
    Run-Tool $CMake @('-S', $source, '-B', $binary) | Out-Null
    [IO.File]::WriteAllText($header, "inline int value() { return 2; }`n", $utf8)
    Run-Tool $CMake @('--build', $binary) | Out-Null
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
        $temporaryRoot = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\') + '\'
        if ($resolved -ne [IO.Path]::GetFullPath($sandbox) -or !$resolved.StartsWith($temporaryRoot, [StringComparison]::OrdinalIgnoreCase) -or
            !(Split-Path -Leaf $resolved).StartsWith('albion-dependencies-')) { throw 'Limpeza temporaria recusada.' }
        Remove-Item -LiteralPath $resolved -Recurse -Force
    }
}
