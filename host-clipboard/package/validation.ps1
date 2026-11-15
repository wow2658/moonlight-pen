function Test-OfficialSunshineOriginal {
    param([string]$Hash, [object]$Manifest)
    # Exact known official payloads only; matching a version string alone is insufficient.
    $accepted = @($Manifest.originalSha256)
    foreach ($variant in @($Manifest.originalVariants)) {
        if ($null -ne $variant -and $variant.sha256) { $accepted += $variant.sha256 }
    }
    return ($Hash -match '^[0-9a-fA-F]{64}$' -and $accepted -contains $Hash)
}
