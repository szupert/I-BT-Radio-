"""Rebuild the standalone Modern HiFi skin assets. Requires Pillow; no radio access."""
from pathlib import Path
import json, hashlib, struct, math, html, zipfile
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parent
COLORS = dict(background='#101518', panel='#192226', border='#344247', text='#f2f0e9', muted='#a8b7bb', accent='#53d4bf', inactive='#38464a')
FONT = Path('C:/Windows/Fonts/segoeui.ttf')
BOLD = Path('C:/Windows/Fonts/segoeuib.ttf')
S = 3

class Canvas:
    def __init__(self,w,h):
        self.w,self.h=w,h
        self.im=Image.new('RGB',(w*S,h*S),COLORS['background'])
        self.d=ImageDraw.Draw(self.im)
        self.svg=[f'<svg xmlns="http://www.w3.org/2000/svg" width="{w}" height="{h}" viewBox="0 0 {w} {h}">',f'<rect width="{w}" height="{h}" fill="{COLORS["background"]}"/>']
    def rect(self,box,fill,outline=None,r=0):
        x,y,w,h=box
        self.d.rounded_rectangle((x*S,y*S,(x+w-1)*S,(y+h-1)*S),r*S,fill=fill,outline=outline,width=S)
        self.svg.append(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="{r}" fill="{fill}" stroke="{outline or "none"}"/>')
    def line(self,points,color,width=1):
        self.d.line([(x*S,y*S) for x,y in points],fill=color,width=width*S)
        self.svg.append(f'<polyline points="{" ".join(f"{x},{y}" for x,y in points)}" fill="none" stroke="{color}" stroke-width="{width}"/>')
    def polygon(self,points,color):
        self.d.polygon([(x*S,y*S) for x,y in points],fill=color)
        self.svg.append(f'<polygon points="{" ".join(f"{x},{y}" for x,y in points)}" fill="{color}"/>')
    def text(self,x,y,t,size=12,color=None,bold=False):
        color=color or COLORS['text']
        font=ImageFont.truetype(str(BOLD if bold else FONT),size*S)
        self.d.text((x*S,y*S),t,font=font,fill=color,anchor='lt')
        self.svg.append(f'<text x="{x}" y="{y}" dominant-baseline="text-before-edge" font-family="Segoe UI, sans-serif" font-size="{size}" font-weight="{700 if bold else 400}" fill="{color}">{html.escape(t)}</text>')
    def icon(self,name,x,y,size,color=None):
        color=color or COLORS['text']; u=size/24
        def pts(a):return [(x+xx*u,y+yy*u) for xx,yy in a]
        if name=='play':self.polygon(pts([(7,3),(21,12),(7,21)]),color)
        elif name in ('previous','next'):
            a=[(18,4),(7,12),(18,20)]
            if name=='next':a=[(24-xx,yy) for xx,yy in a]
            self.polygon(pts(a),color)
            self.rect((x+(4 if name=='previous' else 18)*u,y+4*u,2*u,16*u),color)
        elif name=='menu':
            for yy in (6,12,18):self.line(pts([(3,yy),(21,yy)]),color,2)
        elif name=='wifi':
            for rr in (8,14,20):
                a=[(12+rr*0.5*math.cos(math.radians(220+i)),23+rr*0.5*math.sin(math.radians(220+i))) for i in range(101)]
                self.line(pts(a),color,2)
            self.rect((x+11*u,y+21*u,2*u,2*u),color)
        elif name=='volume':
            self.polygon(pts([(3,9),(8,9),(14,3),(14,21),(8,15),(3,15)]),color)
            self.line(pts([(18,6),(21,10),(21,14),(18,18)]),color,2)
    def save(self,path):
        self.im.resize((self.w,self.h),Image.Resampling.LANCZOS).save(path.with_suffix('.png'))
        path.with_suffix('.svg').write_text('\n'.join(self.svg+['</svg>']),encoding='utf8')

def layout(kind):
    if kind=='big':
        return dict(size=[1024,600],header=[0,0,1024,58],artwork=[202,88,270,270],artist=[500,103,490,44],title=[500,158,490,40],clock=[872,16,140,30],station=[500,221,470,30],transport=[202,506,420,64],volume=[660,506,338,64],queue=[500,298,492,174],navigation=[12,78,168,492],status=[202,462,790,24])
    return dict(size=[320,240],header=[0,0,320,27],artwork=[4,32,136,136],artist=[148,83 if kind=='small' else 39,168,23],title=[148,111 if kind=='small' else 69,168,32],clock=[148,34,168,42] if kind=='small' else [264,7,54,16],station=[148,150 if kind=='small' else 109,168,16],transport=[4,194,312,42],status=[4,175,312,16],buffer=[4,190,312,1],vu=None if kind=='small' else [148,134,168,30])

def draw(kind,preview):
    l=layout(kind);w,h=l['size'];c=Canvas(w,h)
    c.line([(2,26),(318,26)] if kind!='big' else [(12,57),(1012,57)],COLORS['border'])
    c.text(4 if kind!='big' else 20,5 if kind!='big' else 12,'I-BT-Radio',15 if kind!='big' else 26,bold=True)
    if kind=='big':
        for i,t in enumerate(['Rádió','Zene','Kedvencek','Hang','Beállítások']):
            y=78+i*70;c.rect((12,y,168,56),COLORS['panel'],COLORS['accent'] if i==0 else COLORS['border'],6)
            c.text(26,y+15,t,21,color=COLORS['accent'] if i==0 else COLORS['text'])
        c.text(22,541,'OPEN PROJECT',12,COLORS['muted'])
    x,y,aw,ah=l['artwork'];c.rect((x,y,aw,ah),COLORS['panel'],COLORS['border'],4)
    if preview:
        # Deliberately synthetic artwork; not a baked album cover or production download.
        for j in range(ah-4):
            f=j/max(1,ah-5);color='#%02x%02x%02x'%(int(194-110*f),int(109-56*f),int(57-22*f))
            c.rect((x+2,y+2+j,aw-4,1),color)
        for i in range(7):
            xx=x+15+i*aw/8
            c.line([(xx,y+ah-2),(x+aw/2,y+ah*0.40+i*3)],'#e7b47c',1)
    else:
        c.icon('play',x+aw/2-12,y+ah/2-12,24,COLORS['inactive'])
    tr=l['transport']; bx,by,bw,bh=tr
    for i,ic in enumerate(['previous','play','next'] if kind=='big' else ['previous','play','next','menu']):
        n=3 if kind=='big' else 4;cell=bw/n;pad=4
        c.rect((bx+i*cell,by,cell-pad,bh), '#204c46' if i==1 else COLORS['panel'], COLORS['accent'] if i==1 else COLORS['border'],5)
        c.icon(ic,bx+i*cell+(cell-pad-24)/2,by+(bh-24)/2,24,COLORS['accent'] if i==1 else None)
    if kind!='big':
        c.line([(4,190),(315,190)],COLORS['inactive'])
        if kind=='pro':
            for row in range(2):
                c.text(148,134+row*15,'LR'[row],9,COLORS['muted'])
                for i in range(25):c.rect((160+i*6,136+row*15,4,8),COLORS['accent'] if preview and i<16-row*3 else COLORS['inactive'])
    else:
        c.line([(191,78),(191,570)],COLORS['border'])
        c.icon('volume',660,524,26);c.rect((702,533,234,5),COLORS['inactive'],r=2)
        if preview:c.rect((702,533,175,5),COLORS['accent'],r=2)
        c.text(954,517,'75%' if preview else 'VOL',21)
        c.line([(202,493),(998,493)],COLORS['border'])
    if preview:
        c.text(l['clock'][0],l['clock'][1],'19:42',32 if kind=='small' else 12 if kind=='pro' else 27,bold=True)
        c.icon('wifi',236 if kind!='big' else 834,4 if kind!='big' else 13,20 if kind!='big' else 26,COLORS['accent'])
        c.text(l['artist'][0],l['artist'][1],'Bonobo',19 if kind!='big' else 36,bold=True)
        c.text(l['title'][0],l['title'][1],'Kerala',16 if kind!='big' else 30)
        c.text(l['station'][0],l['station'][1],'RADIO · LIVE',10 if kind!='big' else 17,COLORS['muted'])
        c.text(l['status'][0],l['status'][1],'WiFi → DAC · MP3 · 128k · 75%',11 if kind!='big' else 17,COLORS['muted'])
        if kind!='big':c.line([(4,190),(238,190)],COLORS['accent'])
        if kind=='big':
            c.text(500,281,'Kedvenc állomások',19,COLORS['muted'])
            for i,t in enumerate(['01  Petőfi Rádió','02  Bartók Rádió','03  Jazz rádió']):
                c.rect((500,316+i*46,492,39),COLORS['panel'],COLORS['border'],4);c.text(514,322+i*46,t,18)
    return c

def export565(p):
    im=Image.open(p).convert('RGB');words=[((int(r)>>3)<<11)|((int(g)>>2)<<5)|(int(b)>>3) for r,g,b in im.getdata()]
    data=struct.pack('<'+'H'*len(words),*words);p.with_suffix('.rgb565le').write_bytes(data)
    decoded=[(((v>>11)&31)*255//31,((v>>5)&63)*255//63,(v&31)*255//31) for v in words]
    err=max(abs(a-b) for rgb,back in zip(im.getdata(),decoded) for a,b in zip(rgb,back))
    assert len(data)==im.width*im.height*2 and max(words)>255 and err<=7
    assert [((r>>3)<<11)|((g>>2)<<5)|(b>>3) for r,g,b in decoded]==words
    return dict(width=im.width,height=im.height,bytes=len(data),max_channel_error=err,rgb565_roundtrip='PASS',channel_order='RGB565',byte_order='little-endian')

def main():
    checks={}
    for kind in ['small','pro','big']:
        folder=ROOT/kind;folder.mkdir(exist_ok=True)
        for preview in [False,True]:
            draw(kind,preview).save(folder/('preview' if preview else 'background'))
        l=layout(kind)
        l.update(aw_status=[790,16,36,28] if kind=='big' else [214,7,20,14], aw_hide_at_percent=100, status='ASSETS_ONLY_NOT_FIRMWARE_INTEGRATED',artwork_fit='station: contain + edge extension; artist: cover crop',dynamic_fields=['clock','artist','title','station','status','buffer','vu','artwork'],touch_map=[dict(action=action,rect=[int(l['transport'][0]+i*l['transport'][2]/(3 if kind=='big' else 4)),l['transport'][1],int(l['transport'][2]/(3 if kind=='big' else 4))-4,l['transport'][3]]) for i,action in enumerate(['previous','play_pause','next'] if kind=='big' else ['previous','play_pause','next','menu'])])
        for r in [l['artwork'],*[t['rect'] for t in l['touch_map']]]:
            x,y,w,h=r;assert x>=0 and y>=0 and x+w<=l['size'][0] and y+h<=l['size'][1]
        (folder/'layout.json').write_text(json.dumps(l,indent=2,ensure_ascii=False)+'\n',encoding='utf8')
        checks[kind]=export565(folder/'background.png')
    icons=ROOT/'icons';icons.mkdir(exist_ok=True)
    for name in ['play','previous','next','menu','wifi','volume']:
        c=Canvas(24,24);c.icon(name,0,0,24,COLORS['accent']);c.save(icons/name)
    (ROOT/'theme.json').write_text(json.dumps(dict(name='Modern HiFi',version='1.0.0-assets',colors=COLORS,margin=4,small_pro_size=[320,240],big_size=[1024,600],buffer_height=1,animation_default='off',asset_status='not integrated into firmware'),indent=2)+'\n')
    (ROOT/'validation.json').write_text(json.dumps(dict(result='PASS_ASSET_VALIDATION',checks=checks,hardware_test='NOT_RUN',firmware_build='NOT_RUN',dynamic_rendering='NOT_INTEGRATED'),indent=2)+'\n')
    overview=Image.new('RGB',(1064,930),COLORS['background'])
    for name,pos,size in [('small',(20,20),(500,375)),('pro',(544,20),(500,375)),('big',(20,415),(1024,600))]:
        im=Image.open(ROOT/name/'preview.png');im=im.resize(size,Image.Resampling.LANCZOS)
        if name=='big':im=im.resize((853,500),Image.Resampling.LANCZOS)
        overview.paste(im,pos)
    overview.save(ROOT/'overview.png')
    files={p.relative_to(ROOT).as_posix():dict(bytes=p.stat().st_size,sha256=hashlib.sha256(p.read_bytes()).hexdigest()) for p in sorted(ROOT.rglob('*')) if p.is_file() and p.name not in ['manifest.json','Modern-HiFi-Skins.zip'] and '__pycache__' not in p.parts}
    (ROOT/'manifest.json').write_text(json.dumps(dict(version='1.0.0-assets',files=files),indent=2)+'\n')
    with zipfile.ZipFile(ROOT/'Modern-HiFi-Skins.zip','w',zipfile.ZIP_DEFLATED) as z:
        for p in sorted(ROOT.rglob('*')):
            if p.is_file() and p.suffix!='.zip' and '__pycache__' not in p.parts:z.write(p,'modern-hifi-v1/'+p.relative_to(ROOT).as_posix())
    print(json.dumps(checks))

if __name__=='__main__': main()
