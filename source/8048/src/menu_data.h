#pragma once
#include "radio/ui/content.h"
#include <stdlib.h>
struct Row {const char* label;int target;const char* values[9];int count;};
struct Page {const char* title;int parent;Row* rows;int count;};
static constexpr int sourceIds[]={1,2,3,46,45,44,43,4,5,6,7,8,9,53,54,55,47,56,52,49,50,10,11,12,13,20,19,14,15,57,58,59,60,61,62,63,64,16,18,21,22,23,24,25,26,27,28,29,30,31,32,33,34,37,38};
constexpr int pageCount=sizeof(sourceIds)/sizeof(sourceIds[0]);
static Page pages[pageCount];static int totalRows=0;
static dual::Snapshot demo;
inline int pageIndex(int id){for(int i=0;i<pageCount;i++)if(sourceIds[i]==id)return i;return -1;}
inline void buildMenu(){
 static const int links[][3]={{1,0,8},{1,1,6},{1,2,3},{1,3,7},{6,0,14},{6,1,15},{6,2,16},{6,3,3},{3,4,43},{7,0,9},{7,1,10},{7,2,11},{7,3,12},{7,4,19},{7,5,13},{7,6,45},{7,7,46},{7,8,49},{7,9,59},{9,4,53},{53,1,54},{53,2,54},{10,4,47},{47,9,56},{10,9,52},{49,1,50},{19,4,37},{59,1,60},{59,2,61},{59,3,62},{59,4,63},{59,5,64},{15,3,57},{57,10,58},{8,0,4},{8,1,29},{8,2,31},{8,3,30},{8,4,33},{9,0,28},{9,1,21},{9,2,18},{11,2,27},{12,0,2},{12,1,30},{12,2,25},{12,3,24},{19,1,20},{19,2,26},{13,1,2},{13,2,18},{13,3,5},{13,4,34},{3,1,44},{16,0,23},{16,2,44},{16,3,22},{3,3,22}};
 for(int i=0;i<pageCount;i++){
  int id=sourceIds[i];auto& pg=pages[i];pg.title=content::pageTitle(dual::Language::HU,id);pg.parent=0;
  pg.count=content::itemCount(id);if(!pg.count)pg.count=3;
  pg.rows=new Row[pg.count]{};totalRows+=pg.count;
  for(int j=0;j<pg.count;j++){
   auto& r=pg.rows[j];char line[160]={};content::menuItem(line,sizeof(line),demo,id,j);
   if(!strcmp(line,"?")||!line[0])snprintf(line,sizeof(line),"%s",j==pg.count-1?"Vissza":j==0?"Demó információ":"Minta / nincs hardverművelet");
   char* sep=strstr(line,": ");if(sep){*sep=0;r.values[0]=strdup(sep+2);}else r.values[0]="Demó";
   r.label=strdup(line);r.target=!strcmp(line,"Vissza")||!strcmp(line,"Mégse")?-3:-1;
   r.values[1]="Kiválasztva";r.count=r.target==-3?1:2;
   for(auto& link:links)if(link[0]==id&&link[1]==j){r.target=pageIndex(link[2]);r.values[0]="Megnyitás";if(r.target>=0)pages[r.target].parent=i;}
  }
 }
 for(int i=0;i<pageCount;i++)if(sourceIds[i]==10){auto& r=pages[i].rows[2];static const char* brightness[]={"Auto (demó)","10%","25%","50%","75%","100%"};for(int j=0;j<6;j++)r.values[j]=brightness[j];r.count=6;}
 // Secondary demo-list routes; source pages remain reachable through their original groups.
 for(int i=0;i<pageCount;i++){int id=sourceIds[i];if(id==31){pages[i].rows[1].label="Találatok";pages[i].rows[1].target=pageIndex(32);}
 if(id==53){pages[i].rows[1].target=pageIndex(54);pages[i].rows[2].target=pageIndex(54);}
 if(id==54){pages[i].rows[0].target=pageIndex(55);}
 if(id==37){pages[i].rows[0].target=pageIndex(38);}
 if(id==44){pages[i].rows[0].label="Poly VFocus (demó)";pages[i].rows[1].label="JBL Charge 5 (demó)";}
 if(id==30){pages[i].rows[0].label="Ébresztő / tititi.mp3";pages[i].rows[1].label="Zene / Minta.mp3";}
 if(id==4||id==29||id==32){pages[i].rows[0].label="Budapest Rock";pages[i].rows[1].label="Sláger FM";}
 if(id==28||id==21){pages[i].rows[0].label="Minta Wi-Fi 2,4 GHz";pages[i].rows[1].label="Minta Wi-Fi 2";}
 }
}
