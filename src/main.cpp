#include <psp2/display.h>
#include <psp2/kernel/sysmem.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/ctrl.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "retroarch_manager.hpp"
#include "source_pairing.hpp"
#include "vendor/qrcodegen.h"

static const int W=960,H=544,STRIDE=960;
static uint32_t* fb=nullptr; static SceUID fb_uid=-1;
static SceDisplayFrameBuf dfb;
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
static int screenMode=0; // 0 sources, 1 source detail, 2 search, 3 pairing, 4 demo, 5 deploy
static int sourceSelected=0, menuSelected=1, gameSelected=0, rowSelected=0;
static bool globalSearch=false;
static bool coresReady=false;
static char launchMessage[96]="RETROARCH: VERIFICATION...";
static char deployMessage[96]="X POUR TELECHARGER ET DEPLOYER RETROARCH";
static const char* menuItems[7]={"SOURCE","RECHERCHE & FILTRES","ACCUEIL","JEUX","EMULATEURS","HOMEBREWS","PARAMETRES"};

static void status(const char*m){rect(0,H-24,W,24,rgb(14,14,14));text(18,H-17,m,MUTED,1);}
static void sideIcon(int y,const char*label,bool sel){if(sel){rect(12,y-8,194,42,PANEL2);border(12,y-8,194,42,2,WHITE);}text(34,y,label,sel?TEXT:MUTED,1);}
static void sidebar(){rect(0,0,220,H,SIDEBAR);text(30,18,"VITARETRO",TEXT,2);text(30,38,"0.1 DEV",MUTED,1);for(int i=1;i<7;i++)sideIcon(84+i*56,menuItems[i],menuSelected==i);const char* name=vrSource(sourceSelected).name;text(30,68,*name?name:"SOURCE VIDE",ACCENT,1);}
static void cover(int x,int y,int w,int h,const char*title,const char*plat,bool sel,int tint){rect(x,y,w,h,rgb(38+tint,42+tint/2,48));if(sel)border(x-3,y-3,w+6,h+6,3,WHITE);rect(x+10,y+10,w-20,h-45,rgb(50+tint/2,60,68+tint));text(x+12,y+h-29,plat,MUTED,1);text(x+12,y+h-14,title,TEXT,1);}
static void row(int y,const char*title,int rowIndex){text(248,y,title,TEXT,2);int x=250;for(int i=0;i<6;i++){char n[24];snprintf(n,sizeof(n),"JEU %d",i+1);cover(x,y+28,103,122,n,(i%2)?"SNES":"MEGADRIVE",rowSelected==rowIndex&&gameSelected==i,i*4);x+=115;}}

static void sourcePicker(){clear(BG);text(286,72,"CHOISIR UNE SOURCE",TEXT,3);text(325,108,"UNE URL GLOBALE PAR SITE",MUTED,1);int x=94;for(int i=0;i<5;i++){int y=180,w=145,h=170;rect(x,y,w,h,PANEL);if(i==sourceSelected)border(x-4,y-4,w+8,h+8,4,WHITE);rect(x+22,y+22,w-44,92,PANEL2);text(x+48,y+58,*vrSource(i).url?"OK":"+",TEXT,3);char label[25];snprintf(label,sizeof(label),"SOURCE %d",i+1);text(x+24,y+126,label,TEXT,1);char name[19];snprintf(name,sizeof(name),"%s",*vrSource(i).url?vrSource(i).name:"AJOUTER URL");text(x+24,y+144,name,MUTED,1);x+=168;}text(94,396,coresReady?"RETROARCH : COEURS DEPLOYES":"RETROARCH : SELECT POUR DEPLOYER",ACCENT,2);status("X SOURCE  CARRE MODIFIER URL  SELECT DEPLOYER RETROARCH  TRIANGLE DEMO");}

static void pairingScreen(){clear(BG);text(70,50,"AJOUTER UNE SOURCE",TEXT,3);char label[40];snprintf(label,sizeof(label),"EMPLACEMENT %d - URL GLOBALE DU SITE",sourceSelected+1);text(72,96,label,ACCENT,2);const uint8_t* matrix=vrPairQr();if(matrix){int size=qrcodegen_getSize(matrix),step=size>41?4:5,x=565,y=135;rect(x-20,y-20,(size+8)*step,(size+8)*step,WHITE);for(int yy=0;yy<size;yy++)for(int xx=0;xx<size;xx++)if(qrcodegen_getModule(matrix,xx,yy))rect(x+xx*step,y+yy*step,step,step,BG);}text(72,159,"1. CONNECTE TON TELEPHONE AU MEME WIFI",TEXT,1);text(72,184,"2. SCANNE LE QR ET COLLE L URL DU SITE",TEXT,1);text(72,209,"3. APPUIE SUR ENREGISTRER",TEXT,1);text(72,260,vrPairStatus(),ACCENT,2);text(72,307,"UNE SEULE URL POUR TOUTE LA SOURCE",MUTED,1);text(72,330,"LE CATALOGUE DU SITE NECESSITE UN ADAPTATEUR",MUTED,1);text(72,445,vrPairUrl(),TEXT,1);status(vrPairReceived()?"URL ENREGISTREE - X RETOUR AUX SOURCES  O FERMER":"O FERMER  TELEPHONE ET VITA SUR LE MEME WIFI");}

static void home(){clear(BG);sidebar();text(248,24,"SOURCE CONFIGUREE",TEXT,3);text(248,56,vrSource(sourceSelected).name,ACCENT,2);text(248,88,vrSource(sourceSelected).url,TEXT,1);text(248,138,"URL ENREGISTREE SUR CETTE VITA",TEXT,2);text(248,172,"CATALOGUE ET RECHERCHE A CONNECTER AU SITE",MUTED,1);text(248,207,"AUCUN JEU TELECHARGE AUTOMATIQUEMENT",MUTED,1);text(248,260,"RETROARCH : CORES PS VITA INTEGRES",TEXT,2);text(248,293,launchMessage,MUTED,1);text(248,340,"CARRE : MODIFIER L URL DE CETTE SOURCE",TEXT,1);status("CARRE MODIFIER URL  TRIANGLE RECHERCHE  O SOURCES");}

static void toggle(int x,int y,bool on){rect(x,y,64,24,on?ACCENT:rgb(92,92,92));rect(on?x+42:x+4,y+4,18,16,WHITE);}
static void chip(int x,int y,const char*s,bool active){int w=tw(s,1)+24;rect(x,y,w,28,active?PANEL2:PANEL);border(x,y,w,28,1,active?WHITE:rgb(72,72,72));text(x+12,y+10,s,active?TEXT:MUTED,1);}
static void searchScreen(){clear(BG);sidebar();menuSelected=1;text(248,24,"RECHERCHE & FILTRES",TEXT,3);text(248,88,"RECHERCHE GLOBALE",TEXT,2);toggle(480,82,globalSearch);text(248,146,"AUCUN CATALOGUE CONNECTE AUX SOURCES",MUTED,2);text(248,184,"L AJOUT D URL EST DISPONIBLE DEPUIS SOURCES",MUTED,1);status("SELECT GLOBAL  O RETOUR");}
static void demoScreen(){clear(BG);text(248,24,"DEMO LOCALE - AUCUN JEU INCLUS",TEXT,2);row(88,"FICHIERS DE TEST PERSONNELS",0);text(248,310,"PLACE DEMO.SFC OU DEMO.MD DANS UX0:/DATA/VITARETRO/ROMS",MUTED,1);text(248,448,launchMessage,MUTED,1);status("X TESTER SON FICHIER LOCAL  O SOURCES");}
static void deployScreen(){clear(BG);text(76,67,"DEPLOYER RETROARCH VITA",TEXT,3);text(76,122,"TELECHARGEMENT OFFICIEL PREPARE DANS LE CLOUD",TEXT,1);text(76,158,"SIX COEURS ET DONNEES INSTALLES AUTOMATIQUEMENT",MUTED,1);text(76,237,deployMessage,ACCENT,2);text(76,292,"UNE CONNEXION WIFI ET DE L ESPACE LIBRE SONT NECESSAIRES",MUTED,1);text(76,325,"SI LE TELECHARGEMENT ECHOUE, TU PEUX REESSAYER",MUTED,1);status("X DEPLOYER  O RETOUR AUX SOURCES");}
static void dataProgress(const char* stage,uint64_t completed,uint64_t total){static uint64_t last=0;static const char* previous=nullptr;if(stage!=previous||completed<last){last=0;previous=stage;}if(stage[0]=='T'&&completed<total&&completed<last+256*1024)return;last=completed;clear(BG);text(80,124,"DEPLOIEMENT RETROARCH VITA",TEXT,2);text(80,165,stage,ACCENT,2);rect(80,238,800,30,PANEL2);if(total)rect(80,238,(int)(completed*800/total),30,ACCENT);char label[80];if(total)snprintf(label,sizeof(label),"%llu / %llu",(unsigned long long)completed,(unsigned long long)total);else snprintf(label,sizeof(label),"%llu OCTETS",(unsigned long long)completed);text(80,294,label,TEXT,2);sceDisplaySetFrameBuf(&dfb,SCE_DISPLAY_SETBUF_NEXTFRAME);sceDisplayWaitVblankStart();}

int main() {
  vrSourcesLoad();
  coresReady=vrRetroArchPayloadPresent();
  snprintf(launchMessage, sizeof(launchMessage), "RETROARCH: %s",
           coresReady ? "COEURS PRESENTS - LANCEMENT A TESTER" : "A DEPLOYER AVEC SELECT");
  SceKernelAllocMemBlockOpt opt;
  memset(&opt, 0, sizeof(opt));
  opt.size = sizeof(opt);
  opt.attr = SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_ALIGNMENT;
  opt.alignment = 256 * 1024;
  fb_uid = sceKernelAllocMemBlock("VitaRetroFB", SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW,
                                   STRIDE * H * 4, &opt);
  if (fb_uid < 0) return -1;
  void* base = nullptr;
  sceKernelGetMemBlockBase(fb_uid, &base);
  fb = (uint32_t*)base;
  memset(&dfb, 0, sizeof(dfb));
  dfb.size = sizeof(dfb);
  dfb.base = fb;
  dfb.pitch = STRIDE;
  dfb.pixelformat = SCE_DISPLAY_PIXELFORMAT_A8B8G8R8;
  dfb.width = W;
  dfb.height = H;
  sceDisplaySetFrameBuf(&dfb, SCE_DISPLAY_SETBUF_NEXTFRAME);
  sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG);
  SceCtrlData pad;
  memset(&pad, 0, sizeof(pad));
  unsigned old = 0;
  bool run = true;
  while (run) {
    sceCtrlPeekBufferPositive(0, &pad, 1);
    unsigned p = pad.buttons & ~old;
    old = pad.buttons;
    if (p & SCE_CTRL_START) run = false;
    if (screenMode == 0) {
      if (p & SCE_CTRL_LEFT) sourceSelected = (sourceSelected + 4) % 5;
      if (p & SCE_CTRL_RIGHT) sourceSelected = (sourceSelected + 1) % 5;
      if (p & SCE_CTRL_SQUARE) { vrPairStart(sourceSelected); screenMode = 3; }
      if (p & SCE_CTRL_SELECT) screenMode = 5;
      if (p & SCE_CTRL_CROSS) {
        if (*vrSource(sourceSelected).url) screenMode = 1;
        else { vrPairStart(sourceSelected); screenMode = 3; }
      }
      if (p & SCE_CTRL_TRIANGLE) screenMode = 4;
    } else if (screenMode == 1) {
      if (p & SCE_CTRL_SQUARE) { vrPairStart(sourceSelected); screenMode = 3; }
      if (p & SCE_CTRL_TRIANGLE) screenMode = 2;
      if (p & SCE_CTRL_CIRCLE) screenMode = 0;
    } else if (screenMode == 2) {
      if (p & SCE_CTRL_SELECT) globalSearch = !globalSearch;
      if (p & SCE_CTRL_CIRCLE) screenMode = 1;
    } else if (screenMode == 3) {
      vrPairPump();
      if ((p & SCE_CTRL_CIRCLE) || ((p & SCE_CTRL_CROSS) && vrPairReceived())) {
        vrPairStop(); screenMode = 0;
      }
    } else if (screenMode == 4) {
      if (p & SCE_CTRL_LEFT) gameSelected = (gameSelected + 5) % 6;
      if (p & SCE_CTRL_RIGHT) gameSelected = (gameSelected + 1) % 6;
      if (p & SCE_CTRL_CROSS) {
        const char* demo = (gameSelected % 2) ?
          "ux0:/data/VitaRetro/roms/demo.sfc" : "ux0:/data/VitaRetro/roms/demo.md";
        char err[64];
        int lr = vrLaunchGame(demo, err, sizeof(err));
        if (lr < 0) snprintf(launchMessage, sizeof(launchMessage), "LANCEMENT: %s", err);
      }
      if (p & SCE_CTRL_CIRCLE) screenMode = 0;
    } else if (screenMode == 5) {
      if (p & SCE_CTRL_CROSS) {
        char error[80];
        int result = vrDeployRetroArch(dataProgress, error, sizeof(error));
        snprintf(deployMessage, sizeof(deployMessage), "%s",
                 result == 0 ? "RETROARCH DEPLOYE - LANCEMENT A TESTER" : error);
        if (result == 0) {
          coresReady=true;
          snprintf(launchMessage, sizeof(launchMessage),
                   "RETROARCH: COEURS PRESENTS - LANCEMENT A TESTER");
        }
      }
      if (p & SCE_CTRL_CIRCLE) screenMode = 0;
    }
    if (screenMode == 0) sourcePicker();
    else if (screenMode == 1) home();
    else if (screenMode == 2) searchScreen();
    else if (screenMode == 3) pairingScreen();
    else if (screenMode == 4) demoScreen();
    else deployScreen();
    sceDisplaySetFrameBuf(&dfb, SCE_DISPLAY_SETBUF_NEXTFRAME);
    sceDisplayWaitVblankStart();
  }
  vrPairStop();
  sceKernelFreeMemBlock(fb_uid);
  sceKernelExitProcess(0);
  return 0;
}
