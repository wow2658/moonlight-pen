param([Parameter(Mandatory=$true)][string]$SetupPath)
$ErrorActionPreference='Stop'
$tokens=$null;$errors=$null
$ast=[Management.Automation.Language.Parser]::ParseFile($SetupPath,[ref]$tokens,[ref]$errors)
if ($errors.Count) { throw 'Setup parser failed.' }
# Load ONLY the reviewed pure validation functions, never the installation body.
foreach ($name in @('File-Hash','No-Redirect','Verify-Payload','Verify-Protected','Same-Principal')) {
    $definition=$ast.FindAll({param($node) $node -is [Management.Automation.Language.FunctionDefinitionAst]},$false) | Where-Object Name -eq $name
    if (@($definition).Count -ne 1) { throw 'Missing validation function.' }
    . ([scriptblock]::Create($definition.Extent.Text))
}
$script:checks=0
function Assert-Rejected([scriptblock]$Code,[string]$Message) {
    $failed=$false;try { &$Code } catch { $failed=$true }
    if (-not $failed) { throw $Message };$script:checks++
}
$fixture=Join-Path ([IO.Path]::GetTempPath()) ('moonlight-touch-validation-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $fixture | Out-Null
$payload=Join-Path $fixture 'helper.exe'
[IO.File]::WriteAllText($payload,'not executable; hashing fixture only')
$expected=@{'helper.exe'=(File-Hash $payload)}
Verify-Payload $fixture;$script:checks++
[IO.File]::AppendAllText($payload,'changed')
Assert-Rejected { Verify-Payload $fixture } 'Modified payload was accepted.'
$expected=@{'missing.exe'=('0'*64)}
Assert-Rejected { Verify-Payload $fixture } 'Missing payload was accepted.'

# Fake ACL objects exercise the administrator/system ownership and write rules.
# This test never changes filesystem ACLs or registered tasks.
$script:owner='S-1-5-32-544';$script:grants=@();$script:redirect=$false
function No-Redirect { param($Path) if ($script:redirect) { throw 'Fixture redirected.' } }
function Get-Acl {
    param($LiteralPath)
    $result=New-Object PSObject
    $result | Add-Member ScriptMethod GetOwner { param($type) return New-Object Security.Principal.SecurityIdentifier($script:owner) }
    $result | Add-Member ScriptMethod GetAccessRules { param($a,$b,$type) return $script:grants }
    return $result
}
function Grant($Sid,$Rights) {
    return [pscustomobject]@{AccessControlType='Allow';IdentityReference=(New-Object Security.Principal.SecurityIdentifier($Sid));FileSystemRights=[Security.AccessControl.FileSystemRights]$Rights}
}
$script:grants=@((Grant 'S-1-5-18' 'FullControl'),(Grant 'S-1-5-32-544' 'FullControl'),(Grant 'S-1-5-32-545' 'ReadAndExecute'))
Verify-Protected 'fixture';$script:checks++
$script:owner='S-1-5-18';Verify-Protected 'fixture';$script:checks++
$script:owner='S-1-5-32-545';Assert-Rejected { Verify-Protected 'fixture' } 'Unprivileged owner accepted.'
$script:owner='S-1-5-32-544'
foreach ($right in @('Write','Delete','DeleteSubdirectoriesAndFiles','ChangePermissions','TakeOwnership')) {
    $script:grants=@((Grant 'S-1-1-0' $right))
    Assert-Rejected { Verify-Protected 'fixture' } ('Unprivileged '+$right+' accepted.')
}
$script:grants=@((Grant 'S-1-5-32-545' 'ReadAndExecute'));$script:redirect=$true
Assert-Rejected { Verify-Protected 'fixture' } 'Redirected protected directory accepted.'
if ($ast.Extent.Text -match '(?i)\b(Invoke-WebRequest|Invoke-RestMethod|Add-MpPreference|Set-MpPreference|Set-NetFirewallProfile)\b') { throw 'Unexpected network/security-setting command.' }
$script:checks++
$identity=[Security.Principal.WindowsIdentity]::GetCurrent()
if (-not (Same-Principal $identity.Name $identity.User.Value) -or -not (Same-Principal $identity.User.Value $identity.User.Value) -or (Same-Principal 'S-1-5-18' $identity.User.Value)) { throw 'Principal SID/name resolution failed.' }
$script:checks++
Write-Output ('PASS: '+$script:checks+' installer integrity, missing payload, owner, write/delete/ACL-takeover and redirect rejection checks. No install or device change.')
