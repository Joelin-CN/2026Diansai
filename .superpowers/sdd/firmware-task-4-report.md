# Task 4 Report: Install and Validate SysConfig 1.26.2.4477

## Status

PASS

SysConfig 1.26.2 build 4477 was downloaded from the exact approved TI URL, authenticated, installed at the exact required path, and validated. No SysConfig generation was run. No commit was created.

## SDK Requirement Evidence

The required SDK metadata was read from:

`E:\B306\2026\电赛\controller\documents\sdk\.metadata\product.json`

The brief's PowerShell checks completed successfully and reported:

```text
version=2.10.00.04
minToolVersion=1.26.0
cliExists=False
```

Evidence and interpretation:

- SDK product version exactly matched `2.10.00.04`.
- SDK `minToolVersion` was `1.26.0`, which is not greater than the selected SysConfig version `1.26.2`.
- Before download or installation, `C:\ti\sysconfig_1.26.2\sysconfig_cli.bat` did not exist.

## Prior State

- Required CLI before installation: absent.
- Required installer before download: absent.
- `C:\ti` before installation: absent.
- `C:\ti\sysconfig_1.26.2` before installation: absent.
- C-drive free space before installation: `81,550,819,328` bytes (`77,772.92 MiB`), exceeding the 500 MB requirement.
- Current PowerShell process was not elevated. The unattended installation nevertheless completed without a UAC prompt.

The Git worktree was already dirty before this task. Existing modified, untracked, and nested-repository entries were recorded before external installation and were not reverted or modified by this task. The required report itself is the only repository-side artifact intentionally created for Task 4.

## Download Evidence

Approved installer path:

`C:\Users\Joelin\AppData\Local\Temp\opencode\sysconfig-1.26.2_4477-setup.exe`

Exact official URL used:

`https://dr-download.ti.com/software-development/ide-configuration-compiler-or-debugger/MD-nsUM6f7Vvb/1.26.2.4477/sysconfig-1.26.2_4477-setup.exe`

Download result:

- Size: `177,142,592` bytes, approximately `168.94 MiB` or `177.14 MB` decimal.
- SHA-256: `b40e31b01987c68c420ea90bb2095885cd97ee9ab23f2447fa37b4a33933fd31`.
- Required SHA-256 comparison: exact match.
- Authenticode status: `Valid`.
- Signer subject: `CN="Texas Instruments, Inc.", O="Texas Instruments, Inc.", L=Dallas, S=Texas, C=US`.
- Signer issuer: `CN=DigiCert Trusted G4 Code Signing RSA4096 SHA384 2021 CA1, O="DigiCert, Inc.", C=US`.
- Signer certificate expiration: `2027-01-13T07:59:59.0000000+08:00`.

No installer was executed before the SHA-256 and Authenticode checks passed.

## Installer Help and Options Evidence

The installer was first invoked only with `--help`.

Its output identified the release and documented the relevant supported flags:

```text
TI System Configuration Tool 1.26.2+4477
Usage:

 --mode <mode>                               Installation mode
                                             Default: win32
                                             Allowed: win32 unattended

 --prefix <prefix>                           Installation Directory
```

The initial PowerShell call displayed help but did not populate `$LASTEXITCODE`. This was investigated rather than treated as success: the installer is a GUI-subsystem/BitRock-style executable for which direct invocation can return PowerShell control before process completion. A subsequent `Start-Process -Wait -PassThru` invocation with redirected stdout/stderr produced:

```text
waitedExitCode=0
stdoutBytes=1705
stderrBytes=0
```

This confirmed that `--mode unattended` and `--prefix` were installer-supported options before they were used.

## Installation

Installation command semantics:

```powershell
Start-Process `
  -FilePath "C:\Users\Joelin\AppData\Local\Temp\opencode\sysconfig-1.26.2_4477-setup.exe" `
  -ArgumentList @("--mode", "unattended", "--prefix", "C:\ti\sysconfig_1.26.2") `
  -Wait -PassThru
```

The process output was redirected to approved temporary files to capture behavior accurately.

Installation result:

```text
installExitCode=0
stdoutBytes=0
stderrBytes=0
```

- Exact target: `C:\ti\sysconfig_1.26.2`.
- No alternate path or version was used.
- No elevation or UAC interaction was required.
- Installed CLI: `C:\ti\sysconfig_1.26.2\sysconfig_cli.bat`.

## CLI and Release Validation

The exact installed CLI exists and was invoked with `--help`:

```text
Usage:
 cli [--product <file>] [--board <board>] [--device <device>] [--package <package>] [--variant <variant>] [--script <file>] [--output <path>] [--context <context>] [--enableResourceAllocationSetup]
 cli --help
 cli --version
...
cliHelpExitCode=0
```

The CLI was also invoked with `--version`:

```text
1.26.2+4477
cliVersionExitCode=0
```

Installed release metadata at `C:\ti\sysconfig_1.26.2\dist\version.txt` contains:

```text
1.26.2+4477
```

No separately named release-notes or changelog file was present in the installation. Build 4477 was nevertheless independently confirmed by both the installed CLI and the installed `dist\version.txt`. The signed installer's own `--version` output supplied a third confirmation:

```text
TI System Configuration Tool 1.26.2+4477 --- Built on 2026-01-08 17:05:40 IB: 24.11.1-202412121505
```

That installer version process exited `0`, emitted no stderr, and no installer process remained afterward.

## Repository Status

- No firmware source, configuration, generated output, test artifact, SDK content, or other existing repository file was intentionally edited by Task 4.
- No SysConfig generation command was run.
- No commit was created.
- The worktree had substantial pre-existing changes before Task 4. Those entries were left untouched.
- This required report, `.superpowers/sdd/firmware-task-4-report.md`, is the sole intentional repository-side Task 4 artifact and therefore appears as a new untracked file unless separately managed by the orchestration workflow.

## Self-Review and Concerns

- Exact installer filename, official URL, target directory, version, and build all match the brief.
- The installer was not trusted solely by filename or URL: both the exact SHA-256 and a `Valid` Authenticode status were required before execution.
- Installer flags were confirmed from the installer's own help before use.
- The unusual blank `$LASTEXITCODE` from direct installer invocation was resolved by waiting on the process explicitly; installation success is based on the waited process exit code, not the blank value.
- A standalone release-notes file was not installed. This is not a release-validation gap because the CLI, installed `dist\version.txt`, and signed installer banner all identify `1.26.2+4477`.
- The broad Git status cannot literally be clean or globally unchanged because it was dirty before work began and this requested report is new. The relevant assurance is that no pre-existing repository change was altered and no project file was changed by the installation.
- SysConfig generation remains intentionally untested because the brief explicitly prohibits running it during Task 4.
