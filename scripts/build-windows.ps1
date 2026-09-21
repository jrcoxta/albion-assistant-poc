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
$report = Join-Path $root "build\logs\$Configuration-validation.json"
$validator = Join-Path $PSScriptRoot 'validate-delivery.ps1'
$timer = [System.Diagnostics.Stopwatch]::StartNew()

# Recusar redirecionamentos antes de escrever logs ou limpar qualquer cache.
foreach ($path in @((Join-Path $root 'build'), (Split-Path $log -Parent), $build)) {
    if (Test-Path -LiteralPath $path) {
        $item = Get-Item -LiteralPath $path -Force
        if (!$item.PSIsContainer -or ($item.Attributes -band [IO.FileAttributes]::ReparsePoint)) {
            throw "Build recusado: pasta redirecionada ou inválida: $path"
        }
    }
}
New-Item -ItemType Directory -Path (Split-Path $log -Parent) -Force | Out-Null
[System.IO.File]::WriteAllText($log, "Albion Assistant - $Configuration`r`n")
$evidence = [ordered]@{
    configuration = $Configuration
    started_utc = [DateTime]::UtcNow.ToString('o')
    finished_utc = $null
    automated_status = 'running'
    git_commit = $null
    git_dirty = $null
    executable = $null
    sha256 = $null
    log = $log
    independent_review = 'not_assessed_by_build'
    visual_validation = 'not_executed_by_build'
    error = $null
}
[IO.File]::WriteAllText($report, ($evidence | ConvertTo-Json))

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

Push-Location $root
try {
    if ((Test-Path -LiteralPath (Join-Path $root '.git')) -and (Get-Command git.exe -ErrorAction SilentlyContinue)) {
        $revision = & git.exe rev-parse --verify HEAD 2>$null
        if ($LASTEXITCODE -eq 0) {
            $evidence.git_commit = $revision
            $evidence.git_dirty = [bool](& git.exe status --porcelain)
        }
    }
    if ($Configuration -eq 'Release') { & $validator -Directory $dist }
    if ($Clean -and (Test-Path -LiteralPath $build)) {
        $resolved = (Resolve-Path -LiteralPath $build).Path
        if ($resolved -ne [IO.Path]::GetFullPath($build) -or
            !$resolved.StartsWith($root + '\build\', [StringComparison]::OrdinalIgnoreCase)) {
            throw 'Limpeza recusada: diretório de build fora do projeto.'
        }
        Remove-Item -LiteralPath $resolved -Recurse -Force
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
    Invoke-BuildStep '1/3 Configurando Windows x64...' $cmake @('--preset', $preset, "-DCMAKE_MAKE_PROGRAM=$ninja", '-DCMAKE_CXX_COMPILER=cl.exe')
    Invoke-BuildStep '2/3 Compilando...' $cmake @('--build', '--preset', $preset, '--parallel', "$jobs")
    Invoke-BuildStep '3/3 Executando testes...' $ctest @('--preset', $preset, '--no-tests=error')
    $executable = Join-Path $build 'AlbionAssistant.exe'
    if ($Configuration -eq 'Release') {
        & $validator -Directory $dist
        Invoke-BuildStep 'Publicando o executável testado...' $cmake @('--install', $build, '--prefix', $dist)
        $executable = Join-Path $dist 'AlbionAssistant.exe'
        $evidence.sha256 = & $validator -Directory $dist -BuiltExecutable (Join-Path $build 'AlbionAssistant.exe')
        Invoke-BuildStep 'Conferindo o executável publicado...' (Join-Path $build 'packaging_tests.exe') @($executable)
    } else {
        $evidence.sha256 = (Get-FileHash -LiteralPath $executable -Algorithm SHA256).Hash
    }
    $evidence.executable = $executable
    $evidence.automated_status = 'passed'
    Write-Host ("Build e verificações automáticas concluídos em {0:N1}s: {1}" -f $timer.Elapsed.TotalSeconds, $executable)
    Write-Host "Log: $log"
    Write-Host "Evidência: $report (revisão independente e teste visual são registrados em docs/validacao.md)"
} catch {
    $evidence.automated_status = 'failed'
    $evidence.error = $_.Exception.Message
    throw
} finally {
    $evidence.finished_utc = [DateTime]::UtcNow.ToString('o')
    Pop-Location
    [IO.File]::WriteAllText($report, ($evidence | ConvertTo-Json))
}
if ($Run) { Start-Process -FilePath $executable }
