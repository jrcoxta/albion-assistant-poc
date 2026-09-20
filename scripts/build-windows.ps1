param(
    [ValidateSet('Debug', 'Release')][string]$Configuration = 'Debug',
    [switch]$Run
)
$ErrorActionPreference = 'Stop'
$env:VSLANG = '1033'
$root = Split-Path $PSScriptRoot -Parent
if ($root -notmatch '^[A-Za-z]:\\') { throw 'O repositório deve estar no sistema de arquivos Windows (ex.: /mnt/c/...).'}
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (!(Test-Path $vswhere)) { throw 'vswhere não encontrado. Instale o workload C++ do Visual Studio.' }
$vs = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$vs) { throw 'MSVC x64 não encontrado pelo vswhere.' }
$devcmd = Join-Path $vs 'Common7\Tools\VsDevCmd.bat'
$environment = & $env:ComSpec /d /s /c "call `"$devcmd`" -no_logo -arch=x64 -host_arch=x64 >nul && set"
if ($LASTEXITCODE -ne 0) { throw 'VsDevCmd falhou.' }
foreach ($line in $environment) {
    if ($line -match '^([^=]+)=(.*)$') { [Environment]::SetEnvironmentVariable($matches[1], $matches[2], 'Process') }
}
$cmake = Join-Path $vs 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
$ninja = Join-Path $vs 'Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe'
if (!(Test-Path $cmake)) { $cmake = (Get-Command cmake.exe -ErrorAction Stop).Source }
if (!(Test-Path $ninja)) { $ninja = (Get-Command ninja.exe -ErrorAction Stop).Source }
$build = Join-Path $root "build\windows\$Configuration"
& $cmake -S $root -B $build -G Ninja "-DCMAKE_BUILD_TYPE=$Configuration" "-DCMAKE_MAKE_PROGRAM=$ninja" '-DCMAKE_CXX_COMPILER=cl.exe' '-DBUILD_TESTING=ON'
if ($LASTEXITCODE -ne 0) { throw 'Configuração CMake falhou.' }
& $cmake --build $build
if ($LASTEXITCODE -ne 0) { throw 'Compilação falhou.' }
$ctest = Join-Path (Split-Path $cmake -Parent) 'ctest.exe'
& $ctest --test-dir $build --output-on-failure
if ($LASTEXITCODE -ne 0) { throw 'Testes falharam.' }
if ($Run) { & (Join-Path $build 'AlbionAssistant.exe'); if ($LASTEXITCODE -ne 0) { throw 'Aplicativo terminou com erro.' } }
