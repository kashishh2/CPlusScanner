# Part 1: Host-to-VM connection and platform setup

This is the authoritative setup guide for the current Part 1 implementation. It covers both sides of the connection on **macOS, Windows, and Debian Linux**.

- **Host/controller:** the operator's machine. Builds and runs cisscanner-controller.
- **Target/agent:** the VM being checked. Builds and runs cisscanner-agent.

The host and target may use different operating systems. Build the controller natively for the host OS and the agent natively for the target VM OS and CPU architecture. Binaries are not portable between operating systems.

> **Scope:** Part 1 verifies SSH connectivity and the agent's protocol-v1 ping response. It does not run CIS checks, perform remote inventory through the controller, change VM configuration, or remediate anything.

## 1. Architecture

1. The controller starts on the operator's host and invokes the local OpenSSH client.
2. OpenSSH authenticates to the VM with a private key and validates the VM host key.
3. The VM's SSH server launches the short-lived cisscanner-agent --ping --request-id ... process.
4. The agent writes one JSON response to stdout and exits.
5. The controller validates the exact response, protocol version, agent version, operation, and request ID.

The agent is not a daemon or network listener. No inbound agent port is required; only SSH needs to be reachable. The current agent operations do not require root or Administrator privileges.

## 2. Platform support matrix

| Role | macOS | Windows | Debian Linux |
|---|---|---|---|
| Controller build | Supported | Supported | Supported |
| Agent build | Supported | Supported | Supported |
| SSH client | OpenSSH (included) | Windows OpenSSH Client | openssh-client |
| SSH server | Remote Login / sshd | OpenSSH Server | openssh-server |
| C++ toolchain | Apple Clang | Visual Studio 2022 MSVC | GCC or Clang |
| Build system | CMake 3.16+ | CMake 3.16+ | CMake 3.16+ |

Mixed-OS host/target combinations are supported in principle when each binary is built for its execution platform and SSH is configured correctly. CI validates native builds and local agent tests on Windows, macOS, and Ubuntu. It does not validate every mixed-OS SSH pairing. Debian setup is documented here but Debian is not currently a separate CI runner.

## 3. Requirements

### 3.1 Controller host

All host operating systems require:
- C++17 compiler and standard library.
- CMake 3.16 or newer.
- OpenSSH client executable available on PATH as ssh (ssh.exe on Windows).
- Network route to the VM and access to its SSH port (default TCP 22).
- A private key for the target account.
- A trusted VM host-key fingerprint, verified before use.

### 3.2 Target VM

All target operating systems require:
- OpenSSH server enabled and reachable from the controller.
- A dedicated standard (non-root/non-Administrator) account for the initial test.
- The matching public key installed for that account.
- A target-native cisscanner-agent executable installed in a directory on that account's PATH.
- Execute permission for that account.

The agent requires no listening port, persistent service, or elevated privilege for the current ping/system-info operations.

### 3.3 Network

- Permit host-to-VM TCP 22 (or your configured SSH port).
- No VM-to-host connection is required.
- Check routing, VPN, firewalls, cloud security groups, and network ACLs.
- Prefer a private network/VPN and restrict SSH source addresses. Do not expose SSH to untrusted networks for this test.

## 4. Build the controller on the host

Clone the repository on the host machine. Build the controller for the host OS.

### 4.1 macOS host

Requirements: Xcode Command Line Tools (Apple Clang, linker, SDK), CMake 3.16+, and the macOS OpenSSH client.

~~~bash
xcode-select --install
clang++ --version
cmake --version
ssh -V
~~~

If CMake is missing and Homebrew is installed:

~~~bash
brew install cmake
~~~

Build and test:

~~~bash
git clone https://github.com/kashishh2/CPlusScanner.git
cd CPlusScanner
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
~~~

Controller executable: build/cisscanner-controller.

### 4.2 Windows host

Requirements: Windows 10/11 or supported Windows Server, Visual Studio 2022 with **Desktop development with C++** (MSVC, Windows SDK and C++ tools), CMake 3.16+, Git, and OpenSSH Client.

Verify in PowerShell:

~~~powershell
cl
cmake --version
ssh -V
Get-WindowsCapability -Online | Where-Object Name -like 'OpenSSH.Client*'
~~~

If the OpenSSH Client capability is absent, install from elevated PowerShell:

~~~powershell
Add-WindowsCapability -Online -Name OpenSSH.Client~~~~0.0.1.0
~~~

Open **Developer PowerShell for VS 2022** (so MSVC is configured), then:

~~~powershell
git clone https://github.com/kashishh2/CPlusScanner.git
Set-Location CPlusScanner
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
~~~

Controller executable: build\Release\cisscanner-controller.exe. If using a different Visual Studio release, select the corresponding installed CMake generator. Confirm ssh.exe is on PATH in the same environment used to launch the controller.

### 4.3 Debian Linux host

Requirements: Debian 11/12 or newer, GCC or Clang with C++17 support, CMake 3.16+, Git, and OpenSSH client.

~~~bash
sudo apt update
sudo apt install -y build-essential cmake git openssh-client
g++ --version
cmake --version
ssh -V
git clone https://github.com/kashishh2/CPlusScanner.git
cd CPlusScanner
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
~~~

Controller executable: build/cisscanner-controller.

## 5. Build and install the agent on the target VM

Build the agent for the **target VM OS and architecture**, not the host OS. The recommended first-test method is to clone and build directly on the VM. Alternatively, securely copy a binary built for the exact same OS/architecture/ABI.

### 5.1 Debian Linux VM

Install tools and the SSH server:

~~~bash
sudo apt update
sudo apt install -y build-essential cmake git openssh-server
sudo systemctl enable --now ssh
sudo systemctl status ssh --no-pager
~~~

Build only the agent:

~~~bash
git clone https://github.com/kashishh2/CPlusScanner.git
cd CPlusScanner
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target cisscanner-agent --config Release
ctest --test-dir build -R '^agent_' --output-on-failure
~~~

Install it system-wide:

~~~bash
sudo install -o root -g root -m 0755 build/cisscanner-agent /usr/local/bin/cisscanner-agent
command -v cisscanner-agent
cisscanner-agent --ping --request-id agent-local-test
cisscanner-agent --system-info --request-id agent-local-test
~~~

The executable should be root-owned and not writable by the scanner account. The scanner account only needs execute permission.

### 5.2 macOS VM

Requirements: macOS, Xcode Command Line Tools, CMake 3.16+, and Remote Login (sshd).

~~~bash
xcode-select --install
clang++ --version
cmake --version
~~~

Enable **System Settings → General → Sharing → Remote Login** and restrict access to the intended scanner account. On macOS versions supporting systemsetup, this can also be enabled from an administrator shell:

~~~bash
sudo systemsetup -setremotelogin on
sudo systemsetup -getremotelogin
~~~

Build and test:

~~~bash
git clone https://github.com/kashishh2/CPlusScanner.git
cd CPlusScanner
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target cisscanner-agent --config Release
ctest --test-dir build -R '^agent_' --output-on-failure
~~~

Install:

~~~bash
sudo install -o root -g wheel -m 0755 build/cisscanner-agent /usr/local/bin/cisscanner-agent
command -v cisscanner-agent
cisscanner-agent --ping --request-id agent-local-test
cisscanner-agent --system-info --request-id agent-local-test
~~~

If /usr/local/bin does not exist, create it with appropriate administrator ownership. Confirm the SSH account's non-interactive PATH includes /usr/local/bin. Check macOS firewall policy and allowed Remote Login users.

### 5.3 Windows VM

Requirements: Windows 10/11 or supported Windows Server, Visual Studio 2022 with Desktop development with C++, CMake 3.16+, Git, and OpenSSH Server.

Install and start OpenSSH Server from elevated PowerShell:

~~~powershell
Add-WindowsCapability -Online -Name OpenSSH.Server~~~~0.0.1.0
Start-Service sshd
Set-Service -Name sshd -StartupType Automatic
Get-Service sshd
Get-NetFirewallRule -Name OpenSSH-Server-In-TCP
~~~

If the firewall rule is missing, create an inbound TCP rule restricted to the controller network rather than opening SSH broadly.

Open **Developer PowerShell for VS 2022** and build:

~~~powershell
git clone https://github.com/kashishh2/CPlusScanner.git
Set-Location CPlusScanner
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target cisscanner-agent
ctest --test-dir build -C Release -R "^agent_" --output-on-failure
~~~

Install (run PowerShell as Administrator):

~~~powershell
New-Item -ItemType Directory -Force -Path 'C:\Program Files\CPlusScanner'
Copy-Item 'build\Release\cisscanner-agent.exe' 'C:\Program Files\CPlusScanner\cisscanner-agent.exe'
~~~

Add C:\Program Files\CPlusScanner to the **system PATH** using Windows Environment Variables. Restart sshd so new SSH sessions inherit the updated environment:

~~~powershell
Restart-Service sshd
~~~

Local test:

~~~powershell
& 'C:\Program Files\CPlusScanner\cisscanner-agent.exe' --ping --request-id agent-local-test
& 'C:\Program Files\CPlusScanner\cisscanner-agent.exe' --system-info --request-id agent-local-test
~~~

The SSH session must resolve and execute cisscanner-agent.exe by name. Verify using a non-interactive SSH command after key setup. Use a dedicated standard user for the first test. Windows OpenSSH has a special authorized-key path and ACL for accounts in the local Administrators group (C:\ProgramData\ssh\administrators_authorized_keys); do not apply the standard-user ACL recipe to an administrator account.

## 6. Configure SSH authentication and trust

Perform key generation on the controller host. Keep the private key on the host; only copy the public key to the VM.

### 6.1 Generate a dedicated Ed25519 key

macOS/Debian host:

~~~bash
ssh-keygen -t ed25519 -a 64 -f ~/.ssh/cisscanner_ed25519 -C "cisscanner-part1"
~~~

Windows PowerShell:

~~~powershell
ssh-keygen -t ed25519 -a 64 -f "$env:USERPROFILE\.ssh\cisscanner_ed25519" -C "cisscanner-part1"
~~~

Protect the private key. Never commit it to Git or copy it to the target VM.

### 6.2 Debian/macOS target: install public key

Log in using an approved initial administrative method, then run as the target scanner account:

~~~bash
mkdir -p ~/.ssh
chmod 700 ~/.ssh
cat >> ~/.ssh/authorized_keys
~~~

Paste the single-line contents of the controller's cisscanner_ed25519.pub, then press Ctrl-D. Set ownership and permissions:

~~~bash
chmod 600 ~/.ssh/authorized_keys
chown -R "$USER" ~/.ssh
~~~

Ensure the SSH server permits public-key authentication and the scanner account can log in.

### 6.3 Windows target: install public key for a standard user

For a dedicated standard local user named scanner, create C:\Users\scanner\.ssh\authorized_keys and paste the one-line public key into it. Run the following from elevated PowerShell, replacing the account and home path as appropriate:

~~~powershell
$account = "$env:COMPUTERNAME\scanner"
$sshDir = "C:\Users\scanner\.ssh"
New-Item -ItemType Directory -Force -Path $sshDir
notepad "$sshDir\authorized_keys"
icacls $sshDir /inheritance:r
icacls $sshDir /grant:r "$($account):(OI)(CI)F" "SYSTEM:(OI)(CI)F" "Administrators:(OI)(CI)F"
icacls "$sshDir\authorized_keys" /inheritance:r
icacls "$sshDir\authorized_keys" /grant:r "$($account):F" "SYSTEM:F" "Administrators:F"
Restart-Service sshd
~~~

Use the correct local or domain-qualified account identity. Windows OpenSSH requires restrictive ACLs. If the account is an administrator, use the special administrators_authorized_keys location and its required ACL instead; a standard-user recipe is not appropriate.

### 6.4 Verify host-key fingerprint

Obtain the VM's SSH host-key fingerprint through a trusted channel such as the hypervisor console, cloud serial console, or VM administrator. Compare it with the fingerprint presented by SSH (or obtained from the VM's host-key files) before accepting it into the controller user's known_hosts.

Do not blindly trust an unknown key and do not disable StrictHostKeyChecking. A changed host key must be investigated before updating known_hosts.

### 6.5 Test SSH before using CPlusScanner

macOS/Debian host:

~~~bash
ssh -i ~/.ssh/cisscanner_ed25519 -o BatchMode=yes <scanner-user>@<vm-address> true
ssh -i ~/.ssh/cisscanner_ed25519 -o BatchMode=yes <scanner-user>@<vm-address> cisscanner-agent --ping --request-id ssh-manual-test
~~~

Windows PowerShell host:

~~~powershell
ssh -i "$env:USERPROFILE\.ssh\cisscanner_ed25519" -o BatchMode=yes <scanner-user>@<vm-address> true
ssh -i "$env:USERPROFILE\.ssh\cisscanner_ed25519" -o BatchMode=yes <scanner-user>@<vm-address> cisscanner-agent --ping --request-id ssh-manual-test
~~~

The second command must produce one JSON object on stdout. Shell/profile banners or other stdout text can break the strict protocol. Keep login notices off stdout for the dedicated scanner account.

## 7. Run the controller

macOS/Debian host:

~~~bash
./build/cisscanner-controller \
  --host <VM_IP_OR_DNS> \
  --user <SSH_USER> \
  --identity ~/.ssh/cisscanner_ed25519 \
  --port 22 \
  --timeout 20
~~~

Windows PowerShell host:

~~~powershell
.\build\Release\cisscanner-controller.exe --host <VM_IP_OR_DNS> --user <SSH_USER> --identity "$env:USERPROFILE\.ssh\cisscanner_ed25519" --port 22 --timeout 20
~~~

Optional arguments:
- --port: SSH TCP port (default 22).
- --known-hosts: explicit known_hosts file path.
- --timeout: timeout in seconds (1–300).

The controller enforces batch public-key authentication, disables password and keyboard-interactive authentication, requires strict host-key verification, disables forwarding and connection multiplexing, and enables SSH keepalives. Its overall process deadline includes five seconds beyond the configured SSH timeout.

Expected successful output:

~~~text
Connection successful. Protocol 1, agent 0.1.0, request req-...
~~~

## 8. Troubleshooting

| Symptom | Checks |
|---|---|
| ssh command not found | Install/enable OpenSSH client and ensure ssh/ssh.exe is on controller PATH. |
| Connection timed out | Check VM state, address, route/VPN, TCP port, firewall/security group, and SSH server status. |
| Connection refused | SSH server is not listening, wrong port, or firewall is rejecting the connection. |
| Permission denied (publickey) | Check username, public key path, private key selection/permissions, and server public-key authentication. |
| Host key verification failed | Verify the fingerprint through a trusted channel; investigate rebuild/key rotation before updating known_hosts. |
| Agent not found | Check install path, PATH in non-interactive SSH sessions, and execute permissions. Restart Windows sshd after changing system PATH. |
| Works locally but not over SSH | Run the exact non-interactive SSH agent command; inspect PATH, shell startup output, ACLs, and binary architecture. |
| Protocol response mismatch | Ensure stdout contains only the agent JSON line and both binaries use compatible protocol versions. |
| macOS Remote Login failure | Check Sharing → Remote Login, allowed users, firewall, and network reachability. |
| Windows SSH login failure | Check Get-Service sshd, OpenSSH event logs, firewall, account policy, authorized_keys path and ACLs. |

## 9. Failure handling and exit codes

The controller and agent have top-level exception boundaries. Allocation failures and unexpected standard/unknown exceptions produce diagnostics and non-zero exits. The controller bounds responses to 64 KiB, enforces a process deadline, checks process exit status, and rejects an SSH process that exits without closing its output pipe. Windows process wait failures are distinguished from timeouts. SSH client diagnostics are emitted to stderr; CPlusScanner does not yet classify every OpenSSH failure into a machine-readable error category.

These safeguards reduce hangs and uncontrolled failures. They cannot guarantee recovery from power loss, OS termination, hardware faults, or every network failure.

### Controller exit codes

| Code | Meaning |
|---:|---|
| 0 | SSH connection and protocol handshake succeeded |
| 1 | SSH/process/connection failure |
| 2 | Invalid controller arguments or agent invocation |
| 3 | Agent response did not match expected protocol |
| 70 | Unexpected controller exception |

### Agent exit codes

| Code | Meaning |
|---:|---|
| 0 | Operation succeeded |
| 2 | Invalid or unsupported arguments |
| 4 | Agent could not write its response |
| 70 | Unexpected agent exception |

## 10. Validation status and limitations

- GitHub Actions on the Part 1 source revision documented in the Captain's Log completed configure, build, and CTest successfully on windows-latest, macos-latest, and ubuntu-latest.
- CI includes local agent ping/system-info tests and invalid-argument tests. It does **not** run real SSH sessions against provisioned VMs.
- Real end-to-end SSH testing on target VMs remains outstanding.
- The controller currently validates ping only. Agent --system-info can be run locally but is not yet requested/consumed by the controller.
- Deployment, key provisioning, retry policy, persistent structured logs, remote inventory, CIS evaluation, privileged collection, remediation, and configuration writes are not implemented.
- Start with one host, one VM, a standard account, a trusted private network, and a verified host key.

## 11. Part 2 entry point

After Test 1 succeeds, begin Part 2 with remote system inventory:

1. Add a controller operation that requests --system-info over SSH.
2. Parse JSON structurally instead of relying on exact string comparison as the response grows.
3. Validate protocol version, agent version, request ID, OS family, architecture, and hostname.
4. Add OS release/version information (Debian release, macOS version, Windows product/build).
5. Define explicit unsupported-platform and partial-inventory error states.
6. Add integration tests using disposable Debian, macOS (where available), and Windows VMs before implementing CIS collectors.

For the full project state and next-chat handoff, read CAPTAINS_LOG_PART_1.md.
