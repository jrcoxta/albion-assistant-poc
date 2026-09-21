$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$validator = Join-Path $root 'scripts/validate-delivery.ps1'
if (!(Test-Path -LiteralPath $validator)) { throw 'Falta o validador da entrega única.' }
$sandbox = Join-Path ([IO.Path]::GetTempPath()) ('albion-delivery-' + [Guid]::NewGuid().ToString('N'))
$dist = Join-Path $sandbox 'dist'
$built = Join-Path $sandbox 'built.exe'
$checks = 0
$shell = (Get-Process -Id $PID).Path
function Reject([string]$Case, [scriptblock]$Action) {
    $caught = $false
    try { & $Action | Out-Null }
    catch { if ($_.Exception.Message -notlike 'Entrega inválida:*') { throw }; $caught = $true }
    if (!$caught) { throw "Aceitou entrega inválida: $Case" }
    $script:checks++
}
try {
    New-Item -ItemType Directory -Path $sandbox | Out-Null
    [IO.File]::WriteAllText($built, 'conteudo do build')
    & $validator -Directory $dist
    Reject 'publicação ausente' { & $validator -Directory $dist -BuiltExecutable $built }
    New-Item -ItemType Directory -Path $dist | Out-Null
    & $validator -Directory $dist
    Reject 'publicação vazia' { & $validator -Directory $dist -BuiltExecutable $built }
    $exe = Join-Path $dist 'AlbionAssistant.exe'
    Copy-Item -LiteralPath $built -Destination $exe
    $expected = (Get-FileHash -LiteralPath $built -Algorithm SHA256).Hash
    $actual = & $validator -Directory $dist -BuiltExecutable $built
    if ($actual -ne $expected) { throw 'Entrega válida não retornou o hash do executável.' }
    foreach ($extra in @('AlbionAssistant-v2.exe', 'Iniciar.cmd', 'pacote.zip', '.oculto')) {
        $path = Join-Path $dist $extra
        [IO.File]::WriteAllText($path, 'preservar')
        if ($extra -eq '.oculto') { (Get-Item -LiteralPath $path).Attributes = [IO.FileAttributes]::Hidden }
        Reject $extra { & $validator -Directory $dist }
        if ([IO.File]::ReadAllText($path) -ne 'preservar') { throw 'Validação alterou arquivo do usuário.' }
        Remove-Item -LiteralPath $path -Force
    }
    $extraFolder = Join-Path $dist 'versao-antiga'
    New-Item -ItemType Directory -Path $extraFolder | Out-Null
    Reject 'pasta extra vazia' { & $validator -Directory $dist }
    Remove-Item -LiteralPath $extraFolder
    [IO.File]::WriteAllText($exe, 'executavel antigo')
    Reject 'executável desatualizado' { & $validator -Directory $dist -BuiltExecutable $built }
    if ([IO.File]::ReadAllText($exe) -ne 'executavel antigo') { throw 'Validação substituiu o executável.' }
    Remove-Item -LiteralPath $exe
    New-Item -ItemType Directory -Path $exe | Out-Null
    Reject 'pasta com nome de executável' { & $validator -Directory $dist }
    Remove-Item -LiteralPath $exe
    $link = Join-Path $sandbox 'atalho-dist'
    New-Item -ItemType Junction -Path $link -Value $dist | Out-Null
    try { Reject 'dist redirecionado' { & $validator -Directory $link } }
    finally { [IO.Directory]::Delete($link) }
    # Executar o script real em uma fixture isolada, sem tocar no dist/AppData reais.
    $project = Join-Path $sandbox 'project'
    $scripts = Join-Path $project 'scripts'
    $fixtureDist = Join-Path $project 'dist'
    $fixtureLogs = Join-Path $project 'build/logs'
    New-Item -ItemType Directory -Path $scripts, $fixtureDist, $fixtureLogs | Out-Null
    Copy-Item -LiteralPath $validator, (Join-Path $root 'scripts/build-windows.ps1') -Destination $scripts
    $fixtureExe = Join-Path $fixtureDist 'AlbionAssistant.exe'
    $fixtureExtra = Join-Path $fixtureDist 'arquivo-extra'
    [IO.File]::WriteAllText($fixtureExe, 'entrega anterior')
    [IO.File]::WriteAllText($fixtureExtra, 'preservar extra')
    $fixtureReport = Join-Path $fixtureLogs 'Release-validation.json'
    [IO.File]::WriteAllText($fixtureReport, '{"automated_status":"passed"}')
    $ErrorActionPreference = 'Continue'
    $PSNativeCommandUseErrorActionPreference = $false
    $output = & $shell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $scripts 'build-windows.ps1') 2>&1
    $code = $LASTEXITCODE
    $ErrorActionPreference = 'Stop'
    $report = [IO.File]::ReadAllText($fixtureReport) | ConvertFrom-Json
    if ($code -eq 0 -or $report.automated_status -ne 'failed' -or $report.error -notlike 'Entrega inválida:*' -or
        $report.sha256 -or !$report.finished_utc) { throw 'Build não registrou reprovação da entrega inválida.' }
    if ([IO.File]::ReadAllText($fixtureExe) -ne 'entrega anterior' -or
        [IO.File]::ReadAllText($fixtureExtra) -ne 'preservar extra' -or
        (Test-Path -LiteralPath (Join-Path $project 'build/release'))) { throw 'Build alterou entrega recusada ou iniciou compilação.' }
    $checks++
    $blocked = Join-Path $sandbox 'blocked'
    New-Item -ItemType Directory -Path $blocked | Out-Null
    Copy-Item -LiteralPath $scripts -Destination $blocked -Recurse
    $cacheTarget = Join-Path $sandbox 'cache-target'
    New-Item -ItemType Directory -Path (Join-Path $cacheTarget 'release') | Out-Null
    $marker = Join-Path $cacheTarget 'release/preservar.txt'
    [IO.File]::WriteAllText($marker, 'nao apagar')
    $cacheLink = Join-Path $blocked 'build'
    New-Item -ItemType Junction -Path $cacheLink -Value $cacheTarget | Out-Null
    try {
        $ErrorActionPreference = 'Continue'
        $output = & $shell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $blocked 'scripts/build-windows.ps1') -Clean 2>&1
        $code = $LASTEXITCODE
        $ErrorActionPreference = 'Stop'
        if ($code -eq 0 -or [IO.File]::ReadAllText($marker) -ne 'nao apagar' -or
            (Test-Path -LiteralPath (Join-Path $cacheTarget 'logs'))) { throw 'Build seguiu redirecionamento antes de limpar/gravar.' }
        $checks++
    } finally { [IO.Directory]::Delete($cacheLink) }
    Write-Host "$checks cenários inválidos rejeitados; entrega válida e arquivos preservados."
} finally {
    # Apagar somente a pasta temporária criada por este teste, sem seguir junctions.
    if (Test-Path -LiteralPath $sandbox) {
        $resolved = (Resolve-Path -LiteralPath $sandbox).Path
        if ($resolved -ne [IO.Path]::GetFullPath($sandbox) -or
            !$resolved.StartsWith([IO.Path]::GetFullPath([IO.Path]::GetTempPath()), [StringComparison]::OrdinalIgnoreCase)) {
            throw 'Limpeza recusada: pasta temporária inesperada.'
        }
        Remove-Item -LiteralPath $resolved -Recurse -Force
    }
}
