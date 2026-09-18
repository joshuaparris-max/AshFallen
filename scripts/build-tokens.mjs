#!/usr/bin/env node
import { readFileSync, writeFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import { dirname, join } from 'node:path';

const root = join(dirname(fileURLToPath(import.meta.url)), '..');
const tokens = JSON.parse(readFileSync(join(root, 'design/tokens.json'), 'utf8'));
const kebab = (s) => s.replace(/([a-z0-9])([A-Z])/g, '$1-$2').toLowerCase();

const themed = [];
const flat = [];

for (const [name, value] of Object.entries(tokens.color)) {
  themed.push([`--color-${kebab(name)}`, value.light, value.dark]);
}

for (const group of ['radius', 'space', 'font', 'shadow', 'motion', 'chrome']) {
  for (const [name, value] of Object.entries(tokens[group] || {})) {
    flat.push([`--${group}-${kebab(name)}`, value]);
  }
}

const decls = (pairs, idx) =>
  pairs.map((p) => `  ${p[0]}: ${idx === undefined ? p[1] : p[idx]};`).join('\n');

const css = `/* GENERATED FILE — do not edit.
   Source: design/tokens.json
   Rebuild: node scripts/build-tokens.mjs
*/

:root {
${decls(flat)}

  /* light theme (default) */
${decls(themed, 1)}
}

@media (prefers-color-scheme: dark) {
  :root:not([data-theme='light']) {
${decls(themed, 2).replace(/^/gm, '  ')}
  }
}

:root[data-theme='dark'] {
${decls(themed, 2)}
}

:root[data-theme='light'] {
${decls(themed, 1)}
}
`;

writeFileSync(join(root, 'design/tokens.css'), css);
console.log(`design/tokens.css written — ${themed.length} themed, ${flat.length} static tokens`);
