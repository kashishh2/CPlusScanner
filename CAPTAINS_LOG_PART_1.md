# CPlusScanner — Captain's Log / Part 1 Handoff

**Purpose:** Persistent project handoff for continuing CPlusScanner in a new ChatGPT conversation. Read this file first, then inspect the current repository before making code changes.

**Last updated:** 2026-11-26  
**Repository:** https://github.com/kashishh2/CPlusScanner  
**Owner:** kashishh2  
**Default branch:** main  
**Project phase:** Part 1 — SSH host-to-VM connectivity foundation  
**Next phase:** Part 2 — remote system inventory

---

## 1. Mission and product intent

Build a cross-platform C++-based VM security configuration scanner that assesses virtual machines against selected CIS Benchmark controls.

The intended model is:

- A **controller** runs on the operator's workstation or management host.
- A lightweight **agent** is installed on each target VM.
- The controller connects to the agent over SSH.
- The agent performs local, bounded assessment/inventory operations and returns structured evidence.
- The controller validates results and eventually generates auditable CIS assessment reports.

The initial implementation is deliberately connection-first. Do not start writing CIS checks until the SSH connection and protocol are dependable.

### Non-negotiable design principles

1. Read-only assessment by default. No automatic remediation or configuration mutation.
2. Least privilege. Use a dedicated non-root/non-Administrator account wherever possible.
3. SSH public-key authentication and strict host-key verification.
4. The agent is a short-lived executable, not a listening service or persistent daemon.
5. The controller exposes a narrow, versioned operation interface. Do not implement arbitrary remote shell command execution.
6. Structured evidence and explicit errors. Missing evidence must never be treated as a pass.
7. Every future CIS rule must retain benchmark version, recommendation identifier, profile/applicability, evidence, and evaluation result.
8. Keep changes segmented and focused. The project owner prefers short, precise steps and a Captain's Log at the end of work sessions.

---

## 2. Current architecture

### Controller

- Source: controller/main.cpp
- Executable: cisscanner-controller (Windows: cisscanner-controller.exe)
- Runs on the host.
- Invokes the local OpenSSH client.
- Uses SSH public-key authentication.
- Requires StrictHostKeyChecking=yes.
- Disables password and keyboard-interactive authentication.
- Disables SSH forwarding and connection multiplexing.
- Uses connection timeout, keepalive settings, and a bounded process deadline.
- Captures stdout from the SSH process and validates the response.
- Current remote operation: cisscanner-agent --ping --request-id <id>.
- Current controller success means SSH process succeeded and exact protocol-v1 ping response matched.

### Agent

- Source: agent/main.cpp
- Executable: cisscanner-agent (Windows: cisscanner-agent.exe)
- Runs on the target VM.
- Launched by the SSH server per operation and exits immediately.
- Supports:
  - --ping [--request-id ID]
  - --system-info [--request-id ID]
- System-info returns OS family, architecture, and hostname.
- Does not currently collect CIS settings or require elevated privileges.
- Must be installed in the SSH account's PATH so the remote command can invoke it by name.

### Shared protocol

- Source: include/protocol.hpp
- Protocol version: 1
- Agent version: 0.1.0
- JSON output is emitted on stdout; diagnostics go to stderr.
- Controller currently validates the ping JSON by exact string comparison. This is acceptable for the tiny handshake but must be replaced with structural JSON parsing as response complexity increases.

### Build system and CI

- CMake minimum: 3.16.
- C++ standard: C++17, extensions disabled.
- Threads package is found and linked for the controller.
- CI matrix: windows-latest, macos-latest, ubuntu-latest.
- CTest covers agent ping, agent system-info, invalid agent arguments, and controller missing-argument rejection.

---

## 3. Platform setup and operator guide

The authoritative detailed instructions are in **CONNECTION.md**. Read it before providing setup instructions or changing deployment assumptions.

It covers both host/controller and target/agent setup for:
- macOS
- Windows
- Debian Linux

### Host/controller prerequisites

| Host OS | Toolchain/build requirements | SSH client |
|---|---|---|
| macOS | Xcode Command Line Tools / Apple Clang, CMake 3.16+ | macOS OpenSSH |
| Windows | Visual Studio 2022 Desktop development with C++, MSVC, Windows SDK, CMake 3.16+ | Windows OpenSSH Client |
| Debian | build-essential (GCC), CMake 3.16+, Git | openssh-client |

### Target/agent prerequisites

| Target OS | SSH server | Agent build/install |
|---|---|---|
| macOS | Remote Login / sshd | Apple Clang + CMake; install to /usr/local/bin |
| Windows | OpenSSH Server (sshd service) | MSVC + CMake; install to C:\Program Files\CPlusScanner and add to system PATH |
| Debian | openssh-server | GCC + CMake; install to /usr/local/bin |

### Mixed operating systems

Host and VM do not have to use the same OS. Each executable must be built for the OS and architecture on which it will run. The simplest initial workflow is to build the controller on the host and build the agent directly on the VM.

### SSH and network assumptions

- Only host-to-VM SSH TCP port 22 (or configured SSH port) is required.
- No inbound port for the agent.
- No VM-to-host callback.
- Generate an Ed25519 key on the host; keep private key on host; install public key on target.
- Verify VM host-key fingerprint through a trusted channel before accepting it.
- Use a dedicated standard account for the initial test.
- Agent must resolve by name in the non-interactive SSH session.
- Do not allow shell startup banners or other text on stdout; controller expects exact JSON response.

See CONNECTION.md for PowerShell and shell commands, service setup, PATH requirements, Windows authorized_keys ACL notes, build commands, SSH tests, and troubleshooting.

---

## 4. Current source and repository files

| Path | Purpose |
|---|---|
| agent/main.cpp | Target-side agent operations and JSON output |
| controller/main.cpp | Host-side CLI, OpenSSH launch, process supervision, handshake validation |
| include/protocol.hpp | Protocol and JSON helpers |
| CMakeLists.txt | C++17 targets, Threads, warnings, CTest definitions |
| .github/workflows/build.yml | Windows/macOS/Ubuntu native CI matrix |
| CONNECTION.md | Detailed macOS/Windows/Debian host and target setup |
| README.md | Project summary, scope, principles, roadmap |
| CIS standards/README.md | Benchmark catalogue metadata and official references |
| CIS standards/Linux.md | Linux CIS reference/notes |
| CIS standards/Windows Desktop.md | Windows Desktop CIS reference/notes |

Do not assume that files or behavior not listed here are implemented; inspect current main branch first.

---

## 5. Error handling and resilience already implemented

The recent Part 1 hardening pass added or retained:

- Top-level exception handling around controller and agent entry points.
- Explicit handling for std::bad_alloc, std::exception, and unknown exceptions.
- Non-zero exit code 70 for unexpected exceptions.
- Agent stdout write-state check; exit code 4 if response cannot be written.
- Strict argument validation and bounded numeric parsing.
- SSH public-key-only non-interactive configuration.
- Strict host-key checking.
- SSH keepalive and connection/process timeouts.
- A 64 KiB response size bound.
- POSIX process supervision using fork/exec, nonblocking pipe reads, poll, waitpid, and forced child termination on deadline.
- Windows CreateProcessW, pipe reader thread, process wait checks, timeout termination, and reader shutdown handling.
- Rejects a process that exits while the output pipe remains open past the deadline.
- Negative-path CLI smoke tests.

### Exit codes

Controller:
- 0 — SSH connection and protocol handshake succeeded.
- 1 — SSH/process/connection failure.
- 2 — invalid controller arguments or agent invocation.
- 3 — agent response did not match expected protocol.
- 70 — unexpected controller exception.

Agent:
- 0 — operation succeeded.
- 2 — invalid or unsupported arguments.
- 4 — agent response write failed.
- 70 — unexpected agent exception.

### Known error-handling limitations

- SSH stderr is emitted by the OpenSSH client, but the controller does not yet parse every OpenSSH failure into a structured machine-readable category.
- No structured persistent log file or correlation log exists yet.
- No automatic retry policy exists. Do not retry auth failures or host-key failures blindly.
- There is no guarantee against power loss, OS termination, hardware failure, or every network failure.
- Windows process launch and shutdown still deserve focused integration testing on a real Windows host and Windows VM.
- The current protocol parser is exact string matching for ping; move to a real JSON parser before expanding protocol payloads.
- Do not claim real VM connectivity has been tested until an actual SSH test against a provisioned VM is performed.

---

## 6. Changes made in the recent work

Earlier Part 1 work included:
- Cross-platform C++17 controller and agent.
- SSH connection with public-key authentication.
- Protocol v1 ping handshake.
- Agent system-info operation (local invocation only from the current controller).
- CMake and cross-platform GitHub Actions.
- Security defaults: StrictHostKeyChecking, BatchMode, IdentitesOnly, disabled password auth, no forwarding/multiplexing, keepalives.
- POSIX poll-based pipe handling and process timeout logic.
- Windows process launch and output reader.
- Input validation, request IDs, response bounds, docs.

Recent exception/error hardening included:
- Added controller top-level exception boundaries.
- Added agent top-level exception boundaries and output-write checking.
- Improved Windows process wait diagnostics and reader cancellation/cleanup.
- Improved POSIX timeout and lingering output-pipe detection.
- Preallocated bounded response storage before process resources are created, reducing exception/resource-leak risk.
- Added invalid-argument CTest cases.
- Updated connection failure documentation.

Recent source commits in chronological order:
- 1e3cf1f2116ffee783b613841b45350e1ce77020 — add portable Threads dependency.
- 7948434662a09e48c5b0e6f9d1e16c1cde56af10 — explicit cstddef include.
- acd9744a5c3560476ae3d1b9a2aefa3be76251d8 — normalize agent standard includes.
- Additional exception/error hardening commits followed.
- 57db15fa51a5a327379f9d5a1f0e948614e9512f — intermediate resource-allocation refactor; its CI run failed, so do not treat it as a validated state.
- 959b1bf544900fce5bbc7423e60018d73292937b — corrected Windows invalid UTF-8 argument handling/cleanup.
- 56cd1bbb93363169a9bb7cf80c8e9cf8b22375fc — normalized Windows diagnostic newline escape; this is the source revision whose CI matrix completed successfully before the documentation-only updates in this handoff.

Documentation commits in this handoff:
- d5582d1b7c5cc7a5c61aae22dd34593499516661 — expand CONNECTION.md with exact host and agent setup for macOS, Windows, Debian.
- aeadf5dfb7677e72d2620afbd3fea7045c16289b — link setup guide and Captain's Log from README.
- This log is created as a separate repository file in the current handoff commit.

The latest main branch head should be checked in GitHub before the next session; it will include this Captain's Log commit.

---

## 7. CI and validation state

**Verified GitHub Actions run:**  
https://github.com/kashishh2/CPlusScanner/actions/runs/45866077681

Run ID: 45866077681  
Source commit: 56cd1bbb93363169a9bb7cf80c8e9cf8b22375fc  
Result: completed successfully.

Jobs:
- build (macos-latest): success
- build (ubuntu-latest): success
- build (windows-latest): success

The workflow performs:
1. Checkout.
2. CMake configure.
3. CMake build.
4. CTest agent and CLI smoke tests.

**Important distinction:** This proves native CI build and local tests on the three GitHub runner platforms. It does NOT prove real SSH connectivity between physical/virtual machines. No real end-to-end VM connection has yet been reported as successful.

A prior intermediate commit CI run failed. The later source revision listed above passed all three matrix jobs. Always inspect current CI after new code changes.

---

## 8. Current milestone and acceptance criteria

### Part 1 — Connection milestone

Goal: one host connects to one target VM and completes a protocol-v1 agent ping.

Acceptance criteria:
1. Controller builds and runs on the selected host OS.
2. Agent builds and runs on target VM OS/architecture.
3. SSH server is enabled on VM and reachable from host.
4. Public-key authentication succeeds non-interactively.
5. Host key is verified; StrictHostKeyChecking remains enabled.
6. Remote account can execute cisscanner-agent from PATH.
7. Agent returns exactly one valid JSON ping response on stdout.
8. Controller validates response and exits 0.
9. Test at least one failure path (wrong key, unreachable host, or missing agent) and confirm clean non-zero failure without hanging.

Do not mark Part 1 end-to-end complete until the above is run against a real VM and the OS/versions and result are recorded.

---

## 9. Part 2 — Next work plan

Once the first real host-to-VM ping succeeds, start Part 2 with remote system inventory.

Recommended sequence:

### Step 1 — Capture the actual test environment
Record:
- Controller host OS and version.
- Host architecture.
- Target VM OS and version.
- Target architecture.
- OpenSSH client/server versions.
- Network path/port (do not store secrets or private IPs if the repo is public).
- Test result and sanitized diagnostics.

### Step 2 — Extend protocol to remote system-info
- Add a controller option/operation to invoke agent --system-info.
- Preserve request ID and protocol version validation.
- Use a JSON parser (choose a small, well-maintained dependency or a carefully scoped parser; do not keep extending string matching).
- Validate types, required fields, bounds, unknown/missing values, and malformed JSON.
- Ensure stdout is protocol-only and stderr carries diagnostics.
- Add OS release/version/build details:
  - Debian: /etc/os-release fields, kernel release, architecture.
  - macOS: product version/build and architecture.
  - Windows: product name/version/build and architecture.
- Do not collect sensitive data unrelated to inventory.

### Step 3 — Error contract
Define structured errors for:
- Controller configuration/argument failure.
- SSH network/timeout failure.
- SSH authentication failure.
- Host-key verification failure.
- Remote agent missing/not executable.
- Agent operation failure.
- Protocol mismatch/malformed JSON.
- Unsupported platform.
- Partial system inventory.

Do not invent retries. Define a small retry policy only for clearly transient network errors, with bounded attempts and backoff. Never retry authentication or host-key verification failures automatically.

### Step 4 — Tests
- Unit tests for protocol parsing and validation.
- Agent tests on supported operating systems.
- Controller tests for malformed/oversized responses and process failures.
- Integration tests against disposable Debian, Windows, and macOS VMs where available.
- Test network interruption, invalid key, changed host key, missing agent, wrong protocol version, non-zero agent exit, and timeout.

### Step 5 — Only after inventory
Design the CIS assessment rule schema, evidence model, OS-specific collectors, benchmark version mapping, applicability, and report output. Start with a small set of read-only checks and test them against known-good/known-bad VM configurations.

---

## 10. What not to do in Part 2

- Do not jump directly into hundreds of CIS checks.
- Do not add privileged access or sudo requirements without a concrete check that requires it.
- Do not turn the agent into a listening daemon.
- Do not add arbitrary remote command execution.
- Do not silently accept changed/unknown SSH host keys.
- Do not treat collection errors or missing evidence as Pass.
- Do not store private keys, credentials, production hostnames, or sensitive VM inventory in this public repository.
- Do not claim all mixed-OS combinations are validated based solely on native CI builds.

---

## 11. Instructions for the next ChatGPT session

The user will point the next chat to this file. When that happens:

1. Read this entire file first.
2. Open the repository and inspect the current main branch, especially README.md, CONNECTION.md, controller/main.cpp, agent/main.cpp, include/protocol.hpp, CMakeLists.txt, and the workflow.
3. Check the latest commit and CI status; documentation commits may have occurred after the last validated source commit.
4. Ask for or retrieve the result of the first real host-to-VM Test 1 if the user has completed it. Do not assume it succeeded.
5. Continue with Part 2 only after confirming the real connection test state.
6. Keep the work segmented and commit coherent changes directly to the repository when the user asks.
7. At the end of the next session, update this Captain's Log with new commits, validation, decisions, issues, and next steps.

**Suggested opening instruction for the next chat:**

"Read CAPTAINS_LOG_PART_1.md in https://github.com/kashishh2/CPlusScanner and continue from the recorded state. First verify the current main branch and CI. We are moving toward Part 2 (remote system inventory), but confirm whether the first real host-to-VM SSH test has been completed before changing code."
