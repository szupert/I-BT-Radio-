(function(root){'use strict';
 var enc=new TextEncoder();
 function validate(rows,source){
  if(!rows.length||(!source&&rows.length>480))throw Error('1–480 állomás adható meg.');
  return rows.map(function(r,i){
   var name=String(r.name||'').trim(),url=String(r.url||'').trim(),volume=Number(r.volume===undefined?0:r.volume);
   if(!name||enc.encode(name).length>169||/[\x00-\x1f\x7f]/.test(name))throw Error((i+1)+'. sor: hibás vagy túl hosszú név (legfeljebb 169 bájt).');
   if(enc.encode(url).length>169||/[\x00-\x20\x7f]/.test(url)||!/^https?:\/\//i.test(url))throw Error((i+1)+'. sor: teljes http:// vagy https:// streamcím szükséges (legfeljebb 169 bájt).');
   try{if(!new URL(url).hostname)throw Error();}catch(e){throw Error((i+1)+'. sor: hibás streamcím.');}
   if(!Number.isInteger(volume)||volume< -80||volume>80)throw Error((i+1)+'. sor: a hangerőkorrekció −80 és +80 közötti egész szám.');
   return {name:name,url:url,volume:volume};
  });
 }
 function fields(line,separator){
  var a=[],s='',quote=false;
  for(var i=0;i<line.length;i++){
   var c=line[i];if(c==='"'){if(quote&&line[i+1]==='"'){s+='"';i++;}else quote=!quote;}
   else if(c===separator&&!quote){a.push(s);s='';}else s+=c;
  }
  if(quote)throw Error('Lezáratlan idézőjel az importált sorban.');a.push(s);return a;
 }
 function parse(text,source){
  if(enc.encode(text).length>262144)throw Error('A fájl legfeljebb 256 KiB lehet.');
  text=text.replace(/^\uFEFF/,'');var lines=text.split(/\r?\n/).filter(function(l){return l.trim();}),rows=[];
  if(!lines.length)throw Error('A fájl üres.');
  if(lines[0].trim()==='#EXTM3U'){
   var title='';lines.forEach(function(l){if(l.indexOf('#EXTINF:')===0)title=l.slice(l.indexOf(',')+1).trim();
    else if(l[0]!=='#'){rows.push({name:title||l,url:l,volume:0});title='';}});
  }else{
   var separator=lines[0].includes('\t')?'\t':lines[0].includes(';')?';':',';
   lines.forEach(function(l,i){var f=separator==='\t'?l.split('\t'):fields(l,separator);
    if(i===0&&/^(name|név|állomás)$/i.test(f[0].trim())&&/^(url|cím|stream)$/i.test((f[1]||'').trim()))return;
    if(f.length<2||f.length>3)throw Error((i+1)+'. sor: név, streamcím és opcionális hangerőkorrekció kell.');
    rows.push({name:f[0],url:f[1],volume:f[2]===undefined||f[2]===''?0:Number(f[2])});
   });
  }
  return validate(rows,source===true);
 }
 function serialize(rows){return validate(rows).map(function(r){return r.name+'\t'+r.url+'\t'+r.volume;}).join('\n')+'\n';}
 function merge(sources,initial){
  if(!Array.isArray(sources)||!sources.length||sources.length>10)throw Error('Egyszerre 1–10 fájl egyesíthető.');
  var rows=[],seen=new Set(),total=0;
  function add(list){list.forEach(function(row){total++;
   // Hostname/default port normalization is safe; path/query case and order
   // stay intact. Fragments are not sent in an HTTP stream request.
   var address=new URL(row.url);address.hash='';var key=address.href;
   if(!seen.has(key)){seen.add(key);rows.push(row);}
  });}
  if(initial&&initial.length)add(validate(initial));
  sources.forEach(function(text,i){try{add(parse(text,true));}catch(e){throw Error((i+1)+'. fájl: '+e.message);}});
  if(rows.length>480)throw Error('Az egyesítés után '+rows.length+' különböző állomás marad ('+(total-rows.length)+' ismétlődés kiszűrve). A rádió legfeljebb 480-at fogad; az előnézet nem változott.');
  return {rows:validate(rows),total:total,duplicates:total-rows.length};
 }
 var api={parse:parse,validate:validate,serialize:serialize,merge:merge};
 if(typeof module!=='undefined'&&module.exports)module.exports=api;else root.IBTStations=api;
})(typeof globalThis!=='undefined'?globalThis:this);
