const fs = require('node:fs');
const path = require('node:path');

const ivory = '#f4f1e8', gold = '#d5af6c';
const point = (r, angle) => {
  const a = angle * Math.PI / 180;
  return [Math.sin(a) * r, -Math.cos(a) * r].map(v => v.toFixed(2)).join(' ');
};
const icons = [
  ['Sword', 'M0 -19 L5 -12 L3 6 H-3 L-5 -12 Z M0 -13 V5 M-9 7 H9 M0 8 V16 M0 16 L2 18 L0 20 L-2 18 Z'],
  ['Potion', 'M-5 -14 H5 V-5 L12 2 L13 11 L8 17 H-8 L-13 11 L-12 2 L-5 -5 Z M-6 -18 H6 M-6 -14 H6 M0 2 V12 M-5 7 H5'],
  ['Armor', 'M-7 -15 L-15 -11 L-18 -3 L-11 0 L-10 15 H10 L11 0 L18 -3 L15 -11 L7 -15 L5 -9 H-5 Z M-7 -2 L0 1 L7 -2 M0 1 V8 M-8 10 H8'],
  ['Bow', 'M-8 -18 C20 -12 20 12 -8 18 M-8 -18 L-3 0 L-8 18 M-16 0 H18 M12 -5 L18 0 L12 5'],
  ['Scroll', 'M-10 -14 H10 V11 L6 16 H-10 V-8 H-15 V-12 Z M-10 -8 V-12 M-5 -5 H5 M-5 0 H5 M-5 5 H2 M-8 11 H10'],
  ['Outfit', 'M-5 -13 C-5 -21 8 -21 5 -12 C4 -10 0 -10 0 -6 M0 -6 L17 7 V11 H-17 V7 Z'],
  ['Shield', 'M0 -17 L14 -12 L12 6 L7 13 L0 18 L-7 13 L-12 6 L-14 -12 Z M0 -12 V12 M-9 -5 H9'],
  ['Amulet', 'M-12 -17 C-16 0 -4 1 0 5 M12 -17 C16 0 4 1 0 5 M0 2 L8 10 L0 18 L-8 10 Z'],
  ['Food', 'M0 -8 L-7 -12 L-13 -8 L-15 0 L-12 10 L-6 16 L0 14 L6 16 L12 10 L15 0 L13 -8 L7 -12 Z M0 -8 L2 -17 M2 -15 L8 -19 L12 -16 L6 -12 Z M-9 -4 L-10 2'],
  ['Magic', 'M0 -12 L12 0 L0 12 L-12 0 Z M0 -5 L5 0 L0 5 L-5 0 Z M0 -20 V-16 M0 16 V20 M-20 0 H-16 M16 0 H20 M-13 -13 L-10 -10 M10 10 L13 13 M10 -10 L13 -13 M-13 13 L-10 10'],
];
const sectors = icons.map(([name, icon], i) => {
  const centre = i * 36, start = centre - 16.4, end = centre + 16.4;
  const outline = `M${point(365, start)} A365 365 0 0 1 ${point(365, end)} L${point(124, end)} A124 124 0 0 0 ${point(124, start)} Z`;
  const p = point(252, centre), color = i === 1 ? gold : ivory;
  return `  <g aria-label="${name}" stroke="${color}">
    <path d="${outline}" stroke-width="${i === 1 ? 6 : 4}"/>
    <path transform="translate(${p}) scale(2.65)" d="${icon}" stroke-width="1.8"/>
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
