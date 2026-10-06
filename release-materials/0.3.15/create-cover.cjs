const fs = require('node:fs');
const path = require('node:path');

const ivory = '#f4f1e8', gold = '#d5af6c';
const point = (r, angle) => {
  const a = angle * Math.PI / 180;
  return [Math.sin(a) * r, -Math.cos(a) * r].map(v => v.toFixed(2)).join(' ');
};
const icons = [
  // SVG counterparts of the North Etch contours in include/WheelIcons.h.
  // Each entry keeps the same main (1.75) and fine (1.15) stroke hierarchy.
  ['Sword', 'M-3 5 L-4 -12 L0 -19 L4 -12 L3 5 M-9 2 L-7 5 L7 5 L9 2 M-2 5 V14 L-3 16 L0 19 L3 16 L2 14 V5', 'M0 -12 V2 M-2 13 H2'],
  ['Potion', 'M-5 -18 H5 V-14 H-5 Z M-4 -14 V-7 C-4 -4 -13 -1 -13 7 C-13 14 -8 17 0 17 C8 17 13 14 13 7 C13 -1 4 -4 4 -7 V-14 M0 1 L3.6 6 L0 11 L-3.6 6 Z'],
  ['Armor', 'M-6 -17 C-4 -12 4 -12 6 -17 L14 -13 L11 -3 L10 9 L6 15 L0 18 L-6 15 L-10 9 L-11 -3 L-14 -13 Z M-8 -5 L0 -1 L8 -5 M-8 9 L0 12 L8 9', 'M0 -1 V10'],
  ['Bow', 'M-7 -18 C-6 -12 10 -13 10 0 C10 13 -6 12 -7 18 M-7 -18 L-3 0 L-7 18 M-16 0 H17 M11 -5 L17 0 L11 5', 'M-15 -4 L-11 0 M-15 4 L-11 0'],
  ['Scroll', 'M-10 -11 H-15 V-14 C-15 -18 -7 -18 -7 -14 V12 C-7 18 12 18 12 12 V9 H-2 M-11 -17 H9 L12 -14 V5', 'M-2 -9 H7 M-2 -4 H7 M-2 1 H3'],
  ['Outfit', 'M-5 -13 C-5 -19 5 -19 5 -13 C5 -10 0 -10 0 -7 V-4 M0 -4 L16 6 C18 8 17 11 14 11 H-14 C-17 11 -18 8 -16 6 Z'],
  ['Shield', 'M0 -17 C5 -14 10 -13 14 -13 L12 3 C11 10 5 15 0 18 C-5 15 -11 10 -12 3 L-14 -13 C-10 -13 -5 -14 0 -17 M0 -12 L9 -9 L8 2 C7 7 3 11 0 13 C-3 11 -7 7 -8 2 L-9 -9 Z M3 -1 A3 3 0 1 0 -3 -1 A3 3 0 1 0 3 -1'],
  ['Amulet', 'M-13 -17 C-13 -5 -7 0 0 4 C7 0 13 -5 13 -17 M0 2 L5.76 10 L0 18 L-5.76 10 Z', 'M0 6 V14 M-8 -10 L-5 -5 M8 -10 L5 -5'],
  ['Food', 'M0 -8 C-11 -17 -19 -6 -13 7 C-9 18 -5 18 0 14 C5 18 9 18 13 7 C19 -6 11 -17 0 -8 M0 -8 C-1 -12 0 -16 3 -18 M3 -14 C6 -20 11 -19 14 -17 C10 -12 7 -11 3 -14 M-8 -5 C-11 -2 -10 2 -9 4'],
  ['Magic', 'M0 -19 L5 -5 L15 0 L5 5 L0 19 L-5 5 L-15 0 L-5 -5 Z M0 -4 L2.88 0 L0 4 L-2.88 0 Z', 'M-13 -13 L-9 -9 M9 9 L13 13 M9 -9 L13 -13 M-13 13 L-9 9'],
];
const sectors = icons.map(([name, icon, fine], i) => {
  const centre = i * 36, start = centre - 16.4, end = centre + 16.4;
  const outline = `M${point(365, start)} A365 365 0 0 1 ${point(365, end)} L${point(124, end)} A124 124 0 0 0 ${point(124, start)} Z`;
  const p = point(252, centre), color = i === 1 ? gold : ivory;
  return `  <g aria-label="${name}" stroke="${color}">
    <path d="${outline}" stroke-width="${i === 1 ? 6 : 4}"/>
    <g transform="translate(${p}) scale(2.65)">
      <path d="${icon}" stroke-width="1.75"/>${fine ? `\n      <path d="${fine}" stroke-width="1.15"/>` : ''}
    </g>
  </g>`;
}).join('\n');
const svg = `<svg xmlns="http://www.w3.org/2000/svg" width="2560" height="1440" viewBox="0 0 2560 1440" role="img" aria-labelledby="title description">
<title id="title">Favorite Wheel - Radial Actions</title>
<desc id="description">Original vector cover in the author's charcoal, ivory and gold series. A ten-slot favorites wheel with a highlighted potion, original line icons, and a central Q key. Promotional artwork, not a game screenshot.</desc>
<rect width="2560" height="1440" fill="#11191b"/>
<g font-family="Segoe UI, Arial, sans-serif">
  <text x="168" y="390" fill="#b4bdbd" font-size="37" letter-spacing="13">SKYRIM SPECIAL EDITION</text>
  <text x="154" y="650" fill="${ivory}" font-size="244" font-weight="700" letter-spacing="-6">FAVORITE</text>
  <text x="154" y="899" fill="${ivory}" font-size="244" font-weight="700" letter-spacing="-6">WHEEL</text>
  <text x="170" y="1015" fill="${gold}" font-size="58" font-weight="600" letter-spacing="7">RADIAL ACTIONS</text>
  <text x="170" y="1100" fill="#aeb9b9" font-size="41">Your favorites and actions, at your fingertips.</text>
</g>
<g transform="translate(2000 740)" fill="none" stroke-linecap="round" stroke-linejoin="round">
  <circle r="385" stroke="#475252" stroke-width="2"/>
${sectors}
  <circle r="102" stroke="${ivory}" stroke-width="4"/>
  <path d="M80 -120 L90 -110 L80 -100 L70 -110 Z" fill="${gold}" stroke="${gold}" stroke-width="2"/>
  <text x="0" y="30" fill="${ivory}" stroke="none" font-family="Segoe UI, Arial, sans-serif" font-size="96" font-weight="400" text-anchor="middle">Q</text>
</g>
</svg>
`;
fs.writeFileSync(path.join(__dirname, 'FavoriteWheel-cover.svg'), svg);
