### Task 4: Install and Validate SysConfig 1.26.2.4477

**Files:**
- External install: `C:/ti/sysconfig_1.26.2/`
- Download outside workspace: `C:/Users/Joelin/AppData/Local/Temp/opencode/sysconfig-1.26.2_4477-setup.exe`
- Reference: `../../controller/documents/sdk/.metadata/product.json:1-24`

**Interfaces:**
- Consumes: TI's signed Windows installer and 500 MB free disk space.
- Produces: `C:/ti/sysconfig_1.26.2/sysconfig_cli.bat` and SysConfig version `1.26.2.4477`.

- [ ] **Step 1: Confirm the exact SDK/tool requirement and current absence**

Run:

```powershell
$product = Get-Content -Raw -LiteralPath "..\..\controller\documents\sdk\.metadata\product.json" | ConvertFrom-Json
if ($product.version -ne "2.10.00.04") { throw "Unexpected SDK version: $($product.version)" }
if ([version]$product.minToolVersion -gt [version]"1.26.2") { throw "SysConfig 1.26.2 is too old" }
Test-Path -LiteralPath "C:\ti\sysconfig_1.26.2\sysconfig_cli.bat"
```

Expected: SDK version check passes and `Test-Path` initially returns `False`.

- [ ] **Step 2: Download the official Windows installer**

Verify the approved temporary parent exists, then download:

```powershell
$temp = "C:\Users\Joelin\AppData\Local\Temp\opencode"
if (-not (Test-Path -LiteralPath $temp)) { throw "Approved temp parent is missing" }
$installer = Join-Path $temp "sysconfig-1.26.2_4477-setup.exe"
Invoke-WebRequest `
  -Uri "https://dr-download.ti.com/software-development/ide-configuration-compiler-or-debugger/MD-nsUM6f7Vvb/1.26.2.4477/sysconfig-1.26.2_4477-setup.exe" `
  -OutFile $installer
```

Expected: download completes and the file size is approximately 173 MB.

- [ ] **Step 3: Verify the installer checksum and signature**

Run:

```powershell
$installer = "C:\Users\Joelin\AppData\Local\Temp\opencode\sysconfig-1.26.2_4477-setup.exe"
$hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $installer).Hash.ToLowerInvariant()
if ($hash -ne "b40e31b01987c68c420ea90bb2095885cd97ee9ab23f2447fa37b4a33933fd31") {
    throw "SysConfig installer checksum mismatch: $hash"
}
$signature = Get-AuthenticodeSignature -FilePath $installer
if ($signature.Status -ne "Valid") { throw "Invalid installer signature: $($signature.Status)" }
```

Expected: checksum matches TI's published value and the Authenticode status is `Valid`.

- [ ] **Step 4: Inspect installer-supported command-line options**

Run:

```powershell
& "C:\Users\Joelin\AppData\Local\Temp\opencode\sysconfig-1.26.2_4477-setup.exe" --help
```

Expected: installer help is displayed. Confirm it supports `--prefix` and unattended mode before using those options. If it does not, run the installer interactively and select `C:\ti\sysconfig_1.26.2`; do not guess unsupported flags.

- [ ] **Step 5: Install to the configured path**

If Step 4 confirms the BitRock-style options, run:

```powershell
$parent = "C:\ti"
if (-not (Test-Path -LiteralPath "C:\")) { throw "C drive is unavailable" }
if (-not (Test-Path -LiteralPath $parent)) {
    New-Item -ItemType Directory -Path $parent | Out-Null
}
& "C:\Users\Joelin\AppData\Local\Temp\opencode\sysconfig-1.26.2_4477-setup.exe" `
  --mode unattended `
  --prefix "C:\ti\sysconfig_1.26.2"
if ($LASTEXITCODE -ne 0) { throw "SysConfig installer failed: $LASTEXITCODE" }
```

Expected: installer exits `0`. If elevation is required, stop and request the user to approve the Windows elevation prompt rather than changing the install root.

- [ ] **Step 6: Validate the installed CLI and release**

Run:

```powershell
$cli = "C:\ti\sysconfig_1.26.2\sysconfig_cli.bat"
if (-not (Test-Path -LiteralPath $cli)) { throw "SysConfig CLI was not installed at the configured path" }
& $cli --help
if ($LASTEXITCODE -ne 0) { throw "SysConfig CLI validation failed" }
```

Expected: `sysconfig_cli.bat` exists, starts successfully, and reports SysConfig 1.26.2/4477 in its help or banner. If the banner omits the build number, confirm the installation's release notes identify `1.26.2_4477`.

---

