# Part 1: Host-to-VM connection

Part 1 establishes authenticated SSH connectivity and verifies that the CPlusScanner agent can start on the target VM. It performs **no CIS checks** and makes no VM configuration changes.

## Architecture

The host controller launches the local OpenSSH client. SSH authenticates the account using a private key and verifies the VM host key. The SSH server starts a short-lived `cisscanner-agent --ping` process on the VM. The agent returns one JSON response; the controller validates the protocol version and request ID.

The agent is not a network listener or persistent daemon. It exposes fixed operations and does not accept arbitrary commands from CPlusScanner.

## Requirements and build

- C++17 compiler
- CMake 3.16 or newer
- OpenSSH client on the controller host
- OpenSSH server on the target VM

```sh
cmake -S . -B build
cmake --build build --config Release
```

Build the agent for the target VM's OS and CPU architecture. Build the controller for the operator's host. Cross-platform means source builds are supported on Windows, Linux, and macOS; a binary is not portable across operating systems.

## Install the agent

Copy the target-platform `cisscanner-agent` executable to a directory in the SSH account's `PATH`. On Windows, install and start OpenSSH Server and ensure the SSH account can execute the agent. On Linux, install and enable the distribution's OpenSSH server.

The agent is launched per request and exits immediately. It requires no listening port, background service, or root privileges for these initial operations.

## SSH trust setup

1. Use a dedicated SSH account on the VM.
2. Configure public-key authentication and install the matching public key for that account.
3. From the controller host, connect once and verify the VM host-key fingerprint through a trusted channel before accepting it into `known_hosts`.
4. Confirm non-interactive login: `ssh -i <private-key> <user>@<vm-host> true`.
5. Restrict private-key file permissions. Do not disable host-key checking to bypass trust errors.

## Run the connection check

```sh
cisscanner-controller --host 192.0.2.10 --user scanner --identity ~/.ssh/cisscanner_ed25519
```

Optional arguments: `--port 22`, `--known-hosts <file>`, and `--timeout 15` (seconds, 1–300). The controller enforces batch public-key authentication and `StrictHostKeyChecking=yes`. It also disables SSH connection multiplexing and forwarding, uses one connection attempt, and enables server-alive probes to detect stalled sessions. An unknown or changed host key fails safely. The agent must be named `cisscanner-agent` and available in the SSH account's `PATH`.

Local agent smoke tests:

```sh
cisscanner-agent --ping --request-id req-local-test
cisscanner-agent --system-info --request-id req-local-test
```

## Protocol v1

Each operation writes one JSON object to standard output; diagnostics go to standard error. Example ping response:

```json
{"protocol_version":"1","agent_version":"0.1.0","status":"ok","operation":"ping","request_id":"req-0123456789abcdef"}
```

The agent's `--system-info` operation also returns OS family, architecture, and hostname. The current controller validates the ping operation only; system inventory integration is a later step.

## Current limitations

- The controller checks connectivity and agent/protocol compatibility only.
- Agent deployment, key provisioning, inventory, retries, and structured logs are not automated.
- SSH server setup is operator-managed.
- No CIS evaluation, privileged collection, remediation, or configuration writes are included.
- GitHub Actions has successfully configured, built, and run the agent smoke tests on native Windows, Linux, and macOS runners. This validates compilation and local agent behavior on all three platforms; a real SSH session to each target OS still requires separate VM testing.
- The controller intentionally accepts only the agent's exact protocol-v1 ping JSON line on standard output. Avoid shell startup scripts that print banners or other text to standard output for the SSH account.

## Controller exit codes

| Code | Meaning |
|---:|---|
| 0 | SSH connection and protocol handshake succeeded |
| 1 | SSH/process/connection failure |
| 2 | Invalid controller arguments or agent invocation |
| 3 | Agent response did not match the expected protocol |
