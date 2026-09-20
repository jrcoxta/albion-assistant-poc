param(
    [ValidateSet('Debug', 'Release')][string]$Configuration = 'Release',
    [switch]$Clean,
    [switch]$Run
)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$preset = 'windows-' + $Configuration.ToLowerInvariant()
$build = Join-Path $root ('build\' + $Configuration.ToLowerInvariant())
$dist = Join-Path $root 'dist'
$log = Join-Path $root "build\logs\$Configuration.log"
$timer = [System.Diagnostics.Stopwatch]::StartNew()

if ($Clean -and (Test-Path -LiteralPath $build)) {
    $resolved = (Resolve-Path -LiteralPath $build).Path
    $expected = [System.IO.Path]::GetFullPath($build)
    if ($resolved -ne $expected -or !$resolved.StartsWith($root + '\build\', [System.StringComparison]::OrdinalIgnoreCase) -or
        ((Get-Item -LiteralPath $resolved).Attributes -band [System.IO.FileAttributes]::ReparsePoint)) {
        throw 'Limpeza recusada: diretório de build fora do projeto ou redirecionado.'
    }
    Remove-Item -LiteralPath $resolved -Recurse -Force
}
New-Item -ItemType Directory -Path (Split-Path $log -Parent) -Force | Out-Null
[System.IO.File]::WriteAllText($log, "Albion Assistant - $Configuration`r`n")

function Invoke-BuildStep {
    param([string]$Label, [string]$Executable, [string[]]$Arguments)
    Write-Host $Label
    if (!(Test-Path -LiteralPath $Executable -PathType Leaf)) { throw "Ferramenta ausente: $Executable" }
    # stderr também contém avisos: sucesso/falha é determinado pelo código de saída.
    $ErrorActionPreference = 'Continue'
    $PSNativeCommandUseErrorActionPreference = $false
    & $Executable @Arguments 2>&1 | Out-File -LiteralPath $log -Append -Encoding utf8 -ErrorAction Stop
    if ($LASTEXITCODE -ne 0) {
        Get-Content -LiteralPath $log -Encoding utf8 -Tail 35 | Write-Host
        throw "$Label falhou. Log: $log"
    }
}

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (!(Test-Path -LiteralPath $vswhere)) { throw 'Instale Visual Studio Build Tools com o workload Desenvolvimento para desktop com C++.' }
$vs = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$vs) { throw 'MSVC x64 não encontrado. Instale o workload C++ e o SDK do Windows.' }
$devcmd = Join-Path $vs 'Common7\Tools\VsDevCmd.bat'
$environment = & $env:ComSpec /d /s /c "call `"$devcmd`" -no_logo -arch=x64 -host_arch=x64 >nul && set"
if ($LASTEXITCODE -ne 0) { throw 'Não foi possível preparar o compilador do Visual Studio.' }
foreach ($line in $environment) {
    if ($line -match '^([^=]+)=(.*)$') { [Environment]::SetEnvironmentVariable($matches[1], $matches[2], 'Process') }
}
$env:VSLANG = '1033'
$cmake = Join-Path $vs 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
$ninja = Join-Path $vs 'Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe'
if (!(Test-Path -LiteralPath $cmake)) { $cmake = (Get-Command cmake.exe -ErrorAction Stop).Source }
if (!(Test-Path -LiteralPath $ninja)) { $ninja = (Get-Command ninja.exe -ErrorAction Stop).Source }
$ctest = Join-Path (Split-Path $cmake -Parent) 'ctest.exe'
$jobs = [Math]::Max(1, [Math]::Min(8, [Environment]::ProcessorCount))

Push-Location $root
try {
    Invoke-BuildStep '1/3 Configurando Windows x64...' $cmake @('--preset', $preset, "-DCMAKE_MAKE_PROGRAM=$ninja", '-DCMAKE_CXX_COMPILER=cl.exe')
    Invoke-BuildStep '2/3 Compilando...' $cmake @('--build', '--preset', $preset, '--parallel', "$jobs")
    Invoke-BuildStep '3/3 Executando testes...' $ctest @('--preset', $preset)
    $executable = Join-Path $build 'AlbionAssistant.exe'
    if ($Configuration -eq 'Release') {
        Invoke-BuildStep 'Publicando o executável validado...' $cmake @('--install', $build, '--prefix', $dist)
        $executable = Join-Path $dist 'AlbionAssistant.exe'
    }
    Write-Host ("Concluído em {0:N1}s: {1}" -f $timer.Elapsed.TotalSeconds, $executable)
    Write-Host "Log: $log"
    if ($Run) { Start-Process -FilePath $executable }
} finally { Pop-Location }
