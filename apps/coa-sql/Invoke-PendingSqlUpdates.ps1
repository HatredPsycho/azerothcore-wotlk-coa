<#
.SYNOPSIS
Apply the pending SQL updates of this checkout to a running CoA database and register them.

.DESCRIPTION
Compares the files in data/sql/updates/pending_db_auth, pending_db_characters and
pending_db_world with the `updates` table of the matching database and applies whatever is
missing, in file name order, exactly the way the server's own updater does:

  * the hash is the SHA1 of the file read in text mode (CRLF is folded to LF), upper case,
  * an applied file is registered with REPLACE INTO `updates` (name, hash, state, speed),
    state PENDING and the measured run time in milliseconds,
  * a file whose recorded hash differs is applied again (use -SkipChanged to leave it alone),
  * a file whose hash is already recorded under a name that no longer exists is treated as a
    rename and only the row is renamed.

Stop worldserver before running this, or let the server apply its updates itself. Running both
at once can apply the same file twice.

.PARAMETER DefaultsFile
MySQL client option file holding the credentials ([client] section).

.PARAMETER PasswordPrompt
Ask for the password instead of reading it from an option file.

.PARAMETER Mysql
Path to mysql.exe. Searched on PATH and in the MySQL/MariaDB installation directories.

.PARAMETER Root
Repository root. Defaults to the checkout this script lives in.

.PARAMETER Databases
Which databases to update: auth, characters, world. All three by default.

.PARAMETER CharacterSet
Connection character set used while applying a file. Defaults to utf8, the one the server's
updater uses; utf8mb4 makes a user variable collide with a utf8mb4_unicode_ci column.

.PARAMETER DryRun
Report what would be applied without changing anything.

.EXAMPLE
.\Invoke-PendingSqlUpdates.ps1 -DefaultsFile admin-client.ini

.EXAMPLE
.\Invoke-PendingSqlUpdates.ps1 -DefaultsFile admin-client.ini -Databases world -DryRun
#>
[CmdletBinding()]
param(
    [string]$DefaultsFile,
    [switch]$PasswordPrompt,
    [string]$MysqlHost,
    [int]$Port,
    [string]$User,
    [string]$Socket,
    [string]$Protocol,
    [string]$Mysql = "mysql",
    [string]$Root,
    [string]$AuthDb = "acore_auth",
    [string]$CharactersDb = "acore_characters",
    [string]$WorldDb = "acore_world",
    [ValidateSet("auth", "characters", "world")]
    [string[]]$Databases = @("auth", "characters", "world"),
    [string]$CharacterSet = "utf8",
    [switch]$SkipChanged,
    [switch]$DryRun
)

$ErrorActionPreference = "Stop"

function Resolve-MysqlClient {
    param([string]$Path)

    $command = Get-Command -Name $Path -CommandType Application -ErrorAction SilentlyContinue
    if ($command) { return $command.Source }
    if (Test-Path -LiteralPath $Path -PathType Leaf) { return (Resolve-Path -LiteralPath $Path).Path }

    $roots = @($env:ProgramFiles, ${env:ProgramFiles(x86)}, $env:ProgramW6432) | Where-Object { $_ }
    foreach ($root in ($roots | Select-Object -Unique)) {
        foreach ($pattern in @("MySQL\MySQL Server *\bin\mysql.exe", "MariaDB *\bin\mysql.exe")) {
            $found = Get-ChildItem -Path (Join-Path $root $pattern) -ErrorAction SilentlyContinue |
                Sort-Object FullName -Descending | Select-Object -First 1
            if ($found) { return $found.FullName }
        }
    }

    throw "mysql client not found. Pass -Mysql with the full path to mysql.exe."
}

function Get-ConnectionArgument {
    param([string]$OptionFile, [string]$Charset = "utf8mb4")

    $arguments = @()
    if ($OptionFile) { $arguments += "--defaults-extra-file=$OptionFile" } else { $arguments += "--no-defaults" }
    if ($MysqlHost) { $arguments += "--host=$MysqlHost" }
    if ($Port) { $arguments += "--port=$Port" }
    if ($User) { $arguments += "--user=$User" }
    if ($Socket) { $arguments += "--socket=$Socket" }
    if ($Protocol) { $arguments += "--protocol=$Protocol" }
    $arguments += @("--batch", "--raw", "--skip-column-names", "--default-character-set=$Charset",
        "--max-allowed-packet=1GB", "--connect-timeout=10")
    return , $arguments
}

function ConvertTo-CommandLine {
    param([string[]]$Arguments)

    $quoted = foreach ($argument in $Arguments) {
        if ($argument -match '[\s"]') { '"' + ($argument -replace '"', '\"') + '"' } else { $argument }
    }
    return ($quoted -join " ")
}

function Invoke-Mysql {
    param(
        [string]$Database,
        [byte[]]$InputBytes,
        [string]$Charset = "utf8mb4",
        [string]$Context
    )

    $connection = Get-ConnectionArgument -OptionFile $script:OptionFile -Charset $Charset
    $arguments = $connection + @("--database=$Database")

    $info = New-Object System.Diagnostics.ProcessStartInfo
    $info.FileName = $script:MysqlClient
    $info.Arguments = ConvertTo-CommandLine -Arguments $arguments
    Write-Verbose "mysql $($info.Arguments)"
    $info.UseShellExecute = $false
    $info.RedirectStandardInput = $true
    $info.RedirectStandardOutput = $true
    $info.RedirectStandardError = $true
    $info.StandardOutputEncoding = [System.Text.Encoding]::UTF8
    $info.StandardErrorEncoding = [System.Text.Encoding]::UTF8

    $process = [System.Diagnostics.Process]::Start($info)
    $process.StandardInput.BaseStream.Write($InputBytes, 0, $InputBytes.Length)
    $process.StandardInput.BaseStream.Flush()
    $process.StandardInput.Close()
    $stdout = $process.StandardOutput.ReadToEnd()
    $stderr = $process.StandardError.ReadToEnd()
    $process.WaitForExit()

    if ($process.ExitCode -ne 0) {
        $where = $Database
        if ($Context) { $where = "$Database, $Context" }
        throw "mysql failed ($where): $($stderr.Trim())"
    }

    return $stdout
}

function Invoke-MysqlQuery {
    param([string]$Database, [string]$Query)

    $bytes = [System.Text.Encoding]::UTF8.GetBytes($Query + "`n")
    $output = Invoke-Mysql -Database $Database -InputBytes $bytes
    $rows = @()
    foreach ($line in ($output -split "`r?`n")) {
        if ($line.Length -gt 0) { $rows += , ($line -split "`t") }
    }
    return $rows
}

function Get-SqlLiteral {
    param([string]$Value)

    return "'" + ($Value -replace '\\', '\\' -replace "'", "\'") + "'"
}

function Get-UpdateHash {
    # UpdateFetcher reads the file through a text-mode ifstream, so Windows folds CRLF to LF
    # before hashing. Latin-1 maps every byte to one character and back without loss.
    param([string]$Path)

    $bytes = [System.IO.File]::ReadAllBytes($Path)
    $latin1 = [System.Text.Encoding]::GetEncoding(28591)
    $text = $latin1.GetString($bytes) -replace "`r`n", "`n"
    $sha1 = [System.Security.Cryptography.SHA1]::Create()
    try {
        $digest = $sha1.ComputeHash($latin1.GetBytes($text))
    } finally {
        $sha1.Dispose()
    }
    return (($digest | ForEach-Object { $_.ToString("X2") }) -join "")
}

function Invoke-PendingDirectory {
    param(
        [string]$Name,
        [string]$Database,
        [string]$Directory
    )

    if (-not (Test-Path -LiteralPath $Directory -PathType Container)) {
        Write-Verbose "No pending directory for $Name : $Directory"
        return
    }

    $files = Get-ChildItem -LiteralPath $Directory -Filter *.sql -File | Sort-Object Name
    $applied = @{}
    $byHash = @{}
    $recorded = Invoke-MysqlQuery -Database $Database -Query "SELECT ``name``, ``hash``, ``state`` FROM ``updates``;"
    foreach ($row in $recorded) {
        $applied[$row[0]] = [pscustomobject]@{ Hash = $row[1]; State = $row[2] }
        if ($row[1]) { $byHash[$row[1]] = $row[0] }
    }
    $present = @{}
    foreach ($file in $files) { $present[$file.Name] = $true }

    foreach ($file in $files) {
        $hash = Get-UpdateHash -Path $file.FullName
        $record = $applied[$file.Name]
        $action = "apply"
        $note = ""

        if ($record) {
            if ($record.Hash -eq $hash) {
                if ($record.State -eq "PENDING") {
                    $action = "skip"
                } else {
                    $action = "state"
                    $note = "$($record.State) -> PENDING"
                }
            } elseif ($SkipChanged) {
                $action = "changed"
                $note = "recorded $($record.Hash.Substring(0, 7)), file $($hash.Substring(0, 7))"
            } else {
                $action = "reapply"
                $note = "recorded $($record.Hash.Substring(0, 7)), file $($hash.Substring(0, 7))"
            }
        } elseif ($byHash.ContainsKey($hash) -and -not $present.ContainsKey($byHash[$hash])) {
            $action = "rename"
            $note = "was $($byHash[$hash])"
        }

        $speed = 0
        if (-not $DryRun) {
            switch ($action) {
                "apply" { $speed = Invoke-SqlFile -Database $Database -Path $file.FullName }
                "reapply" { $speed = Invoke-SqlFile -Database $Database -Path $file.FullName }
                "rename" {
                    $from = Get-SqlLiteral $byHash[$hash]
                    $to = Get-SqlLiteral $file.Name
                    $move = "DELETE FROM ``updates`` WHERE ``name`` = $to;" +
                        " UPDATE ``updates`` SET ``name`` = $to WHERE ``name`` = $from;"
                    Invoke-MysqlQuery -Database $Database -Query $move | Out-Null
                }
            }
            if ($action -eq "apply" -or $action -eq "reapply" -or $action -eq "state") {
                $register = "REPLACE INTO ``updates`` (``name``, ``hash``, ``state``, ``speed``) VALUES (" +
                    (Get-SqlLiteral $file.Name) + ", " + (Get-SqlLiteral $hash) + ", 'PENDING', $speed);"
                Invoke-MysqlQuery -Database $Database -Query $register | Out-Null
            }
        }

        [pscustomobject]@{
            Database = $Name
            File     = $file.Name
            Action   = $action
            Milliseconds = $speed
            Note     = $note
        }
    }
}

function Invoke-SqlFile {
    param([string]$Database, [string]$Path)

    # A file saved by a Windows editor can start with a UTF-8 BOM, which MySQL rejects as syntax.
    $bytes = [System.IO.File]::ReadAllBytes($Path)
    if ($bytes.Length -ge 3 -and $bytes[0] -eq 0xEF -and $bytes[1] -eq 0xBB -and $bytes[2] -eq 0xBF) {
        $bytes = $bytes[3..($bytes.Length - 1)]
    }

    # The server's updater applies files with --default-character-set=utf8 and sets nothing else.
    # A migration that compares a user variable with a utf8mb4_unicode_ci column relies on that:
    # under a utf8mb4 connection the variable carries utf8mb4_0900_ai_ci and MySQL rejects the
    # comparison as an illegal mix of collations.
    $watch = [System.Diagnostics.Stopwatch]::StartNew()
    $name = Split-Path -Leaf $Path
    Invoke-Mysql -Database $Database -InputBytes $bytes -Charset $script:UpdateCharset -Context $name | Out-Null
    $watch.Stop()
    return [int]$watch.Elapsed.TotalMilliseconds
}

if (-not $Root) { $Root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..\..")).Path }
if ($PasswordPrompt -and $DefaultsFile) { throw "Use either -DefaultsFile or -PasswordPrompt." }

$script:MysqlClient = Resolve-MysqlClient -Path $Mysql
$script:UpdateCharset = $CharacterSet
$script:OptionFile = $null
$temporaryOptionFile = $null

try {
    if ($DefaultsFile) {
        $script:OptionFile = (Resolve-Path -LiteralPath $DefaultsFile).Path
    } elseif ($PasswordPrompt) {
        $secure = Read-Host -Prompt "MySQL password" -AsSecureString
        $plain = [System.Runtime.InteropServices.Marshal]::PtrToStringUni(
            [System.Runtime.InteropServices.Marshal]::SecureStringToGlobalAllocUnicode($secure))
        $temporaryOptionFile = [System.IO.Path]::GetTempFileName()
        $content = "[client]`npassword=""" + ($plain -replace '\\', '\\' -replace '"', '\"') + """`n"
        [System.IO.File]::WriteAllText($temporaryOptionFile, $content, (New-Object System.Text.UTF8Encoding($false)))
        $script:OptionFile = $temporaryOptionFile
    }

    $targets = @(
        [pscustomobject]@{ Name = "auth"; Database = $AuthDb; Directory = "data/sql/updates/pending_db_auth" }
        [pscustomobject]@{ Name = "characters"; Database = $CharactersDb
                           Directory = "data/sql/updates/pending_db_characters" }
        [pscustomobject]@{ Name = "world"; Database = $WorldDb; Directory = "data/sql/updates/pending_db_world" }
    )

    $results = @()
    foreach ($target in $targets) {
        if ($Databases -notcontains $target.Name) { continue }
        $directory = Join-Path $Root $target.Directory
        $results += Invoke-PendingDirectory -Name $target.Name -Database $target.Database -Directory $directory
    }

    if ($results) {
        $results | Format-Table -AutoSize | Out-String | Write-Host
    }

    $counts = $results | Group-Object Action | Sort-Object Name
    $summary = ($counts | ForEach-Object { "$($_.Name)=$($_.Count)" }) -join ", "
    if (-not $summary) { $summary = "nothing to do" }
    if ($DryRun) {
        Write-Host "Dry run, nothing was changed: $summary"
    } else {
        Write-Host "Done: $summary"
    }
} finally {
    if ($temporaryOptionFile) { Remove-Item -LiteralPath $temporaryOptionFile -Force -ErrorAction SilentlyContinue }
}
