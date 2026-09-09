# Security Policy

## Supported versions

Kite is pre-1.0 and moves quickly. Only the latest release gets fixes.

| Version | Supported |
| ------- | --------- |
| latest `0.x` | yes |
| anything older | no |

## Reporting a vulnerability

**Do not open a public issue for a security problem.**

Report it privately through GitHub:

1. Go to <https://github.com/probablytaylors/Kite/security/advisories/new>
2. Describe the issue, the affected version (`kite --version`), and a way to
   reproduce it.

You'll get a reply within a week. Once a fix is ready it ships in a normal
release and the advisory is published with credit to the reporter (unless you
ask otherwise).

## Threat model

Kite runs code. Treat a `.kite` file like any other script:

- The interpreter, the bytecode VM, and native programs have **no sandbox**.
  A program can read and write files (`read_file`, `write_file`), read
  environment variables (`env`), and read standard input. Only run `.kite`
  files you trust, the same as a `.py` or `.js` file.
- Installing the `.kite` file association (an opt-in step in the installer)
  means double-clicking a `.kite` file runs it. Leave it off if you don't want
  that.

Out of scope: a program (or a hand-crafted `.kbc`) can loop forever or exhaust
memory. Kite does not cap run time or allocation. Interrupt with Ctrl+C.

What Kite *does* try to guarantee:

- **`kite exec` on an untrusted `.kbc` file must not corrupt memory.** A
  malformed or hand-edited bytecode artifact is rejected or fails cleanly, it
  does not read or write outside its own buffers. Bytecode-verifier or VM bugs
  that break this are in scope.
- **`kite run` / `kite build` on a syntactically hostile `.kite` file must not
  crash the process** (stack exhaustion, integer overflow, and similar). Still
  in scope even though the file is "trusted" to run.
- **`kite native` must not let a `.kite` source inject arbitrary C** into the
  program it generates.
- **`kite update` verifies every download** against a published SHA-256 and
  refuses to run an installer it can't check.

## Advisories

Published security advisories are listed at
<https://github.com/probablytaylors/Kite/security/advisories>. Releases that
contain security fixes say so in the changelog, and `kite update` flags them.
