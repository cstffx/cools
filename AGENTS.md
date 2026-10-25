# Agents

Use these before reading files:
- bigfiles DIR --max N [--type text|binary|code] [--ext a,b] [--magic HEX] → LINES<TAB>PATH
- changed DIR (--days N|--since DATE) → MTIME<TAB>PATH
- fileinfo DIR → LINES<TAB>BYTES<TAB>TYPE<TAB>PATH
- contextpack DIR (--budget B|--tokens T) [--ext a,b] [--max-file N] [--all] → sections

Prefer bigfiles/fileinfo first. Use contextpack to build bounded context.
