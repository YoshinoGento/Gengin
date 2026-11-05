const fs = require('node:fs');
const path = require('node:path');
const {pathToFileURL} = require('node:url');
const {chromium} = require('playwright');
const out = path.resolve(__dirname, '../../generated/ui_progress_revision_20261103');

(async () => {
  const printPath = path.join(out, 'preview/static-print.html');
  fs.writeFileSync(printPath, '<!doctype html><meta charset="utf-8"><title>UI改善 静止画予備版</title><style>@page{size:13.333333in 7.5in;margin:0}body{margin:0}section{width:13.333333in;height:7.5in;break-after:page}section:last-child{break-after:auto}img{display:block;width:100%;height:100%;object-fit:contain}</style>' + Array.from({length:6}, (_,i) => `<section><img src="slide-${i+1}.png" alt="スライド${i+1}"></section>`).join(''));
  const browser = await chromium.launch({channel:'msedge', headless:true});
  try {
    const page = await browser.newPage();
    await page.goto(pathToFileURL(printPath).href);
    await page.waitForFunction(() => [...document.images].every(x => x.complete && x.naturalWidth > 0));
    await page.pdf({path:path.join(out,'UI改善_静止画予備版.pdf'),printBackground:true,preferCSSPageSize:true});
  } finally {
    await browser.close();
  }
  console.log('PASS: six-page static fallback PDF');
})().catch(error => { console.error(error); process.exitCode = 1; });
