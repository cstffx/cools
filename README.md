# cools

A small collection of command-line tools to help AI agents work with the file system.

Build dependency: `libmagic-dev` (for `fileinfo`).

| Tool | Description |
|------|-------------|
| bigfiles | Lists files whose line count exceeds a threshold; filters by type/ext/magic. |
| changed | Lists files modified since a date or within the last N days. |
| fileinfo | Reports line count, byte size and text/binary type per file. |
| contextpack | Packs text files into a context up to a byte/token budget. |
