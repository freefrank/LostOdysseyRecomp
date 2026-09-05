param([switch]$CheckOnly)

$ErrorActionPreference = 'Stop'
function Invoke-Git {
    param([Parameter(ValueFromRemainingArguments = $true)][string[]]$Arguments)
    $result = & git @Arguments
    if ($LASTEXITCODE -ne 0) { throw "git failed: $($Arguments -join ' ')" }
    return $result
}

Push-Location (Join-Path $PSScriptRoot '..')
try {
    if ((Invoke-Git branch --show-current) -ne 'main') { throw 'Switch to main before publishing.' }
    $revision = Invoke-Git rev-parse refs/heads/main
    # Require the reviewed public baseline and reject reintroduced private ancestry.
    Invoke-Git merge-base --is-ancestor 6bd28e98fac10f7014f0769fcc88c993887edd6c $revision
    $messages = (Invoke-Git log $revision --format=%B) -join "`n"
    if ($messages -match '(?im)^Co-authored-by:.*(Claude|Anthropic)') {
        throw 'Unreviewed assistant attribution found in published history.'
    }
    $paths = Invoke-Git ls-tree -r --name-only $revision
    $excluded = $paths | Where-Object {
        $_ -ne '.gitignore' -and $_ -notmatch '/\.gitkeep$' -and
        ($_ -match '(^|/)(private|decompile|decompiled|logs|out|build)/' -or
         $_ -match '\.(xex|fpd|fpi|iso|log|exe|dll|obj|pdb|dmp|bin)$' -or
         $_ -eq 'CLAUDE.md')
    }
    if ($excluded) { throw "Private/generated paths are tracked: $($excluded -join ', ')" }
    foreach ($remote in @('origin', 'github')) {
        $null = Invoke-Git remote get-url $remote
    }
    if ($CheckOnly) { Write-Host "Publication checks passed: $revision"; return }

    # Publish only this exact main commit, never archive refs or automatic tags.
    foreach ($remote in @('origin', 'github')) {
        Invoke-Git -c push.followTags=false push $remote "${revision}:refs/heads/main"
    }
    foreach ($remote in @('origin', 'github')) {
        $remoteHead = Invoke-Git ls-remote $remote refs/heads/main
        if (($remoteHead -split '\s+')[0] -ne $revision) { throw "$remote main does not match $revision" }
    }
    Write-Host "Both remotes verified: $revision"
}
finally { Pop-Location }
