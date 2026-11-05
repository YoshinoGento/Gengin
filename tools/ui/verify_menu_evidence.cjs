const { chromium } = require('playwright');
const fs = require('node:fs');
const path = require('node:path');
const { pathToFileURL } = require('node:url');

(async () => {
  const root = process.argv[2] ? path.resolve(process.argv[2])
    : path.resolve(__dirname, '../../generated/ui_presentation_20261102');
  const browser = await chromium.launch({ headless: true, channel: 'msedge' });
  try {
    const page = await browser.newPage({ viewport: { width: 1536, height: 1024 } });
    const errors = [];
    page.on('pageerror', error => errors.push(String(error)));
    await page.goto(pathToFileURL(path.join(root, 'presentation/index.html')).href);
    const checks = [];
    for (const width of [1536, 900, 375]) {
      await page.setViewportSize({ width, height: 1024 });
      const state = await page.evaluate(() => ({
        width: innerWidth,
        overflow: document.documentElement.scrollWidth > innerWidth,
        images: [...document.images].map(image => ({
          source: image.getAttribute('src'), complete: image.complete,
          width: image.naturalWidth, height: image.naturalHeight
        }))
      }));
      if (state.overflow || state.images.some(image => !image.complete || !image.width))
        throw new Error(JSON.stringify(state));
      checks.push(state);
    }
    await page.setViewportSize({ width: 1536, height: 1024 });
    await page.locator('section').first().screenshot({ path: path.join(root, 'comparison_preview.png') });
    if (errors.length) throw new Error(errors.join('\n'));
    fs.writeFileSync(path.join(root, 'html_check.json'), JSON.stringify(checks, null, 2));
    console.log('PASS: comparison images and page bounds at 1536/900/375px');
  } finally {
    await browser.close();
  }
})().catch(error => { console.error(error); process.exitCode = 1; });
