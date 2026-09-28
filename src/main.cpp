#include <psp2/display.h>
#include <psp2/kernel/sysmem.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/ctrl.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "retroarch_manager.hpp"

static const int W=960,H=544,STRIDE=960;
static uint32_t* fb=nullptr; static SceUID fb_uid=-1;
static uint32_t rgb(uint8_t r,uint8_t g,uint8_t b){return 0xFF000000u|((uint32_t)b<<16)|((uint32_t)g<<8)|r;}
static void clear(uint32_t c){for(int i=0;i<STRIDE*H;i++)fb[i]=c;}
static void rect(int x,int y,int w,int h,uint32_t c){if(x<0){w+=x;x=0;}if(y<0){h+=y;y=0;}if(x+w>W)w=W-x;if(y+h>H)h=H-y;if(w<=0||h<=0)return;for(int yy=y;yy<y+h;yy++){uint32_t*p=fb+yy*STRIDE+x;for(int xx=0;xx<w;xx++)p[xx]=c;}}
static void border(int x,int y,int w,int h,int t,uint32_t c){rect(x,y,w,t,c);rect(x,y+h-t,w,t,c);rect(x,y,t,h,c);rect(x+w-t,y,t,h,c);}

struct Glyph{char ch;uint8_t r[7];};
static const Glyph FONT[]={
{'A',{14,17,17,31,17,17,17}},{'B',{30,17,17,30,17,17,30}},{'C',{14,17,16,16,16,17,14}},{'D',{30,17,17,17,17,17,30}},{'E',{31,16,16,30,16,16,31}},{'F',{31,16,16,30,16,16,16}},{'G',{14,17,16,23,17,17,14}},{'H',{17,17,17,31,17,17,17}},{'I',{31,4,4,4,4,4,31}},{'J',{7,2,2,2,18,18,12}},{'K',{17,18,20,24,20,18,17}},{'L',{16,16,16,16,16,16,31}},{'M',{17,27,21,21,17,17,17}},{'N',{17,25,21,19,17,17,17}},{'O',{14,17,17,17,17,17,14}},{'P',{30,17,17,30,16,16,16}},{'Q',{14,17,17,17,21,18,13}},{'R',{30,17,17,30,20,18,17}},{'S',{15,16,16,14,1,1,30}},{'T',{31,4,4,4,4,4,4}},{'U',{17,17,17,17,17,17,14}},{'V',{17,17,17,17,17,10,4}},{'W',{17,17,17,21,21,21,10}},{'X',{17,17,10,4,10,17,17}},{'Y',{17,17,10,4,4,4,4}},{'Z',{31,1,2,4,8,16,31}},
{'0',{14,17,19,21,25,17,14}},{'1',{4,12,4,4,4,4,14}},{'2',{14,17,1,2,4,8,31}},{'3',{30,1,1,14,1,1,30}},{'4',{2,6,10,18,31,2,2}},{'5',{31,16,16,30,1,1,30}},{'6',{14,16,16,30,17,17,14}},{'7',{31,1,2,4,8,8,8}},{'8',{14,17,17,14,17,17,14}},{'9',{14,17,17,15,1,1,14}},
{'-',{0,0,0,31,0,0,0}},{'.',{0,0,0,0,0,12,12}},{':',{0,12,12,0,12,12,0}},{'/',{1,2,4,8,16,0,0}},{'&',{12,18,20,8,21,18,13}},{'_',{0,0,0,0,0,0,31}},{'+',{0,4,4,31,4,4,0}},{'?',{14,17,1,2,4,0,4}},{' ',{0,0,0,0,0,0,0}}
};
static const uint8_t* glyph(char c){if(c>='a'&&c<='z')c=(char)(c-'a'+'A');for(unsigned i=0;i<sizeof(FONT)/sizeof(FONT[0]);i++)if(FONT[i].ch==c)return FONT[i].r;for(unsigned i=0;i<sizeof(FONT)/sizeof(FONT[0]);i++)if(FONT[i].ch=='?')return FONT[i].r;return nullptr;}
static void text(int x,int y,const char*s,uint32_t c,int scale=2){int ox=x;for(;*s;s++){if(*s=='\n'){y+=8*scale;x=ox;continue;}const uint8_t*g=glyph(*s);if(g)for(int yy=0;yy<7;yy++)for(int xx=0;xx<5;xx++)if(g[yy]&(1<<(4-xx)))rect(x+xx*scale,y+yy*scale,scale,scale,c);x+=6*scale;}}
static int tw(const char*s,int scale=2){return(int)strlen(s)*6*scale;}

static const uint32_t BG=0xFF181818u, SIDEBAR=0xFF151515u, PANEL=0xFF2B2B2Bu, PANEL2=0xFF3A3A3Au, TEXT=0xFFF3F3F3u, MUTED=0xFFB0B0B0u, ACCENT=0xFF6EE0A8u, WHITE=0xFFFFFFFFu;
static int screenMode=0; // 0 source picker, 1 home, 2 search
static int sourceSelected=0, menuSelected=1, gameSelected=0, rowSelected=0;
static bool globalSearch=false;
static char launchMessage[96]="RETROARCH: VERIFICATION...";
static const char* sources[5]={"SOURCE 1","SOURCE 2","SOURCE 3","SOURCE 4","SOURCE 5"};
static const char* menuItems[7]={"SOURCE","RECHERCHE & FILTRES","ACCUEIL","JEUX","EMULATEURS","HOMEBREWS","PARAMETRES"};

static void status(const char*m){rect(0,H-24,W,24,rgb(14,14,14));text(18,H-17,m,MUTED,1);}
static void sideIcon(int y,const char*label,bool sel){if(sel){rect(12,y-8,194,42,PANEL2);border(12,y-8,194,42,2,WHITE);}text(34,y,label,sel?TEXT:MUTED,1);}
static void sidebar(){rect(0,0,220,H,SIDEBAR);text(30,18,"VITARETRO",TEXT,2);text(30,38,"0.1 DEV",MUTED,1);for(int i=1;i<7;i++)sideIcon(84+i*56,menuItems[i],menuSelected==i);text(30,68,sources[sourceSelected],ACCENT,1);}
static void cover(int x,int y,int w,int h,const char*title,const char*plat,bool sel,int tint){rect(x,y,w,h,rgb(38+tint,42+tint/2,48));if(sel)border(x-3,y-3,w+6,h+6,3,WHITE);rect(x+10,y+10,w-20,h-45,rgb(50+tint/2,60,68+tint));text(x+12,y+h-29,plat,MUTED,1);text(x+12,y+h-14,title,TEXT,1);}
static void row(int y,const char*title,int rowIndex){text(248,y,title,TEXT,2);int x=250;for(int i=0;i<6;i++){char n[24];snprintf(n,sizeof(n),"JEU %d",i+1);cover(x,y+28,103,122,n,(i%2)?"SNES":"MEGADRIVE",rowSelected==rowIndex&&gameSelected==i,i*4);x+=115;}}

static void sourcePicker(){clear(BG);text(286,72,"CHOISIR UNE SOURCE",TEXT,3);text(325,108,"5 EMPLACEMENTS CONFIGURABLES",MUTED,1);int x=94;for(int i=0;i<5;i++){int y=180,w=145,h=170;rect(x,y,w,h,PANEL);if(i==sourceSelected)border(x-4,y-4,w+8,h+8,4,WHITE);rect(x+22,y+22,w-44,92,PANEL2);text(x+48,y+58,"+",TEXT,3);text(x+24,y+126,sources[i],TEXT,1);text(x+24,y+144,"NON CONFIGUREE",MUTED,1);x+=168;}status("GAUCHE/DROITE SOURCE  X OUVRIR  TRIANGLE ACCUEIL DEMO  START QUITTER");}

static void home(){clear(BG);sidebar();text(248,24,"ACCUEIL",TEXT,3);text(248,52,"CONTENU DE LA SOURCE ACTIVE",MUTED,1);row(88,"NOUVEAUTES ET AJOUTS RECENTS",0);row(270,"POPULAIRES",1);text(248,448,launchMessage,MUTED,1);text(248,466,"X JOUER - DETECTION AUTO DU CORE",MUTED,1);status("HAUT/BAS RUBRIQUE  GAUCHE/DROITE JEU  X JOUER  TRIANGLE RECHERCHE & FILTRES  O SOURCES");}

static void toggle(int x,int y,bool on){rect(x,y,64,24,on?ACCENT:rgb(92,92,92));rect(on?x+42:x+4,y+4,18,16,WHITE);}
static void chip(int x,int y,const char*s,bool active){int w=tw(s,1)+24;rect(x,y,w,28,active?PANEL2:PANEL);border(x,y,w,28,1,active?WHITE:rgb(72,72,72));text(x+12,y+10,s,active?TEXT:MUTED,1);}
static void searchScreen(){clear(BG);sidebar();menuSelected=1;text(248,24,"RECHERCHE & FILTRES",TEXT,3);rect(248,62,620,46,PANEL2);text(270,78,"RECHERCHER UN JEU...",MUTED,2);text(248,132,"RECHERCHE GLOBALE",TEXT,2);toggle(480,126,globalSearch);text(248,178,"FILTRES",TEXT,2);chip(248,210,"PLATEFORME",true);chip(370,210,"GENRE",false);chip(448,210,"ANNEE",false);chip(526,210,"LANGUE",false);chip(624,210,"TOUT-PETITS",false);chip(748,210,"A-Z",false);text(248,262,"RESULTATS PAR SOURCE",TEXT,2);for(int s=0;s<3;s++){char ss[24];snprintf(ss,sizeof(ss),"SOURCE %d",s+1);text(248,302+s*70,ss,ACCENT,1);for(int i=0;i<4;i++){int x=340+i*130;rect(x,288+s*70,116,52,PANEL);text(x+10,302+s*70,"JEU TEST",TEXT,1);text(x+10,320+s*70,(i%2)?"SNES":"PS1",MUTED,1);}}status("SELECT GLOBAL  X FILTRE  O RETOUR");}

int main(){snprintf(launchMessage,sizeof(launchMessage),"RETROARCH: %s",vrRetroArchPayloadPresent()?"CORES PRESENTS - LANCEMENT A TESTER":"CORES ABSENTS DU BUILD"); SceKernelAllocMemBlockOpt opt;memset(&opt,0,sizeof(opt));opt.size=sizeof(opt);opt.attr=SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_ALIGNMENT;opt.alignment=256*1024;fb_uid=sceKernelAllocMemBlock("VitaRetroFB",SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW,STRIDE*H*4,&opt);if(fb_uid<0)return-1;void*base=nullptr;sceKernelGetMemBlockBase(fb_uid,&base);fb=(uint32_t*)base;SceDisplayFrameBuf dfb;memset(&dfb,0,sizeof(dfb));dfb.size=sizeof(dfb);dfb.base=fb;dfb.pitch=STRIDE;dfb.pixelformat=SCE_DISPLAY_PIXELFORMAT_A8B8G8R8;dfb.width=W;dfb.height=H;sceDisplaySetFrameBuf(&dfb,SCE_DISPLAY_SETBUF_NEXTFRAME);sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG);SceCtrlData pad;memset(&pad,0,sizeof(pad));unsigned old=0;bool run=true;while(run){sceCtrlPeekBufferPositive(0,&pad,1);unsigned p=pad.buttons&~old;old=pad.buttons;if(p&SCE_CTRL_START)run=false;if(screenMode==0){if(p&SCE_CTRL_LEFT)sourceSelected=(sourceSelected+4)%5;if(p&SCE_CTRL_RIGHT)sourceSelected=(sourceSelected+1)%5;if(p&SCE_CTRL_CROSS)screenMode=1;if(p&SCE_CTRL_TRIANGLE)screenMode=1;}else if(screenMode==1){if(p&SCE_CTRL_LEFT)gameSelected=(gameSelected+5)%6;if(p&SCE_CTRL_RIGHT)gameSelected=(gameSelected+1)%6;if(p&SCE_CTRL_UP)rowSelected=(rowSelected+1)%2;if(p&SCE_CTRL_DOWN)rowSelected=(rowSelected+1)%2;if(p&SCE_CTRL_CROSS){const char* demo=(gameSelected%2)?"ux0:/data/VitaRetro/roms/demo.sfc":"ux0:/data/VitaRetro/roms/demo.md";char err[64];int lr=vrLaunchGame(demo,err,sizeof(err));if(lr<0)snprintf(launchMessage,sizeof(launchMessage),"LANCEMENT: %s",err);}if(p&SCE_CTRL_TRIANGLE){screenMode=2;menuSelected=1;}if(p&SCE_CTRL_CIRCLE){screenMode=0;menuSelected=2;}}else{if(p&SCE_CTRL_SELECT)globalSearch=!globalSearch;if(p&SCE_CTRL_CIRCLE){screenMode=1;menuSelected=2;}}if(screenMode==0)sourcePicker();else if(screenMode==1)home();else searchScreen();sceDisplaySetFrameBuf(&dfb,SCE_DISPLAY_SETBUF_NEXTFRAME);sceDisplayWaitVblankStart();}
sceKernelFreeMemBlock(fb_uid);sceKernelExitProcess(0);return 0;}
