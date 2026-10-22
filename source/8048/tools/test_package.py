import importlib.util,io,json,subprocess,sys,tempfile,time,unittest,shutil
from pathlib import Path
from contextlib import redirect_stdout
P=Path(__file__).resolve().parents[1]
U=Path('C:/esp32/USER/I-BT-Radio_MENU_8048S070C')
spec=importlib.util.spec_from_file_location('installer',P/'tools/update.py');up=importlib.util.module_from_spec(spec);spec.loader.exec_module(up)
class Checks(unittest.TestCase):
 def setUp(self):
    self.temp=tempfile.TemporaryDirectory(dir='C:/esp32/SYSTEM/LOGS/REPORTS');self.root=Path(self.temp.name)/'INSTALLER';shutil.copytree(U/'INSTALLER',self.root);up.ROOT=self.root
    for f in U.iterdir():
      if f.is_file():shutil.copy2(f,self.root.parent/f.name)
 def tearDown(self):
    assert Path(self.temp.name).resolve().is_relative_to(Path('C:/esp32/SYSTEM/LOGS/REPORTS').resolve())
    self.temp.cleanup()
 def test_integrity(self):
    with redirect_stdout(io.StringIO()):self.assertEqual(up.verify()['firmware'],'MENU8048C-R4')
 def test_tamper(self):
    f=self.root/'firmware.bin';f.write_bytes(f.read_bytes()+b'bad')
    with self.assertRaises(RuntimeError):up.verify()
 def test_offsets(self):
    f=self.root/'PACKAGE.json';m=json.loads(f.read_text());m['offsets']['bootloader.bin']=0x1000;f.write_text(json.dumps(m))
    with self.assertRaises(RuntimeError):up.verify()
 def test_target(self):
    f=self.root/'PACKAGE.json';m=json.loads(f.read_text());m['target']='SMALL';f.write_text(json.dumps(m))
    with self.assertRaises(RuntimeError):up.verify()
 def test_baud_chip(self):
    for action,baud in [('flash-id',115200),('read-flash',115200),('write-flash',1500000)]:
      c=up.command_for('COM99',action,[]);self.assertEqual(c[c.index('--baud')+1],str(baud));self.assertEqual(c[c.index('--chip')+1],'esp32s3')
 def test_live_cr(self):
    sink=io.StringIO();screen=io.StringIO()
    with redirect_stdout(screen):rc,out=up.run_live([sys.executable,'-u','-c',"import sys,time;sys.stdout.write('Writing 25%\\r');sys.stdout.flush();time.sleep(.08);print('100%')"],sink,3)
    self.assertEqual(rc,0);self.assertIn('25%\r',sink.getvalue());self.assertEqual(sink.getvalue(),screen.getvalue());self.assertIn('100%',out)
 def test_timeout(self):
    with redirect_stdout(io.StringIO()),self.assertRaises(TimeoutError):up.run_live([sys.executable,'-u','-c','import time;time.sleep(10)'],io.StringIO(),.2)
 def test_partition_gate(self):
    table=(self.root/'partitions.bin').read_bytes()
    up.validate_existing(b'\xff'*4096,table);up.validate_existing(table+b'\xff'*(4096-len(table)),table)
    for bad in [b'\x00'*4096,b'\xff'*100]:
      with self.assertRaises(RuntimeError):up.validate_existing(bad,table)
 def test_known_factory_table(self):
    table=(self.root/'partitions.bin').read_bytes();factory=(self.root/'partitions_factory.bin').read_bytes()
    self.assertEqual(up.validate_existing(factory+b'\xff'*(4096-len(factory)),table,factory),'partitions_factory.bin')
    self.assertEqual(up.validate_existing(b'\xff'*4096,table,factory),'partitions.bin')
    bad=bytearray(factory+b'\xff'*(4096-len(factory)));bad[8]^=1
    with self.assertRaises(RuntimeError):up.validate_existing(bad,table,factory)
 def test_preserve_boot_mode(self):
    m=json.loads((self.root/'PACKAGE.json').read_text());args=up.flash_args(m,'partitions_factory.bin')
    self.assertEqual(args[args.index('--flash-mode')+1],'keep')
    self.assertEqual(args[args.index('--flash-freq')+1],'keep')
    self.assertNotIn('qio',args);self.assertIn(str(self.root/'partitions_factory.bin'),args)
    self.assertEqual((self.root/'bootloader.bin').read_bytes()[2],2)
 def test_rehashed_wrong_boot_mode(self):
    import hashlib
    f=self.root/'bootloader.bin';d=bytearray(f.read_bytes());d[2]=0;f.write_bytes(d)
    manifest=self.root/'PACKAGE.json';m=json.loads(manifest.read_text());m['files']['bootloader.bin']['sha256']=hashlib.sha256(d).hexdigest();manifest.write_text(json.dumps(m))
    with self.assertRaises(RuntimeError):up.verify()
 def test_parallel_uploader_gate(self):
    a=up.acquire_port_lock('COM_HOSTTEST')
    try:
      with self.assertRaises(RuntimeError):up.acquire_port_lock('COM_HOSTTEST')
    finally:a.close()
    up.acquire_port_lock('COM_HOSTTEST').close()
 def test_flash_size(self):
    up.validate_flash_info('Detected flash size: 16MB')
    with self.assertRaises(RuntimeError):up.validate_flash_info('Detected flash size: 8MB')
 def test_rehashed_wrong_chip(self):
    import hashlib
    f=self.root/'firmware.bin';d=bytearray(f.read_bytes());d[12]=0;d[13]=0;f.write_bytes(d)
    manifest=self.root/'PACKAGE.json';m=json.loads(manifest.read_text());m['files']['firmware.bin']['sha256']=hashlib.sha256(d).hexdigest();manifest.write_text(json.dumps(m))
    with self.assertRaises(RuntimeError):up.verify()
 def test_all_pages_return(self):
    m=json.loads((P/'menu.json').read_text(encoding='utf-8'));self.assertEqual(len(m['pages']),55)
    seen=set();todo=[1]
    while todo:
      a=todo.pop()
      if a in seen:continue
      seen.add(a);todo.extend(c for x,b,c in m['links'] if x==a)
    self.assertEqual(set(m['pages']),seen)
    import hashlib
    self.assertEqual(m['expected_rows'],287)
    self.assertEqual(m['content_sha256'],hashlib.sha256((P/'src/radio/ui/content.h').read_bytes()).hexdigest())
 def test_idle_no_continuous_render(self):
    s=(P/'src/main.cpp').read_text(encoding='utf-8')
    self.assertIn('if(dirty)draw()',s);self.assertIn('choices[pageCount][16]',s)
    self.assertIn('screen.setClipRect(0,y,800,49)',s)
 def test_skin_copy_precision(self):
    m=json.loads((P/'skin_audit.json').read_text())
    self.assertEqual(len(m),9)
    for a in m:self.assertEqual(a['pixels'],76800);self.assertGreater(a['max'],255);self.assertTrue(a['exact_copy'])
if __name__=='__main__':unittest.main()
