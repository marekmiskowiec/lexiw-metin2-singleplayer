<#
  Lexiw: smoke test of the launcher. Touches nothing on the running server.

    powershell -ExecutionPolicy Bypass -File tools\Test-Launcher.ps1

  1. every launcher script parses (no syntax errors)
  2. launcher\branding.json is valid JSON
  3. the module loads and exports the functions the launcher calls
  4. the window self-test (-UiSelfTest) passes in Polish and English: every page
     at three widths, the live language switch, navigation, the log box
  5. if the panel is up: /api/launcher-status answers (a skipped check, not a
     failure, when the server is stopped)

  Exit code 0 = all passed, 1 = something failed.
#>
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$failed = 0

function Report([string]$Name, [bool]$Ok, [string]$Detail = '') {
    if ($Ok) { Write-Host ("  OK    {0}" -f $Name) -ForegroundColor Green }
    else { Write-Host ("  FAIL  {0}  {1}" -f $Name, $Detail) -ForegroundColor Red; $script:failed++ }
}

Write-Host '1. Scripts parse'
$scripts = @(Get-ChildItem -LiteralPath $root -Filter 'Metin2-Launcher*.ps1') +
    @(Get-ChildItem -LiteralPath (Join-Path $root 'launcher') -Recurse -File | Where-Object { $_.Extension -in '.ps1', '.psm1' })
foreach ($script in $scripts) {
    $errors = $null
    [void][Management.Automation.Language.Parser]::ParseFile($script.FullName, [ref]$null, [ref]$errors)
    Report $script.Name (-not $errors -or $errors.Count -eq 0) (($errors | Select-Object -First 1 | ForEach-Object { "$($_.Message) (line $($_.Extent.StartLineNumber))" }) -join '')
}

Write-Host '2. branding.json'
try { [void](Get-Content -LiteralPath (Join-Path $root 'launcher\branding.json') -Raw -Encoding UTF8 | ConvertFrom-Json); Report 'branding.json' $true }
catch { Report 'branding.json' $false $_.Exception.Message }

Write-Host '3. Module exports'
try {
    $module = Import-Module (Join-Path $root 'launcher\Metin2Launcher.psm1') -Force -PassThru -DisableNameChecking
    foreach ($name in 'New-M2DatabaseBackup', 'Get-M2LastBackup', 'Remove-M2OldAutoBackups','Test-M2AuthorUpdatesEnabled', 'Get-M2BrandValue') {
        Report $name ($module.ExportedFunctions.ContainsKey($name))
    }
}
catch { Report 'Import-Module' $false $_.Exception.Message }

Write-Host '4. Window self-test'
foreach ($language in 'pl', 'en') {
    $outDir = Join-Path ([IO.Path]::GetTempPath()) ("m2-launcher-test-$language-" + [Guid]::NewGuid().ToString('N').Substring(0, 8))
    try {
        $arguments = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', (Join-Path $root 'Metin2-Launcher-GUI.ps1'), '-UiSelfTest', '-UiTestOutput', $outDir)
        if ($language -eq 'en') { $arguments += @('-UiLanguage', 'en') }
        $text = (& powershell.exe @arguments 2>&1 | Out-String)
        $code = $LASTEXITCODE
        $result = $null
        try { $result = $text.Substring($text.IndexOf('{')) | ConvertFrom-Json } catch { }
        if ($code -ne 0 -or -not $result) { Report "self-test $language" $false "exit $code, no result"; continue }
        $bad = @($result.Checks | Where-Object { $_ -notmatch ': OK$' })
        Report "self-test ${language}: $(@($result.Checks).Count) layouts" ($bad.Count -eq 0) ($bad -join '; ')
        foreach ($key in 'Navigation', 'Sidebar', 'ReportBox', 'LogToggle', 'LogScroll', 'RatesRoute', 'CoffeeLink') {
            if ($result.PSObject.Properties[$key]) { Report "self-test $language $key" ($result.$key -eq 'OK') ([string]$result.$key) }
        }
    }
    finally { Remove-Item -LiteralPath $outDir -Recurse -Force -ErrorAction SilentlyContinue }
}

Write-Host '5. Panel status endpoint'
try {
    $live = Invoke-RestMethod -Uri 'http://127.0.0.1:7794/api/launcher-status' -TimeoutSec 3
    Report "/api/launcher-status ($($live.bots) bots)" ($null -ne $live.bots -and $null -ne $live.cores)
}
catch { Write-Host '  skip  the panel is not running' -ForegroundColor Yellow }

Write-Host ''
if ($failed) { Write-Host "$failed check(s) FAILED" -ForegroundColor Red; exit 1 }
Write-Host 'All checks passed' -ForegroundColor Green
exit 0
