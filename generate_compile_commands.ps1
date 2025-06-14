# PowerShell script to generate compile_commands.json for IDE support
# This script creates a basic compilation database for C++ IntelliSense

param(
    [string]$BuildDir = "build",
    [string]$SourceDir = "."
)

Write-Host "Generating compile_commands.json for IDE support..."

# Common compile flags based on CMakeLists.txt
$CommonFlags = @(
    "-std=c++23",
    "-DWIN32_LEAN_AND_MEAN",
    "-DNOMINMAX",
    "-Wall",
    "-m32"  # 32-bit architecture
)

# Find all C++ source files
$SourceFiles = Get-ChildItem -Path $SourceDir -Recurse -Include "*.cpp", "*.cxx", "*.cc" | Where-Object { $_.FullName -notmatch "\\build\\" -and $_.FullName -notmatch "\\.git\\" }

# Create compilation database entries
$CompileCommands = @()

foreach ($File in $SourceFiles) {
    $RelativePath = Resolve-Path -Path $File.FullName -Relative
    $Directory = Split-Path -Path $File.FullName -Parent
    
    # Determine include directories based on file location
    $IncludeDirs = @()
    if ($File.FullName -match "app_hook") {
        $IncludeDirs += "-I$SourceDir/app_hook/include"
    }
    if ($File.FullName -match "injector") {
        $IncludeDirs += "-I$SourceDir/injector/include"
    }
    $IncludeDirs += "-I$SourceDir/config"
    
    # Build command
    $Command = "clang++ " + ($CommonFlags + $IncludeDirs + @("-c", $RelativePath)) -join " "
    
    $Entry = @{
        directory = $Directory
        command = $Command
        file = $File.FullName
    }
    
    $CompileCommands += $Entry
}

# Convert to JSON and save
$Json = $CompileCommands | ConvertTo-Json -Depth 3
$OutputPath = Join-Path $SourceDir "compile_commands.json"
$Json | Out-File -FilePath $OutputPath -Encoding UTF8

Write-Host "Generated compile_commands.json with $($CompileCommands.Count) entries"
Write-Host "File saved to: $OutputPath" 