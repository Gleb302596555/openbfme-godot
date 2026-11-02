# The build queue of build.bat: at most OPENBFME_BUILD_SLOTS builds (default 2) run on this machine, whichever tree they belong to; the others wait here.
# docs/ennorath/PROCESS.md, section 7. Three builds at once took the machine's memory: each link alone can take gigabytes.
# The slots are named mutexes, not a semaphore: a build that dies frees its slot on its own (the next wait reports it as abandoned, which still acquires it).
# cmake runs directly, at this process's below-normal priority, which ninja and the compilers inherit: `cmd /c start /wait` lost the exit code, so a failed build said BUILD OK.
param(
    [Parameter(Mandatory = $true)][string]$BuildDir,
    [Parameter(Mandatory = $true)][string]$Targets,
    [Parameter(Mandatory = $true)][int]$Jobs
)
[System.Diagnostics.Process]::GetCurrentProcess().PriorityClass = [System.Diagnostics.ProcessPriorityClass]::BelowNormal
$count = 2
if ($env:OPENBFME_BUILD_SLOTS) { $count = [Math]::Max(1, [int]$env:OPENBFME_BUILD_SLOTS) }
$slots = 0..($count - 1) | ForEach-Object { New-Object System.Threading.Mutex($false, "Local\OpenBFMEBuildSlot$_") }
$held = -1
$told = $false
$start = Get-Date
while ($held -lt 0) {
    try {
        $index = [System.Threading.WaitHandle]::WaitAny([System.Threading.WaitHandle[]]$slots, 15000)
        if ($index -ne [System.Threading.WaitHandle]::WaitTimeout) { $held = $index }
    }
    catch [System.Threading.AbandonedMutexException] {
        $held = $_.Exception.MutexIndex
    }
    if ($held -lt 0 -and -not $told) {
        Write-Host "build queue: $count builds are running, waiting for a slot"
        $told = $true
    }
}
if ($told) { Write-Host ("build queue: waited {0:N1} min" -f ((Get-Date) - $start).TotalMinutes) }
$arguments = @('--build', $BuildDir, '--target') + ($Targets -split '\s+' | Where-Object { $_ })
if ($Jobs -ne 0) { $arguments += @('--parallel', "$Jobs") }
$code = 1
try {
    & cmake @arguments
    $code = $LASTEXITCODE
}
finally {
    $slots[$held].ReleaseMutex()
}
exit $code
