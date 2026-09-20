$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $true
[Console]::OutputEncoding = New-Object System.Text.UTF8Encoding($false)
$root = Split-Path $PSScriptRoot -Parent
$tokens = $null; $errors = $null
$ast = [System.Management.Automation.Language.Parser]::ParseFile((Join-Path $root 'scripts/build-windows.ps1'), [ref]$tokens, [ref]$errors)
if ($errors.Count) { throw 'Script de build inválido.' }
$definition = $ast.Find({param($node) $node -is [System.Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq 'Invoke-BuildStep'}, $true)
if (!$definition) { throw 'Função de build não encontrada.' }
. ([scriptblock]::Create($definition.Extent.Text))
$folder = Join-Path $root 'build/logs'
New-Item -ItemType Directory -Path $folder -Force | Out-Null
$log = Join-Path $folder ("build-script-tests-ps" + $PSVersionTable.PSVersion.Major + '.log')
[System.IO.File]::WriteAllText($log, "Verificação do build`r`n")
$child = $env:ComSpec
function Native-Arguments([int]$Code) {
    @('/d', '/c', "echo signal-error 1>&2 & echo signal-ok & exit /b $Code")
}
Invoke-BuildStep 'Aviso com sucesso' $child (Native-Arguments 0)
if ($LASTEXITCODE -ne 0) { throw 'Aviso tratado como falha.' }
if ($ErrorActionPreference -ne 'Stop' -or !$PSNativeCommandUseErrorActionPreference) { throw 'Preferências do chamador alteradas.' }
$caught = $false
try { Invoke-BuildStep 'Falha esperada' $child (Native-Arguments 7) }
catch { if ($_.Exception.Message -notlike 'Falha esperada falhou. Log:*') { throw }; $caught = $true }
if (!$caught) { throw 'Código de saída 7 foi aceito.' }
$utf8 = New-Object System.Text.UTF8Encoding($false, $true)
$content = $utf8.GetString([IO.File]::ReadAllBytes($log))
if (!$content.Contains('Verificação do build') -or !$content.Contains('signal-error') -or !$content.Contains('signal-ok')) { throw 'Log não preserva stdout/stderr em UTF-8.' }
Write-Host 'Avisos, falhas, UTF-8 e preferências do build verificados.'
exit 0
