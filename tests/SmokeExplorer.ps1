param([string]$Compiler = 'D:\soft\CLion 2026.2.4\bin\mingw\bin\g++.exe')
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path $PSScriptRoot -Parent
$binDir = Join-Path $PSScriptRoot 'bin'
$fixtureDir = Join-Path $PSScriptRoot 'explorer-fixture'
New-Item -ItemType Directory -Path $binDir,$fixtureDir -Force | Out-Null
$env:PATH = (Split-Path $Compiler -Parent) + ';' + $env:PATH
& $Compiler -std=c++17 -O2 -Wall -Wextra -Werror -DJPEGVIEW_EXPLORER_ORDER_DIAGNOSTICS -municode (Join-Path $PSScriptRoot 'ExplorerOrderProbe.cpp') (Join-Path $repoRoot 'src\JPEGView\ExplorerOrder.cpp') -o (Join-Path $binDir 'ExplorerOrderProbe.exe') -lole32 -loleaut32 -lshell32 -luuid -lshlwapi
if ($LASTEXITCODE -ne 0) { throw 'Probe compilation failed.' }
Add-Type -AssemblyName System.Drawing
foreach ($spec in @(@('a-large.bmp',96),@('b-small.bmp',32),@('c-medium.bmp',64))) {
    $bitmap = New-Object Drawing.Bitmap ([int]$spec[1]),([int]$spec[1])
    try { $bitmap.Save((Join-Path $fixtureDir $spec[0]),[Drawing.Imaging.ImageFormat]::Bmp) }
    finally { $bitmap.Dispose() }
}
$shell = New-Object -ComObject Shell.Application
$existing = @($shell.Windows() | Where-Object { try { $_.Document.Folder.Self.Path -eq $fixtureDir } catch { $false } })
if ($existing.Count) { throw 'Test folder is already open; close that test-folder window first.' }
$window = $null
$opened = $false
$results = @()
try {
    Start-Process -FilePath "$env:WINDIR\explorer.exe" -ArgumentList @('/n,',('"'+$fixtureDir+'"')) -WindowStyle Hidden
    $opened = $true
    $deadline = [DateTime]::UtcNow.AddSeconds(12)
    do {
        $window = $shell.Windows() | Where-Object { try { $_.Document.Folder.Self.Path -eq $fixtureDir } catch { $false } } | Select-Object -First 1
        if (!$window) { Start-Sleep -Milliseconds 100 }
    } while (!$window -and [DateTime]::UtcNow -lt $deadline)
    if (!$window) { throw 'Could not locate the isolated Explorer test folder.' }
    $image = Join-Path $fixtureDir 'c-medium.bmp'
    foreach ($case in @(
        @{Name='size-ascending'; Sort='prop:+System.Size;'; Expected=@('b-small.bmp','c-medium.bmp','a-large.bmp')},
        @{Name='name-descending'; Sort='prop:-System.ItemNameDisplay;'; Expected=@('c-medium.bmp','b-small.bmp','a-large.bmp')}
    )) {
        $window.Document.SortColumns = $case.Sort
        Start-Sleep -Milliseconds 400
        # Second case also verifies consumption after a slow first-image paint:
        # a snapshot completed within budget must remain usable after 2 seconds.
        $delayPoll = if ($case.Name -eq 'name-descending') { '2200' } else { '0' }
        $lines = @(& (Join-Path $binDir 'ExplorerOrderProbe.exe') $image ([string]$window.HWND) $delayPoll)
        if ($LASTEXITCODE -ne 0) { throw "Snapshot failed for $($case.Name): $lines" }
        $actual = @($lines | ForEach-Object { Split-Path ([string]$_) -Leaf })
        if (($actual -join '|') -ne ($case.Expected -join '|')) {
            throw "Wrong order for $($case.Name) [Shell=$($window.Document.SortColumns)]: $($actual -join ', ')"
        }
        $results += [PSCustomObject]@{Case=$case.Name; Result='PASS'; Order=$actual; ShellSortColumns=[string]$window.Document.SortColumns; PollDelayMs=[int]$delayPoll}
    }
    $results | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $PSScriptRoot 'explorer-smoke-result.json') -Encoding UTF8
    $results | ConvertTo-Json -Depth 5
} finally {
    # Close only the window opened by this test and still showing our fixture.
    if ($opened -and $window) {
        try { if ($window.Document.Folder.Self.Path -eq $fixtureDir) { $window.Quit() } } catch { }
    }
}
