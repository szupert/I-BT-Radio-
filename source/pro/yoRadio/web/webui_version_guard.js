(() => {
 'use strict';
 const client={major:'166',release:'176',webui:'IBT_R176_1'};
 const key='ibt-webui-version:'+location.host;
 const token=v=>[v.major,v.release,v.webui].join('|');
 const controller=new AbortController(),timer=setTimeout(()=>controller.abort(),5000);
 fetch('/api/version',{cache:'no-store',signal:controller.signal}).then(r=>{if(!r.ok)throw Error('Version unavailable');return r.json();}).then(server=>{
  if(!['major','release','webui'].every(k=>typeof server[k]==='string'&&server[k]))return;
  const target=token(server);let old=null;
  try{old=localStorage.getItem(key);localStorage.setItem(key,target);}catch(e){}
  const mismatch=token(client)!==target || (old!==null&&old!==target);
  if(mismatch){try{Object.keys(localStorage).filter(k=>k.startsWith('ibt-menu-')&&k.includes(location.host+':')).forEach(k=>localStorage.removeItem(k));}catch(e){}}
  const url=new URL(location.href);
  if(mismatch&&url.searchParams.get('ibt-ui')!==target){
   url.searchParams.set('ibt-ui',target);location.replace(url.href);
  }
 }).catch(()=>{}).finally(()=>clearTimeout(timer));
})();
