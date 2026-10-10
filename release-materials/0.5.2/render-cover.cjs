const fs = require('node:fs');
const path = require('node:path');
const sharp = require('sharp');

async function main() {
  const svg = fs.readFileSync(path.join(__dirname, 'FavoriteWheel-cover.svg'));
  for (const width of [2560, 1280]) {
    await sharp(svg).resize(width, width * 9 / 16).png().toFile(
      path.join(__dirname, `FavoriteWheel-cover${width === 2560 ? '' : '-1280'}.png`));
  }
}
main().catch(error => { console.error(error); process.exitCode = 1; });
