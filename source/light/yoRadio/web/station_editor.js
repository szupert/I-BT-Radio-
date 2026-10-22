// Included inside the main WebUI closure; no separate browser request.
var editorRows=[],editorSelected=-1,editorLoaded=false,editorDirty=false,formChanged=false,editorBusy=false,expectedCsv=null;
function editorNote(text){$('station-save-state').textContent=text;}
function stationFetch(url,options){
 var controller=new AbortController(),timer=setTimeout(function(){controller.abort();},10000);
 return fetch(url,Object.assign({cache:'no-store'},options||{},{signal:controller.signal})).finally(function(){clearTimeout(timer);});
}
function editorLock(on){editorBusy=on;$('stations-dialog').querySelectorAll('input,button').forEach(function(b){if(!b.dataset.close)b.disabled=on;});}
function editorForm(){
 var row=editorRows[editorSelected];$('station-name').value=row?row.name:'';$('station-url').value=row?row.url:'';$('station-volume').value=row?row.volume:0;formChanged=false;
}
function editorRender(){
 var list=$('station-editor-list');list.replaceChildren();
 editorRows.forEach(function(row,i){var li=document.createElement('li'),button=document.createElement('button');
  button.type='button';button.textContent=(i+1)+'. '+row.name;button.setAttribute('aria-pressed',String(i===editorSelected));
  button.addEventListener('click',function(){if(!editorCommit())return;editorSelected=i;editorForm();editorRender();});li.appendChild(button);list.appendChild(li);
 });
 $('station-info').textContent=editorRows.length+' / 480 állomás'+(editorDirty?' · még nincs mentve':'');
}
function editorCommit(){
 if(!formChanged)return true;
 try{
  var row=IBTStations.validate([{name:$('station-name').value,url:$('station-url').value,volume:$('station-volume').value}])[0];
  if(editorSelected>=0&&editorSelected<editorRows.length)editorRows[editorSelected]=row;
  else{if(editorRows.length>=480)throw Error('Legfeljebb 480 állomás fér el.');editorRows.push(row);editorSelected=editorRows.length-1;}
  formChanged=false;editorDirty=true;editorRender();return true;
 }catch(e){editorNote(e.message);return false;}
}
function editorReload(){
 if(editorBusy)return;
 if(expectedCsv){editorNote('Előbb ellenőrizd a már elküldött mentést. Nem dobom el annak állapotát.');return;}
 if(editorDirty&&!confirm('Eldobod a még nem mentett állomáslista-módosításokat?'))return;
 editorLock(true);editorNote('Állomáslista beolvasása…');
 stationFetch('/api/radio-stations.csv').then(function(r){if(r.status===404)return '';if(!r.ok)throw Error('HTTP '+r.status);return r.text();})
 .then(function(text){editorRows=text.trim()?IBTStations.parse(text):[];editorSelected=editorRows.length?0:-1;editorLoaded=true;editorDirty=false;editorForm();editorRender();editorNote('Lista beolvasva. A módosítások csak a teljes lista mentésekor kerülnek a rádióra.');})
 .catch(function(e){editorNote('Beolvasási hiba: '+e.message);}).finally(function(){editorLock(false);});
}
$('open-stations').addEventListener('click',function(){$('stations-dialog').showModal();if(!editorLoaded)editorReload();if(saveRecord&&saveRecord.phase==='queued')runStationSave(false);});
$('stations-reload').addEventListener('click',editorReload);
$('station-form').addEventListener('input',function(){formChanged=true;editorDirty=true;});
$('station-form').addEventListener('submit',function(e){e.preventDefault();if(editorCommit()){editorRender();editorNote('Sor alkalmazva az előnézetre. A rádión még nem változott.');}});
$('station-add').addEventListener('click',function(){if(!editorCommit())return;if(editorRows.length>=480){editorNote('Legfeljebb 480 állomás adható meg.');return;}editorSelected=-1;editorForm();editorRender();$('station-name').focus();});
$('station-delete').addEventListener('click',function(){if(editorSelected<0)return;editorRows.splice(editorSelected,1);editorSelected=Math.min(editorSelected,editorRows.length-1);editorDirty=true;editorForm();editorRender();});
function editorMove(direction){if(!editorCommit())return;var next=editorSelected+direction;if(editorSelected<0||next<0||next>=editorRows.length)return;
 var row=editorRows[editorSelected];editorRows[editorSelected]=editorRows[next];editorRows[next]=row;editorSelected=next;editorDirty=true;editorForm();editorRender();}
$('station-up').addEventListener('click',function(){editorMove(-1);});$('station-down').addEventListener('click',function(){editorMove(1);});
$('station-import').addEventListener('click',function(){var files=Array.from($('station-file').files||[]),keep=$('station-merge-current').checked;if(!files.length||files.length>10){editorNote('Válassz 1–10 állomáslista-fájlt.');return;}
 if(files.some(function(file){return file.size>262144;})){editorNote('Egy fájl legfeljebb 256 KiB lehet.');return;}
 if(keep&&!editorCommit())return;
 if(!keep&&editorDirty&&!confirm('A fájlok lecserélik a szerkesztő még nem mentett előnézetét. Folytatod?'))return;
 editorLock(true);Promise.all(files.map(function(file){return file.arrayBuffer().then(function(bytes){return new TextDecoder('utf-8',{fatal:true}).decode(bytes);});})).then(function(texts){
  var result=IBTStations.merge(texts,keep?editorRows:[]);
  editorRows=result.rows;editorSelected=0;editorDirty=true;editorLoaded=true;editorForm();editorRender();editorNote(files.length+' fájl · '+result.total+' sorból '+result.rows.length+' állomás · '+result.duplicates+' duplikáció kiszűrve. Ellenőrizd, majd mentsd a teljes listát.');
 }).catch(function(e){editorNote('Importálási hiba, a korábbi lista megmaradt: '+e.message);}).finally(function(){editorLock(false);});
});
$('station-export').addEventListener('click',function(){if(!editorCommit())return;try{var csv=IBTStations.serialize(editorRows),url=URL.createObjectURL(new Blob([csv],{type:'text/tab-separated-values;charset=utf-8'})),a=document.createElement('a');
 a.href=url;a.download='playlist.csv';document.body.appendChild(a);a.click();a.remove();setTimeout(function(){URL.revokeObjectURL(url);},1000);
 editorNote(editorDirty?'A szerkesztett előnézet exportálva; a rádióra még nincs mentve.':'A rádió állomáslistája exportálva.');
 }catch(e){editorNote(e.message);}});
// Included in the editor closure. This is a control transaction, not an audio stress test.
var saveRecord=null,saveTimer=0,saveKnownDevice='',saveKey='ibt-station-save-v2:'+location.host;
function saveRemember(){localStorage.setItem(saveKey,JSON.stringify(saveRecord));}
function saveClear(message){
 localStorage.removeItem(saveKey);saveRecord=null;expectedCsv=null;clearTimeout(saveTimer);saveTimer=0;
 $('station-check').hidden=true;$('station-cancel-save').hidden=true;editorNote(message);
}
function saveSchedule(){
 clearTimeout(saveTimer);saveTimer=0;
 if(saveRecord&&saveRecord.phase==='queued'&&saveRecord.device&&!document.hidden)saveTimer=setTimeout(function(){saveTimer=0;runStationSave(false);},15000);
}
function savePath(action){return '/api/station-save/'+action+'?job='+saveRecord.job+(action==='prepare'&&saveRecord.device?'&device='+encodeURIComponent(saveRecord.device):'');}
async function saveControl(action){
 var r=await stationFetch(savePath(action),action==='status'?{}:{method:'POST'});
 if(!r.ok)throw Error('Mentésvezérlés: HTTP '+r.status);
 var state=await r.json();
 if(state.job!==saveRecord.job||typeof state.device!=='string'||!state.device)throw Error('Eltérő mentési kérés vagy hiányzó rádióazonosító.');
 if(saveRecord.device&&saveRecord.device!==state.device)throw Error('Másik rádió van ezen az IP-címen; a listát nem küldöm el.');
 saveRecord.device=state.device;saveKnownDevice=state.device;saveRemember();return state;
}
async function runStationSave(manual){
 clearTimeout(saveTimer);saveTimer=0;if(!saveRecord||editorBusy||document.hidden)return;
 editorLock(true);var sent=false;
 try{
  editorNote('Az állomásmentés az elsődleges. A rádió szabályosan leállítja a hangot, majd ellenőrzi és elmenti a listát…');
  $('station-check').hidden=false;
  var state=await saveControl(saveRecord.phase==='queued'?'prepare':'status'),deadline=Date.now()+30000;
  while(true){
   if(saveRecord.cancel){
    if(state.phase==='ready'){await saveControl('cancel');saveClear('A mentési kérés megszakítva; a rádió listáját nem cseréltem le. A lejátszás visszaállítását kértem.');return;}
    if(state.phase==='complete'||state.phase==='failed'){saveClear('A kérés már lezárult a rádión. A listát újra beolvashatod.');return;}
   }
   if(state.phase==='ready'&&state.uploadReady&&!saveRecord.cancel){
    if(saveRecord.phase==='verified'||(saveRecord.phase!=='queued'&&!manual))throw Error('A korábbi feltöltés eredménye bizonytalan. Az ellenőrzés gombbal folytathatod.');
    saveRecord.phase='submitted';saveRemember();sent=true;
    var body=new FormData();body.append('plfile',new Blob([saveRecord.csv],{type:'text/plain;charset=utf-8'}),'playlist.csv');
    var response=await stationFetch('/upload',{method:'POST',headers:{'X-Station-Save':String(saveRecord.job)},body:body});
    if(response.status!==202)throw Error('A lista feltöltése HTTP '+response.status+' hibával visszautasult. Az előnézet megmaradt.');
    await response.json();manual=false;editorNote('Lista elküldve. A rádiós ellenőrzést és a teljes visszaolvasást várom…');
   }else if(state.phase==='saved'||state.phase==='complete'){
    if(!state.installed)throw Error('A rádió nem igazolta a lista telepítését.');
    if(saveRecord.phase!=='verified'){
     var read=await stationFetch('/api/radio-stations.csv');if(!read.ok)throw Error('Visszaolvasás: HTTP '+read.status);
     var csv=IBTStations.serialize(IBTStations.parse(await read.text()));
     if(csv!==saveRecord.csv)throw Error('A teljes visszaolvasott lista eltér. A mentést nem jelölöm sikeresnek.');
     saveRecord.phase='verified';saveRemember();
    }
    if(state.phase==='saved')await saveControl('finish');
    var unchanged=false;try{unchanged=!formChanged&&IBTStations.serialize(editorRows)===saveRecord.csv;}catch(e){}
    editorDirty=!unchanged;stationNames=null;stationLoading=null;editorRender();
    saveClear('Mentés kész, a teljes állomáslista visszaolvasva és ellenőrizve. A korábbi lejátszás folytatását kértem; törölt adó helyett nem indítok másikat.'+(editorDirty?' Az újabb előnézet-módosítás még nincs mentve.':''));return;
   }else if(state.phase==='failed')throw Error('A rádiós mentési kérés lezárult sikeres telepítés nélkül. Az előnézet megmaradt.');
   if(Date.now()>=deadline||document.hidden)throw Error('A mentési kérés megmaradt. Később az ellenőrzés gombbal folytasd; ne töltsd fel vakon újra.');
   await new Promise(function(resolve){setTimeout(resolve,750);});state=await saveControl('status');
  }
 }catch(e){
  editorNote('A mentés még nincs igazolva: '+e.message);$('station-check').hidden=false;
  $('station-cancel-save').hidden=!saveRecord||saveRecord.phase!=='queued';
 }finally{editorLock(false);if(!sent)saveSchedule();}
}
function checkStationSave(){runStationSave(true);}
$('station-check').addEventListener('click',checkStationSave);
$('station-save').addEventListener('click',function(){
 if(editorBusy||!editorCommit())return;
 if(saveRecord){editorNote('Már van mentési kérés. Előbb az ellenőrzés / folytatás gombot használd.');return;}
 try{
  var csv=IBTStations.serialize(editorRows);
  saveRecord={schema:2,job:1+Math.floor(Math.random()*2147483646),device:saveKnownDevice,csv:csv,phase:'queued'};
  saveRemember();expectedCsv=csv;runStationSave(true);
 }catch(e){saveRecord=null;expectedCsv=null;editorNote('A böngésző nem tudta megőrizni a kérést; nem küldtem semmit a rádióra. Exportáld a listát CSV-be. '+e.message);}
});
$('station-cancel-save').addEventListener('click',function(){
 if(!saveRecord||editorBusy||saveRecord.phase!=='queued')return;
 saveRecord.cancel=true;saveRemember();runStationSave(false);
});
document.addEventListener('visibilitychange',function(){if(document.hidden){clearTimeout(saveTimer);saveTimer=0;}else saveSchedule();});
try{
 var stored=JSON.parse(localStorage.getItem(saveKey)||'null');
 if(stored&&stored.schema===2&&Number.isInteger(stored.job)&&stored.job>0&&stored.job<=2147483647&&typeof stored.device==='string'&&['queued','submitted','verified'].includes(stored.phase)){
  var rows=IBTStations.parse(stored.csv);if(IBTStations.serialize(rows)!==stored.csv)throw Error('Sérült mentési kérés.');
  saveRecord=stored;expectedCsv=stored.csv;saveKnownDevice=stored.device;editorRows=rows;editorSelected=0;editorLoaded=true;editorDirty=true;editorForm();editorRender();
  $('station-check').hidden=false;$('station-cancel-save').hidden=stored.phase!=='queued';
  editorNote('Korábbi mentési kérés visszatöltve. A mentés a hang elé kerül; az eredmény még ellenőrizendő.');saveSchedule();
 }else{
  // R175 drafts never become an automatic new upload or an automatic stop.
  var old=JSON.parse(localStorage.getItem('ibt-station-save-v1:'+location.host)||'null');
  if(old&&typeof old.csv==='string'){
   editorRows=IBTStations.parse(old.csv);saveKnownDevice=typeof old.device==='string'?old.device:'';editorSelected=0;editorLoaded=true;editorDirty=true;editorForm();editorRender();
   editorNote('Régi mentési előnézet visszatöltve. A rádióra nem küldtem újra; ellenőrizd, majd külön kérd a mentést.');
  }
 }
}catch(e){editorNote('A korábbi mentési kérés nem tölthető vissza: '+e.message);}

$('reset-open').addEventListener('click',function(){$('reset-confirm').showModal();});
$('reset-accept').addEventListener('click',function(){var button=this;button.disabled=true;
 stationFetch('/api/reset-user',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'confirm=reset-user-keep-connections'})
 .then(function(r){if(r.status!==202)throw Error(r.status===409?'Előbb állítsd le a lejátszást, válts internetes rádió / DAC módra, és fejezd be az importálást.':'HTTP '+r.status);
  $('reset-confirm').close();menuPanel.hidden=true;menuPause();$('reset-state').textContent='Visszaállítás kérése elfogadva. A rádió újraindul; utána töltsd újra ezt az oldalt. A Wi-Fi és a BT-párosítások megmaradnak.';
  editorLoaded=false;editorDirty=false;expectedCsv=null;saveRecord=null;clearTimeout(saveTimer);saveTimer=0;stationNames=null;stationLoading=null;
  try{localStorage.removeItem(saveKey);localStorage.removeItem('ibt-station-save-v1:'+location.host);}catch(e){$('reset-state').textContent+=' A böngésző mentési kérését nem sikerült törölni; a lapot zárd be.';}
  try{Object.keys(localStorage).filter(function(k){return k.indexOf('ibt-menu-')===0&&k.includes(location.host+':');}).forEach(function(k){localStorage.removeItem(k);});}catch(e){}
 }).catch(function(e){$('reset-confirm').close();$('reset-state').textContent='Visszaállítás nincs igazolva: '+e.message;}).finally(function(){button.disabled=false;});
});
