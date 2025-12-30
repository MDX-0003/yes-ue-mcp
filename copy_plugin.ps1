# Safe Plugin Copy Script
# Copies yes-ue-mcp plugin to test projects with proper handling for Windows special files

param(
    [ValidateSet("All", "Elpis", "GameAnim")]
    [string]$Target = "All"
)

$src = "F:\src3\yes-ue-mcp"

$targets = @{
    "Elpis"    = "F:\src3\Covenant\ElpisClient\Plugins\yes-ue-mcp"
    "GameAnim" = "F:\src_ue5\GameAnimationSample56\Plugins\yes-ue-mcp"
}

# Directories to exclude from copy
$excludeDirs = @(".git", ".claude", "Tests", ".pytest_cache", ".github", "Docs")

# Files to exclude from copy
$excludeFiles = @("nul", ".gitignore", ".gitattributes", "fix_errors.sh", "TESTING.md")

function Copy-Plugin {
    param([string]$Destination)

    Write-Host "`n========================================" -ForegroundColor Cyan
    Write-Host "Copying to: $Destination" -ForegroundColor Cyan
    Write-Host "========================================" -ForegroundColor Cyan

    # Step 1: Safe removal using robocopy (handles nul and special files)
    if (Test-Path $Destination) {
        Write-Host "Removing existing directory..." -ForegroundColor Yellow

        # Create empty temp directory for robocopy mirror
        $tempEmpty = Join-Path $env:TEMP "empty_$(Get-Random)"
        New-Item -ItemType Directory -Path $tempEmpty -Force | Out-Null

        # Use robocopy /MIR to safely delete (ignores special files like nul)
        $robocopyOutput = robocopy $tempEmpty $Destination /MIR /NFL /NDL /NJH /NJS /nc /ns /np 2>&1

        # Clean up temp directory
        Remove-Item -Path $tempEmpty -Force -ErrorAction SilentlyContinue

        # Remove the now-empty destination (robocopy leaves it)
        Remove-Item -Path $Destination -Recurse -Force -ErrorAction SilentlyContinue

        Write-Host "  Removed existing directory" -ForegroundColor Green
    }

    # Step 2: Create destination directory
    New-Item -ItemType Directory -Path $Destination -Force | Out-Null
    Write-Host "Created destination directory" -ForegroundColor Green

    # Step 3: Copy files with exclusions
    Write-Host "`nCopying files..." -ForegroundColor Yellow
    Write-Host "  Excluding dirs: $($excludeDirs -join ', ')" -ForegroundColor DarkGray
    Write-Host "  Excluding files: $($excludeFiles -join ', ')" -ForegroundColor DarkGray

    $copiedCount = 0
    $skippedCount = 0

    Get-ChildItem -Path $src | ForEach-Object {
        $item = $_

        # Check exclusions
        if ($item.Name -in $excludeDirs -or $item.Name -in $excludeFiles) {
            Write-Host "  Skipped: $($item.Name)" -ForegroundColor DarkGray
            $skippedCount++
            return
        }

        # Copy item
        try {
            Copy-Item -Path $item.FullName -Destination $Destination -Recurse -Force -ErrorAction Stop
            Write-Host "  Copied: $($item.Name)" -ForegroundColor Green
            $copiedCount++
        }
        catch {
            Write-Host "  ERROR copying $($item.Name): $_" -ForegroundColor Red
        }
    }

    # Step 4: Verification
    Write-Host "`nVerification:" -ForegroundColor Yellow
    Write-Host "  Copied: $copiedCount items" -ForegroundColor Green
    Write-Host "  Skipped: $skippedCount items" -ForegroundColor DarkGray

    Write-Host "`nDestination contents:" -ForegroundColor Yellow
    Get-ChildItem $Destination | ForEach-Object {
        $icon = if ($_.PSIsContainer) { "[D]" } else { "[F]" }
        Write-Host "  $icon $($_.Name)" -ForegroundColor White
    }

    return $true
}

# Main execution
Write-Host "yes-ue-mcp Plugin Copy Script" -ForegroundColor Magenta
Write-Host "Source: $src" -ForegroundColor Magenta
Write-Host "Target: $Target" -ForegroundColor Magenta

$success = $true

switch ($Target) {
    "All" {
        foreach ($key in $targets.Keys) {
            if (-not (Copy-Plugin -Destination $targets[$key])) {
                $success = $false
            }
        }
    }
    "Elpis" {
        if (-not (Copy-Plugin -Destination $targets["Elpis"])) {
            $success = $false
        }
    }
    "GameAnim" {
        if (-not (Copy-Plugin -Destination $targets["GameAnim"])) {
            $success = $false
        }
    }
}

Write-Host "`n========================================" -ForegroundColor Cyan
if ($success) {
    Write-Host "Copy completed successfully!" -ForegroundColor Green
} else {
    Write-Host "Copy completed with errors" -ForegroundColor Red
}
Write-Host "========================================`n" -ForegroundColor Cyan
