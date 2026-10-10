const fs = require('node:fs');
const path = require('node:path');
const bbcode = fs.readFileSync(path.join(__dirname, 'Nexus-description-en.bbcode.txt'), 'utf8');
const summary = fs.readFileSync(path.join(__dirname, 'Nexus-summary.txt'), 'utf8').trim();
const stack = [];
const supported = new Set(['center', 'size', 'b', 'color', 'list', 'url']);
for (const token of bbcode.matchAll(/\[(\/)?([a-z]+|\*)(?:=([^\]]+))?\]/g)) {
  const [, closing, tag] = token;
  if (tag === '*') {
    if (stack.at(-1) !== 'list') throw new Error('List entry outside a list');
  } else {
    if (!supported.has(tag)) throw new Error(`Unsupported BBCode tag: ${tag}`);
    if (closing) {
      if (stack.pop() !== tag) throw new Error(`Mismatched closing tag: ${tag}`);
    } else stack.push(tag);
  }
}
if (stack.length) throw new Error(`Unclosed BBCode tags: ${stack.join(', ')}`);
if (summary.length > 350) throw new Error('Summary exceeds 350 characters');
const escape = text => text.replaceAll('&', '&amp;').replaceAll('<', '&lt;').replaceAll('>', '&gt;').replaceAll('"', '&quot;');
const inline = text => escape(text)
  .replace(/\[b\]([\s\S]*?)\[\/b\]/g, '<strong>$1</strong>')
  .replace(/\[size=(\d+)\]([\s\S]*?)\[\/size\]/g, (_, size, text) => `<span class="size-${size}">${text}</span>`)
  .replace(/\[color=(#[a-f0-9]+)\]([\s\S]*?)\[\/color\]/gi, '<span style="color:$1">$2</span>')
  .replace(/\[url=(https:\/\/[^\]]+)\]([\s\S]*?)\[\/url\]/g, '<a href="$1">$2</a>');
const blocks = [];
let paragraph = [];
const flush = () => {
  if (paragraph.length) blocks.push(`<p>${inline(paragraph.join('\n')).replaceAll('\n', '<br>')}</p>`);
  paragraph = [];
};
for (let line of bbcode.split(/\r?\n/)) {
  if (line.startsWith('[center]')) { flush(); blocks.push('<div class="center">'); line = line.slice(8); }
  const closesCenter = line.endsWith('[/center]');
  if (closesCenter) line = line.slice(0, -9);
  if (line === '[list]') { flush(); blocks.push('<ul>'); }
  else if (line === '[/list]') { flush(); blocks.push('</ul>'); }
  else if (line.startsWith('[*]')) { flush(); blocks.push(`<li>${inline(line.slice(3))}</li>`); }
  else if (!line.trim()) flush();
  else paragraph.push(line);
  if (closesCenter) { flush(); blocks.push('</div>'); }
}
flush();
const html = `<!doctype html>
<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1">
<title>Favorite Wheel - Radial Actions — Description Preview</title>
<style>
body{margin:0;background:#11191b;color:#e4e6e4;font:16px/1.65 "Segoe UI",Arial,sans-serif}
main{max-width:900px;margin:0 auto;padding:32px 28px 72px}img{display:block;width:100%;height:auto;border-radius:8px;margin-bottom:28px}
.summary{border-left:3px solid #d5af6c;padding:12px 20px;background:#192326;margin-bottom:36px}.summary small{color:#aeb9b9}
.center{text-align:center;margin-bottom:32px}p{margin:0 0 20px}strong{color:#f4f1e8}a{color:#d5af6c;text-decoration:underline}
li{padding-left:4px;margin:8px 0}ul{padding-left:24px;margin:0 0 24px}.size-6{font-size:32px;line-height:1.3}.size-4{font-size:23px;line-height:1.4}.size-2{font-size:13px;color:#aeb9b9}
</style></head><body><main>
<img src="../0.3.15/FavoriteWheel-cover-1280.png" alt="Favorite Wheel - Radial Actions cover">
<div class="summary"><small>Nexus summary</small><br>${escape(summary)}</div>
${blocks.join('\n')}
</main></body></html>\n`;
fs.writeFileSync(path.join(__dirname, 'Nexus-preview.html'), html);
console.log(`BBCode tags balanced; summary ${summary.length} characters; HTML preview generated.`);
