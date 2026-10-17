param(
    [ValidateSet('Read', 'Search', 'Files')][string]$Mode = 'Read',
    [string[]]$RelativePath = @(''),
    [string]$Pattern = '',
    [int]$Context = 0
)
$ErrorActionPreference = 'Stop'
$engineRoot = [System.IO.Path]::GetFullPath('D:\Epic Games\UE_5.8\Engine')
foreach ($relative in $RelativePath) {
    $target = [System.IO.Path]::GetFullPath((Join-Path $engineRoot $relative))
    if ($target -ne $engineRoot -and -not $target.StartsWith($engineRoot + '\', [StringComparison]::OrdinalIgnoreCase)) {
        throw 'Read-Engine only permits paths inside the UE 5.8 Engine directory.'
    }
    switch ($Mode) {
        'Read' { Get-Content -LiteralPath $target }
        'Search' { & rg -n -C $Context -- $Pattern $target }
        'Files' { & rg --files -- $target }
    }
}
