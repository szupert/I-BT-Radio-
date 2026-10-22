#pragma once
#include <Arduino.h>
// I-BT-Radio-owned presentation layer. Does not modify storage, URLs or API identifiers.
static const char ibt_branding_js[] PROGMEM = R"IBTJS(
(() => {
  'use strict';
  const product = 'I-BT-Radio';
  const credit = 'I-BT-Radio · Licencek';
  const skip = 'script,style,textarea,input,pre,code,[data-ibt-preserve],#ibt-source-credit';
  let queued = false;
  function applyBrand() {
    queued = false;
    if (!document.body) return;
    if (document.title !== product + ' V029') document.title = product + ' V029';
    const walker = document.createTreeWalker(document.body, NodeFilter.SHOW_TEXT);
    const texts = [];
    while (walker.nextNode()) texts.push(walker.currentNode);
    for (const node of texts) {
      if (!node.parentElement || node.parentElement.closest(skip)) continue;
      const next = node.nodeValue.replace(/\byoradio\b/gi, product);
      if (next !== node.nodeValue) node.nodeValue = next;
    }
    for (const el of document.querySelectorAll('[title],[aria-label],img[alt]')) {
      if (el.closest(skip)) continue;
      for (const a of ['title','aria-label','alt']) {
        const text = el.getAttribute(a);
        if (!text || !/\byoradio\b/i.test(text)) continue;
        el.setAttribute(a, text.replace(/\byoradio\b/gi, product));
        if (el.tagName === 'IMG' && a === 'alt' && !el.hasAttribute('data-ibt-logo-replaced')) {
          el.setAttribute('data-ibt-logo-replaced','1');
          const label = document.createElement('span');
          label.className = 'ibt-product-mark'; label.textContent = product;
          el.insertAdjacentElement('afterend',label); el.hidden = true;
        }
      }
    }
    // A title/logo authored by old scripts must not conceal the current product.
    if (!document.getElementById('ibt-product-header')) {
      const h=document.createElement('header'); h.id='ibt-product-header';
      h.textContent=product+' V029'; document.body.prepend(h);
    }
    if (!document.getElementById('ibt-source-credit')) {
      const f=document.createElement('footer'); f.id='ibt-source-credit';
      const a=document.createElement('a'); a.textContent=credit;
      a.href='https://github.com/szupert/I-BT-Radio-/blob/main/THIRD_PARTY_NOTICES.md'; a.target='_blank'; a.rel='noopener noreferrer';
      f.appendChild(a); document.body.appendChild(f);
    }
    if (!document.getElementById('ibt-brand-style')) {
      const style=document.createElement('style'); style.id='ibt-brand-style';
      style.textContent='#ibt-product-header{padding:10px 12px;text-align:center;font:bold 18px system-ui,sans-serif;color:inherit}#ibt-source-credit{padding:12px;text-align:center;font:12px system-ui,sans-serif;color:inherit}#ibt-source-credit a{color:inherit}.ibt-product-mark{font-weight:bold}';
      document.head.appendChild(style);
    }
  }
  function schedule() {
    if (!queued) { queued=true; setTimeout(applyBrand,40); }
  }
  function start() {
    applyBrand();
    const observer=new MutationObserver(schedule);
    observer.observe(document.documentElement,{childList:true,subtree:true,characterData:true});
  }
  if (document.readyState==='loading') document.addEventListener('DOMContentLoaded',start,{once:true});
  else start();
})();
)IBTJS";
