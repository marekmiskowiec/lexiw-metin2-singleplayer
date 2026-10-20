# Lexiw: tańce custom w kliencie — taniec 7-24 z paczki "Dances by KaiaProduction" oraz salto
# i pompka z nowego klienta MT2009 (LEXIW_KAIA_DANCES_V1).
#
#   .\Kaia-Dances.ps1 -Action Install   buduje paczkę root z ORYGINALNEJ i podmienia ją w kliencie
#   .\Kaia-Dances.ps1 -Action Restore   przywraca ostatnią kopię oryginalnej paczki root
#   .\Kaia-Dances.ps1 -Action Status    mówi, która paczka root jest teraz w kliencie
#
# Install zawsze startuje od oryginału: gdy w kliencie jest jeszcze oryginał, najpierw robi jego
# kopię w backups\client-root-<data>\; gdy jest już wersja z tańcami, bierze ostatnią taką kopię.
# Po aktualizacji klienta przez MT2009-Patcher paczka root wraca do wersji autora: wtedy znowu
# Install (zrobi kopię nowego oryginału). Budowanie idzie w kontenerze Dockera (obraz panelu
# Seban), bez sieci; z paczek tańców czytane są tylko animacje, skrypty ruchu i ikony.
param(
    [ValidateSet('Install', 'Restore', 'Status')][string]$Action = 'Status',
    [string]$ClientDir = (Join-Path $PSScriptRoot '..\..\..\mt2009 classic singleplayer klient 2.21.1'),
    [string]$KaiaDir = (Join-Path $PSScriptRoot '..\..\..\Dances by KaiaProduction'),
    [string]$NewPackDir = (Join-Path $PSScriptRoot '..\..\..\..\MT2009-new\pack'),
    [string]$Image = 'mt2009classic/seban-panel:latest'
)
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$pack = (Resolve-Path (Join-Path $ClientDir 'pack')).Path
$backups = Join-Path $repo 'backups'

function Test-ClientRunning {
    if (Get-Process metin2client -ErrorAction SilentlyContinue) {
        throw 'Klient gry jest uruchomiony. Zamknij go i spróbuj jeszcze raz.'
    }
}

function Test-KaiaRoot {
    # root.kaia holds the hash of the root.index this script installed.
    $stamp = Join-Path $pack 'root.kaia'
    return (Test-Path $stamp) -and ((Get-Content $stamp -Raw).Trim() -eq (Get-FileHash (Join-Path $pack 'root.index') -Algorithm SHA256).Hash)
}

function Get-LastOriginal {
    Get-ChildItem $backups -Directory -Filter 'client-root-*' -ErrorAction SilentlyContinue | Sort-Object Name | Select-Object -Last 1
}

switch ($Action) {
    'Status' {
        if (Test-KaiaRoot) { 'Klient ma paczkę root z tańcami custom.' }
        else { 'Klient ma oryginalną paczkę root (bez tańców custom).' }
        Get-ChildItem $backups -Directory -Filter 'client-root-*' -ErrorAction SilentlyContinue |
            Sort-Object Name | ForEach-Object { "  kopia oryginału: $($_.FullName)" }
    }
    'Install' {
        Test-ClientRunning
        $kaia = (Resolve-Path $KaiaDir).Path
        $newPack = (Resolve-Path $NewPackDir).Path
        $bad = Get-ChildItem $kaia -Recurse -File | Where-Object { $_.Extension -notin '.gr2', '.msa' }
        if ($bad) { throw "W paczce tańców są inne pliki niż .gr2/.msa: $($bad.FullName -join ', ')" }
        if (Test-KaiaRoot) {
            $original = Get-LastOriginal
            if (-not $original) { throw 'W kliencie jest paczka z tańcami, a w backups\ nie ma kopii oryginału.' }
            $source = $original.FullName
            "Buduję od oryginału z kopii: $source"
        } else {
            $stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
            $source = Join-Path $backups "client-root-$stamp"
            New-Item -ItemType Directory $source -Force | Out-Null
            Copy-Item (Join-Path $pack 'root.index'), (Join-Path $pack 'root.data') $source
            foreach ($f in 'root.index', 'root.data') {
                if ((Get-FileHash (Join-Path $pack $f)).Hash -ne (Get-FileHash (Join-Path $source $f)).Hash) { throw "Kopia $f się nie zgadza." }
            }
            "Kopia oryginału: $source"
        }
        $work = Join-Path $env:TEMP ("kaia-build-" + [guid]::NewGuid().ToString('N'))
        New-Item -ItemType Directory $work | Out-Null
        try {
            & docker run --rm --network none -e EPK_KEYS=/pack/PackMakerLite.json -v "${PSScriptRoot}:/tools:ro" `
                -v "${pack}:/pack:ro" -v "${kaia}:/kaia:ro" -v "${source}:/rootsrc:ro" -v "${newPack}:/newpack:ro" `
                -v "${work}:/out" --entrypoint python $Image -I /tools/build_kaia_root.py /pack /kaia /out /rootsrc /newpack
            if ($LASTEXITCODE -ne 0) { throw 'Budowanie paczki root nie udało się (w kliencie nic nie zostało zmienione).' }
            Copy-Item (Join-Path $work 'root.data') (Join-Path $pack 'root.data') -Force
            Copy-Item (Join-Path $work 'root.index') (Join-Path $pack 'root.index') -Force
            (Get-FileHash (Join-Path $pack 'root.index') -Algorithm SHA256).Hash | Set-Content (Join-Path $pack 'root.kaia') -Encoding ascii
            'Zainstalowane. Okno postaci (C) -> Emocje -> sekcja "Tańce (custom)": taniec 7-24, salto i pompka.'
        } finally {
            Remove-Item $work -Recurse -Force -ErrorAction SilentlyContinue
        }
    }
    'Restore' {
        Test-ClientRunning
        $copy = Get-LastOriginal
        if (-not $copy) { throw 'Nie ma kopii paczki root w backups\.' }
        Copy-Item (Join-Path $copy.FullName 'root.index'), (Join-Path $copy.FullName 'root.data') $pack -Force
        Remove-Item (Join-Path $pack 'root.kaia') -ErrorAction SilentlyContinue
        "Przywrócono oryginalną paczkę root z $($copy.FullName)."
    }
}
