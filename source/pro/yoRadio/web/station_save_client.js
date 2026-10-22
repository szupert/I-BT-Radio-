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
