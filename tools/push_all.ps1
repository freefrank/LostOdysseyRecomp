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
    # v0.5.0 already published reviewed Issue #12 contributor attribution.
    # Check only commits that are not yet on a publishing remote. Already-published
    # history is not rewritten by this script.
    $reviewedPublicBaseline = 'f78f64f'
    Invoke-Git merge-base --is-ancestor $reviewedPublicBaseline $revision
    foreach ($remote in @('origin', 'github')) {
        $null = Invoke-Git remote get-url $remote
        $remoteHead = (Invoke-Git ls-remote $remote refs/heads/main)
        if (-not $remoteHead) { throw "$remote has no main ref" }
        $remoteSha = ($remoteHead -split '\s+')[0]
        $unpublished = (Invoke-Git log "${remoteSha}..${revision}" --format=%B) -join "`n"
        if ($unpublished -match '(?im)^Co-authored-by:.*(Claude|Anthropic)') {
            throw "Unreviewed assistant attribution found in unpublished $remote commits."
        }
        $unpublishedPaths = @(Invoke-Git diff --name-only $remoteSha $revision)
        $excluded = $unpublishedPaths | Where-Object {
            $_ -and $_ -ne '.gitignore' -and $_ -notmatch '/\.gitkeep$' -and
            ($_ -match '(^|/)(private|decompile|decompiled|logs|out|build)/' -or
             $_ -match '\.(xex|fpd|fpi|iso|log|exe|dll|obj|pdb|dmp|bin)$' -or
             $_ -eq 'CLAUDE.md')
        }
        if ($excluded) { throw "Private/generated paths added on unpublished $remote commits: $($excluded -join ', ')" }
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
