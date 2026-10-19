// Original Kajon-only renderer extracted from SMALL R165; shared host interfaces are described in README.
static void kajonMenuFrame(const char* title){
    lcd.drawRoundRect(1,1,318,238,5,0x8410);
    lcd.drawRoundRect(3,3,314,234,4,0xc638);lcd.fillRect(5,5,310,19,0x18c3);
    textSmall(12,8,220,title,0x05ff,0x18c3);
}

static void kajonMeter(unsigned channel,unsigned position,int previous){
    const uint16_t* face=channel?ibtkajon::faceR:ibtkajon::faceL;
    if(previous>=0){
        const auto old=kajon::needles[previous>32?32:previous];
        const int x=64+std::min(int(old.x0),int(old.x1));
        const int y=80-std::max(int(old.y0),int(old.y1));
        const int w=abs(int(old.x1)-int(old.x0))+1;
        const int h=abs(int(old.y1)-int(old.y0))+1;
        uint16_t strip[48*8];
        for(int row=0;row<h;row+=8){
            const int rows=std::min(8,h-row);
            for(int yy=0;yy<rows;++yy)memcpy(strip+yy*w,face+(y+row+yy)*128+x,w*sizeof(uint16_t));
            lcd.drawRGBBitmap(ibtkajon::faceX[channel]+x,ibtkajon::faceY+y+row,strip,w,rows);
        }
    }
    const auto n=kajon::needles[position>32?32:position];const int cx=ibtkajon::faceX[channel]+64,cy=104;
    lcd.drawLine(cx+n.x0,cy-n.y0,cx+n.x1,cy-n.y1,0xff59);
    lcd.fillCircle(cx,cy,11,0x1082);lcd.drawCircle(cx,cy,11,0x4208);
}

static void kajonHome(const dual::Snapshot& s,bool layout){
    // Home owns these existing cache slots; structural transitions reset them.
    int& oldL=uiCache->widths[8];int& oldR=uiCache->widths[9];int& oldVolume=uiCache->widths[10];
    int& oldPlaying=uiCache->widths[11];int& oldSource=uiCache->widths[12];
    if(layout){drawPlayer(ibtkajon::background);oldL=oldR=oldVolume=oldPlaying=oldSource=-1;
        memset(rows,0,16*192);
        // Keep all navigation available without replacing the original controls.
        textSmall(16,229,60,"MENU",0xc638,0);textSmall(85,229,68,"BT IN/OUT",0xc638,0);
    }
    if(layout||oldSource!=int(s.sd)){lcd.fillRect(240,227,80,13,0);textSmall(248,229,65,s.sd?"RADIO":"SD",0xc638,0);oldSource=int(s.sd);}
    const bool enabled=vuprobe::dacEnabled.load(std::memory_order_relaxed)||vuprobe::localBtEnabled.load(std::memory_order_relaxed);
    const unsigned left=s.playing&&enabled?vuprobe::dacLeft.load(std::memory_order_relaxed):0;
    const unsigned right=s.playing&&enabled?vuprobe::dacRight.load(std::memory_order_relaxed):0;
    const int l=kajon::index(left),r=kajon::index(right);
    if(layout||l!=oldL){kajonMeter(0,l,oldL);oldL=l;}if(layout||r!=oldR){kajonMeter(1,r,oldR);oldR=r;}
    auto field=[&](unsigned id,int y,const char* value,uint16_t color){
        if(layout||strncmp(rows[id],value,sizeof(rows[id])-1)){strlcpy(rows[id],value,sizeof(rows[id]));
            lcd.fillRect(16,y,287,14,0);textSmall(17,y+2,284,value,color,0);}
    };
    field(0,119,s.station[0]?s.station:(s.sd?"SD":"I-BT-Radio"),0x05ff);
    field(1,137,uiCache->trackLine,0xc638);
    const int volume=constrain((int(s.volume)*100+127)/254,0,100);
    if(layout||volume!=oldVolume){lcd.fillRoundRect(208,197,99,4,2,0x2104);lcd.fillRoundRect(208,197,99*volume/100,4,2,0xc638);oldVolume=volume;}
    if(layout||oldPlaying!=int(s.playing)){
        lcd.fillCircle(74,194,11,0x1082);
        if(s.playing){lcd.fillRect(69,187,3,14,0x05ff);lcd.fillRect(77,187,3,14,0x05ff);}
        else lcd.fillTriangle(70,186,70,202,81,194,0x05ff);
        oldPlaying=int(s.playing);
    }
}
