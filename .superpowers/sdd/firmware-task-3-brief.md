### Task 3: Correct the EIDE SysConfig Pre-Build Command

**Files:**
- Modify: `.eide/eide.yml:66-76`
- Reference: `.vscode/tasks.json:23-44`

**Interfaces:**
- Consumes: EIDE build-task fields `name`, `command`, and `stopBuildAfterFailed`; built-in variable `${ProjectRoot}`.
- Produces: one complete SysConfig command in `builder.params.options.beforeBuildTasks[0].command`.

- [ ] **Step 1: Record the malformed task shape**

Run:

```powershell
$text = Get-Content -Raw -LiteralPath ".eide\eide.yml"
if ($text -notmatch '(?m)^\s+args:\s*>-') { throw "Expected the unsupported args field" }
if ($text -notmatch '\$\{WorkspaceRoot\}') { throw "Expected the unsupported WorkspaceRoot variable" }
```

Expected: command exits `0`, proving both incompatible constructs are present before the edit.

- [ ] **Step 2: Put the executable and arguments in one supported command**

Replace the existing task with:

```yaml
          beforeBuildTasks:
            - name: Generate SysConfig
              command: >-
                C:/ti/sysconfig_1.26.2/sysconfig_cli.bat
                --product "${ProjectRoot}/../../controller/documents/sdk/.metadata/product.json"
                --device MSPM0G3507
                --package "LQFP-48(PT)"
                --script "${ProjectRoot}/NewProject1.syscfg"
                --output "${ProjectRoot}/Debug"
                --compiler keil
              stopBuildAfterFailed: true
```

Do not change `.vscode/tasks.json`; its array-based task already passes its arguments correctly.

- [ ] **Step 3: Verify the unsupported task constructs are gone**

Run:

```powershell
$text = Get-Content -Raw -LiteralPath ".eide\eide.yml"
if ($text -match '(?m)^\s+args:\s*>-') { throw "Unsupported args field remains" }
if ($text -match '\$\{WorkspaceRoot\}') { throw "Unsupported WorkspaceRoot remains" }
if ($text -notmatch '\$\{ProjectRoot\}') { throw "ProjectRoot was not configured" }
if ($text -notmatch 'stopBuildAfterFailed:\s*true') { throw "Pre-build failure gate was removed" }
```

Expected: command exits `0`.

- [ ] **Step 4: Review the task diff**

Run:

```powershell
git diff -- .eide/eide.yml
git diff --check -- .eide/eide.yml
```

Expected: only the SysConfig pre-build task shape changes and `git diff --check` exits `0`.

---

