#!/usr/bin/env node
import { readFileSync, writeFileSync, mkdirSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import { dirname, join } from 'node:path';

const root = join(dirname(fileURLToPath(import.meta.url)), '..');
const read = (p) => readFileSync(join(root, p), 'utf8');

let html = read('shell/index.html');

const inline = [
  ['<link rel="stylesheet" href="../design/tokens.css">', () => `<style>\n${read('design/tokens.css')}\n</style>`],
  ['<link rel="stylesheet" href="shell.css">', () => `<style>\n${read('shell/shell.css')}\n</style>`],
  ['<script src="shell.js"></script>', () => `<script>\n${read('shell/shell.js')}\n</script>`],
  ['<script src="apps.js"></script>', () => `<script>\n${read('shell/apps.js')}\n</script>`]
];

for (const [needle, replacement] of inline) {
  if (!html.includes(needle)) {
    console.error(`build-shell: could not find in index.html:\n  ${needle}`);
    process.exit(1);
  }
  html = html.replace(needle, replacement());
}

mkdirSync(join(root, 'dist'), { recursive: true });
const out = join(root, 'dist/josh-os-shell.html');
writeFileSync(out, html);
console.log(`dist/josh-os-shell.html written — ${(html.length / 1024).toFixed(1)} KB, no external files`);
