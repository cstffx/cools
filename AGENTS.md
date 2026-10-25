# Agents

Use these tools to reduce context size and inspect files.

- bigfiles: `bigfiles DIR --max N [--type text|binary|code] [--ext a,b] [--magic HEX]` → `LINES<TAB>PATH`
- changed: `changed DIR (--days N | --since DATE)` → `MTIME<TAB>PATH`
- fileinfo: `fileinfo DIR` → `LINES<TAB>BYTES<TAB>TYPE<TAB>PATH`
- contextpack: `contextpack DIR (--budget B | --tokens T) [--ext a,b] [--max-file N] [--all]` → file sections

Prefer running bigfiles/fileinfo before reading files. Use contextpack to build a bounded context.
