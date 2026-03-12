#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Ralph - Autonomous AI implementation loop for QuantFlow.

.DESCRIPTION
    Reads PRD.md, finds the next incomplete user story, dispatches Claude Code
    to implement it, logs the result in progress.txt, and loops until all
    stories are complete or MaxIterations is reached.

.PARAMETER MaxIterations
    Maximum number of stories to attempt (default: unlimited).

.PARAMETER Story
    Implement a specific story (e.g., "US-042") instead of the next one.

.PARAMETER DryRun
    Show which story would be picked without executing.

.PARAMETER Model
    Model to use for Claude Code (default: sonnet).

.PARAMETER StartFrom
    Skip stories before this ID (e.g., "US-030" to start from Phase 3).

.EXAMPLE
    ./ralph.ps1                           # Run until all stories done
    ./ralph.ps1 -MaxIterations 5          # Implement at most 5 stories
    ./ralph.ps1 -Story US-042             # Implement a specific story
    ./ralph.ps1 -DryRun                   # Show next 10 stories without executing
    ./ralph.ps1 -StartFrom US-030         # Skip to Phase 3
    ./ralph.ps1 -Model opus              # Use opus model
#>
[CmdletBinding()]
param(
    [int]$MaxIterations = 9999,
    [string]$Story = "",
    [switch]$DryRun,
    [string]$Model = "sonnet",
    [string]$StartFrom = ""
)

$ErrorActionPreference = "Stop"

# ─── Paths ────────────────────────────────────────────────────────────────────
$ProjectDir = $PSScriptRoot
if (-not $ProjectDir) { $ProjectDir = (Get-Location).Path }
$PRDFile     = Join-Path $ProjectDir "PRD.md"
$ProgressFile = Join-Path $ProjectDir "progress.txt"
$LogDir      = Join-Path $ProjectDir "ralph_logs"
$ArchDoc     = Join-Path $ProjectDir "docs" "ARCHITECTURE_V2.md"
$DesignDoc   = Join-Path $ProjectDir "docs" "plans" "2026-02-24-feed-handler-design.md"

# ─── Validate ─────────────────────────────────────────────────────────────────
if (-not (Test-Path $PRDFile)) {
    Write-Error "PRD.md not found at $PRDFile. Run the PRD generator first."
    exit 1
}
if (-not (Test-Path $LogDir)) {
    New-Item -ItemType Directory -Path $LogDir -Force | Out-Null
}
if (-not (Test-Path $ProgressFile)) {
    @"
# Progress Log

## Learnings
(Patterns discovered during implementation)

---
"@ | Set-Content $ProgressFile -Encoding UTF8
}

# ─── Helpers ──────────────────────────────────────────────────────────────────

function Get-Stories {
    <# Parses PRD.md for all US-XXX story headers. Returns ordered list. #>
    $prd = Get-Content $PRDFile -Raw
    $regex = [regex]'### (US-\d{3}): (.+)'
    $matches = $regex.Matches($prd)
    $stories = @()
    foreach ($m in $matches) {
        $stories += [PSCustomObject]@{
            Id    = $m.Groups[1].Value
            Title = $m.Groups[2].Value.Trim()
        }
    }
    return $stories
}

function Get-CompletedIds {
    <# Reads progress.txt for completed story IDs. #>
    if (-not (Test-Path $ProgressFile)) { return @() }
    $lines = Get-Content $ProgressFile
    $ids = @()
    foreach ($line in $lines) {
        if ($line -match '(US-\d{3})\s*\|\s*COMPLETED') {
            $ids += $Matches[1]
        }
    }
    return $ids
}

function Get-StoryBlock {
    <# Extracts the full story markdown block for a given US-XXX ID. #>
    param([string]$Id)
    $prd = Get-Content $PRDFile -Raw
    $escaped = [regex]::Escape($Id)
    # Match from ### US-XXX: to next ### US-YYY: or ## SECTION or end-of-file
    $pattern = "(?s)(### ${escaped}:.+?)(?=\n### US-\d{3}:|\n## [A-Z]|\z)"
    if ($prd -match $pattern) {
        return $Matches[1].Trim()
    }
    return $null
}

function Get-StoryNumber {
    <# Extracts numeric part of story ID for ordering. #>
    param([string]$Id)
    if ($Id -match 'US-(\d{3})') { return [int]$Matches[1] }
    return 0
}

function Invoke-Claude {
    <# Dispatches a single story to Claude Code for implementation. #>
    param([string]$Id, [string]$Block, [string]$Title)

    $progress = Get-Content $ProgressFile -Raw

    $prompt = @"
You are implementing user story $Id for the QuantFlow algorithmic trading platform.

## Working Directory
$ProjectDir

## Architecture References
- Full architecture: $ArchDoc
- Feed handler design: $DesignDoc
- PRD: $PRDFile

## Your Story to Implement

$Block

## Previously Completed Work
$progress

## Implementation Rules

1. **Read before writing.** Read existing related files BEFORE modifying anything.
2. **All criteria must pass.** Implement every acceptance criterion checkbox.
3. **C++17** for all backend code. **React 18 + TypeScript** for dashboard.
4. **Follow existing patterns.** Match the namespace, naming conventions, and directory structure already in the codebase.
5. **Build after implementing.** Run: cmake -B build -G "MinGW Makefiles" && cmake --build build
6. **Run tests** if you created or modified test files: ./build/run_tests.exe
7. **Stay focused.** Do NOT touch files unrelated to this story.
8. **Production-grade.** No TODOs, no placeholders, no half-implementations.
9. **If a file already exists as a stub**, implement the real logic in it rather than creating a new file.
10. **If the build fails**, fix the issue before finishing.

When you are done, list each acceptance criterion and state PASS or FAIL.
"@

    $logFile = Join-Path $LogDir "${Id}_$(Get-Date -Format 'yyyyMMdd_HHmmss').log"

    Write-Host ""
    Write-Host ("=" * 74) -ForegroundColor Cyan
    Write-Host "  RALPH  |  $Id  |  $Title" -ForegroundColor Cyan
    Write-Host "  $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')  |  Model: $Model" -ForegroundColor DarkCyan
    Write-Host ("=" * 74) -ForegroundColor Cyan
    Write-Host ""

    Push-Location $ProjectDir
    try {
        # Run Claude Code in print mode, fully autonomous (no permission prompts)
        $output = & claude -p $prompt --model $Model --dangerously-skip-permissions 2>&1
        $exitCode = $LASTEXITCODE

        # Save full output to log file
        $output | Out-File -FilePath $logFile -Encoding UTF8

        # Print last 30 lines of output as summary
        $lines = ($output -split "`n")
        $summaryStart = [Math]::Max(0, $lines.Count - 30)
        Write-Host "`n--- Output Summary (last 30 lines) ---" -ForegroundColor DarkGray
        $lines[$summaryStart..($lines.Count - 1)] | ForEach-Object { Write-Host $_ }
        Write-Host "--- End Summary ---`n" -ForegroundColor DarkGray

        Write-Host "  Full log: $logFile" -ForegroundColor DarkGray

        return ($exitCode -eq 0)
    }
    catch {
        Write-Warning "Claude invocation failed: $_"
        "ERROR: $_" | Out-File -FilePath $logFile -Append -Encoding UTF8
        return $false
    }
    finally {
        Pop-Location
    }
}

# ─── Main Loop ────────────────────────────────────────────────────────────────

$allStories = Get-Stories
$doneIds    = Get-CompletedIds
$remaining  = $allStories.Count - $doneIds.Count

# Resolve StartFrom to a number for filtering
$startFromNum = 0
if ($StartFrom) {
    $startFromNum = Get-StoryNumber $StartFrom
}

Write-Host ""
Write-Host "  +========================================================+" -ForegroundColor Yellow
Write-Host "  |       RALPH - Autonomous QuantFlow Builder              |" -ForegroundColor Yellow
Write-Host "  +========================================================+" -ForegroundColor Yellow
Write-Host "  |  Stories total:     $($allStories.Count.ToString().PadLeft(4))                                |" -ForegroundColor Yellow
Write-Host "  |  Stories completed: $($doneIds.Count.ToString().PadLeft(4))                                |" -ForegroundColor Yellow
Write-Host "  |  Stories remaining: $($remaining.ToString().PadLeft(4))                                |" -ForegroundColor Yellow
Write-Host "  |  Model:             $($Model.PadRight(35))|" -ForegroundColor Yellow
Write-Host "  |  Max iterations:    $($MaxIterations.ToString().PadLeft(4))                                |" -ForegroundColor Yellow
if ($StartFrom) {
    Write-Host "  |  Starting from:     $($StartFrom.PadRight(35))|" -ForegroundColor Yellow
}
Write-Host "  +========================================================+" -ForegroundColor Yellow
Write-Host ""

$iteration = 0
$successes = 0
$failures  = 0

foreach ($s in $allStories) {
    # ─── Gate: iteration limit ────────────────────────────────────────
    if ($iteration -ge $MaxIterations) {
        Write-Host "`n  Max iterations reached ($MaxIterations). Stopping." -ForegroundColor Yellow
        break
    }

    # ─── Gate: specific story requested ───────────────────────────────
    if ($Story -and $s.Id -ne $Story) { continue }

    # ─── Gate: StartFrom filter ───────────────────────────────────────
    $storyNum = Get-StoryNumber $s.Id
    if ($startFromNum -gt 0 -and $storyNum -lt $startFromNum) { continue }

    # ─── Gate: already completed ──────────────────────────────────────
    if ($doneIds -contains $s.Id) {
        if ($Story) {
            Write-Host "  $($s.Id) is already completed." -ForegroundColor DarkGray
        }
        continue
    }

    # ─── Extract story content ────────────────────────────────────────
    $block = Get-StoryBlock -Id $s.Id
    if (-not $block) {
        Write-Host "  Could not parse story block for $($s.Id). Skipping." -ForegroundColor Red
        continue
    }

    # ─── Dry run mode ─────────────────────────────────────────────────
    if ($DryRun) {
        Write-Host "  [DRY RUN] $($s.Id) - $($s.Title)" -ForegroundColor Magenta
        $iteration++
        if ($Story) { break }
        if ($iteration -ge 10 -and -not $Story) {
            Write-Host "  ... and $($remaining - $iteration) more" -ForegroundColor DarkGray
            break
        }
        continue
    }

    # ─── Execute story ────────────────────────────────────────────────
    $t0 = Get-Date
    $ok = Invoke-Claude -Id $s.Id -Block $block -Title $s.Title
    $elapsed = (Get-Date) - $t0
    $mins = [math]::Round($elapsed.TotalMinutes, 1)

    # ─── Log result ───────────────────────────────────────────────────
    $status = if ($ok) { "COMPLETED" } else { "ATTEMPTED" }
    $ts = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
    $entry = "$ts | $($s.Id) | $status | $($s.Title) | ${mins}m"
    Add-Content $ProgressFile $entry -Encoding UTF8

    if ($ok) {
        $successes++
        Write-Host "  >> $entry" -ForegroundColor Green
    } else {
        $failures++
        Write-Host "  >> $entry" -ForegroundColor Red
    }

    $iteration++

    # ─── Single story mode ────────────────────────────────────────────
    if ($Story) { break }

    # ─── Brief cooldown between stories ───────────────────────────────
    Write-Host "  Cooldown 3s..." -ForegroundColor DarkGray
    Start-Sleep -Seconds 3
}

# ─── Session Summary ──────────────────────────────────────────────────────────
if (-not $DryRun) {
    Write-Host ""
    Write-Host "  +========================================================+" -ForegroundColor Green
    Write-Host "  |       RALPH - Session Complete                          |" -ForegroundColor Green
    Write-Host "  +========================================================+" -ForegroundColor Green
    Write-Host "  |  Stories attempted: $($iteration.ToString().PadLeft(4))                                |" -ForegroundColor Green
    Write-Host "  |  Successes:         $($successes.ToString().PadLeft(4))                                |" -ForegroundColor Green
    Write-Host "  |  Failures:          $($failures.ToString().PadLeft(4))                                |" -ForegroundColor Green
    Write-Host "  +========================================================+" -ForegroundColor Green
    Write-Host ""
}
