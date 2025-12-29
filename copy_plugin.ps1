$src = "F:\src3\yes-ue-mcp"
$dst = "F:\src_ue5\GameAnimationSample56\Plugins\yes-ue-mcp"

# Remove destination if exists
if (Test-Path $dst) {
    Remove-Item -Path $dst -Recurse -Force
}

# Create destination
New-Item -ItemType Directory -Path $dst -Force | Out-Null

# Copy files, excluding .git, .claude, nul
$excludeDirs = @(".git", ".claude")
$excludeFiles = @("nul", ".gitignore", ".gitattributes")

Get-ChildItem -Path $src | Where-Object {
    $_.Name -notin $excludeDirs -and $_.Name -notin $excludeFiles
} | ForEach-Object {
    Copy-Item -Path $_.FullName -Destination $dst -Recurse -Force
}

Write-Host "Copied plugin to $dst"
Get-ChildItem $dst
