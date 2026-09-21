param(
    [Parameter(Mandatory = $true)][string]$Directory,
    [string]$BuiltExecutable
)
$ErrorActionPreference = 'Stop'
# Antes da publicação, dist pode não existir ou estar vazia. Depois, exige o EXE testado.
if (Test-Path -LiteralPath $Directory) {
    $folder = Get-Item -LiteralPath $Directory -Force
    if (!$folder.PSIsContainer -or ($folder.Attributes -band [IO.FileAttributes]::ReparsePoint)) {
        throw 'Entrega inválida: dist deve ser uma pasta local, sem redirecionamento.'
    }
    $extras = @(Get-ChildItem -LiteralPath $Directory -Force | Where-Object {
        $_.Name -ne 'AlbionAssistant.exe' -or $_.PSIsContainer -or
        ($_.Attributes -band [IO.FileAttributes]::ReparsePoint)
    })
    if ($extras.Count) {
        throw ('Entrega inválida: somente AlbionAssistant.exe é permitido em dist. Revise: ' + ($extras.Name -join ', '))
    }
}
if ($BuiltExecutable) {
    $published = Join-Path $Directory 'AlbionAssistant.exe'
    if (!(Test-Path -LiteralPath $published -PathType Leaf) -or !(Test-Path -LiteralPath $BuiltExecutable -PathType Leaf)) {
        throw 'Entrega inválida: executável publicado ou compilado ausente.'
    }
    $hash = (Get-FileHash -LiteralPath $published -Algorithm SHA256).Hash
    if ($hash -ne (Get-FileHash -LiteralPath $BuiltExecutable -Algorithm SHA256).Hash) {
        throw 'Entrega inválida: executável publicado difere do build testado.'
    }
    return $hash
}
