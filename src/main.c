/* GLORY PAD 0.3.3 - Celestial Edition */
#include "fm1.h"
#define N 6
#define FULL 16777216
#define COUNT 20
static const char *const notes[]={"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
static const char *const names[]={"ANALOG SINE","CATHEDRAL","CLEAN ORGAN","DARK ANALOG","DARK UNO","DEFAULT","RESONANT","SOFT SQUARE","STRINGS","TRIANGLE CORE","HEAVEN OPEN","HOLY PRESENCE","ATMOSPHERE","ETERNAL","SOAKING","ABBA","HOLY STRINGS","ENDLESS GLORY","AMBIENT GUITAR","SECRET PLACE"};
typedef struct {int w[N],o[N],shape,detune;} Patch;
static const Patch patches[COUNT]={
{{9000,3500,4200,1800,900,500},{0,0,7,12,12,19},0,10},
{{6500,3500,4200,3300,2400,1800},{0,0,7,12,19,24},1,12},
{{6500,5000,4000,2800,2200,1200},{0,0,7,12,12,19},0,8},
{{8500,4500,3500,1600,900,500},{0,0,7,12,12,19},2,18},
{{10000,5000,2800,1200,600,300},{-12,0,7,12,12,19},0,5},
{{7000,4000,4200,2500,1800,900},{0,0,7,12,12,19},3,16},
{{6500,3500,4000,2800,2300,1200},{0,0,7,12,19,24},4,12},
{{7500,4000,4000,2000,1200,600},{0,0,7,12,12,19},5,15},
{{5200,4800,4200,3500,2400,1600},{0,0,7,12,12,19},6,25},
{{8500,4000,3600,2200,1200,600},{0,0,7,12,12,19},7,10},
{{4200,3300,4200,4400,2700,1600},{0,12,7,12,24,31},8,18},
{{7600,3200,4200,2800,1700,900},{0,-12,7,12,19,24},9,9},
{{5500,2500,3700,3500,3300,1400},{0,12,7,19,24,31},10,22},
{{6500,3200,4000,3700,2200,1100},{0,-12,7,12,24,19},8,15},
{{8500,1800,4000,2500,900,400},{0,0,7,12,19,24},11,4},
{{7200,2600,4200,2800,1200,700},{0,12,7,12,19,24},7,8},
{{5200,3500,4300,3900,2500,1400},{0,12,7,12,19,24},12,20},
{{4800,2600,3800,3600,3000,2400},{0,12,7,19,24,31},10,14},
{{6300,3000,4400,2800,1500,900},{0,12,7,12,19,24},13,9},
{{9500,1500,3500,1600,600,300},{0,-12,7,12,19,24},11,3}
};
typedef struct {uint32_t ph[N],inc[N];int32_t amp;int root,preset,oct,mode;uint32_t hp[3],hi[3],clock;int alternate;} Bank;
static Bank bank[2];static int current;
static volatile uint32_t request=(2u<<4)|(2u<<9);
static volatile int level=85,tone=55,motion=0,fade=5,quiet=1;
static int root=0,preset=2,oct=0,playing=0,theme=0;
static uint32_t lfo;
static int mode=0,page=0;
static volatile int verb=55,feedback=65,lowpass=127,highpass=0,harmonic=1,period=16,depth=85;
static int16_t dl[4093],dr[3907],el[2719],er[3251];
static unsigned il,ir,jl,jr;
static volatile int shimmer=20,chorus=20,space=50,pan=64;
static int16_t cl[1024],cr[1024];
static unsigned ci;
static uint32_t cp;
static int32_t dampL,dampR;
/* Preset defaults: reverb, tail, octave bloom, chorus, diffusion */
static const unsigned char fx[20][5]={
{35,55,8,15,30},{85,95,35,30,80},{30,50,5,12,25},{55,70,10,30,50},{45,65,5,15,40},{70,80,25,35,65},{60,75,20,25,55},{55,70,15,40,50},{75,85,20,55,65},{65,75,15,30,60},
{100,105,75,45,100},{90,100,40,30,85},{110,110,45,55,110},{95,100,65,45,95},{110,115,15,25,110},{90,100,20,30,90},{75,85,15,65,70},{115,115,100,45,115},{95,105,40,50,85},{90,110,8,15,100}};
static void preset_fx(int p){verb=fx[p][0];feedback=fx[p][1];shimmer=fx[p][2];chorus=fx[p][3];space=fx[p][4];}

static int32_t fl,fr,hl,hr;
static const char *const modes[]={"OPEN","MAJOR","MINOR"};
static const char *const motions[]={"STATIC","SUS4","SUS2","FLOAT"};

static const uint16_t colors[]={RGB(164,191,255),RGB(237,196,126),RGB(198,160,255),RGB(114,217,223),RGB(146,213,174),RGB(238,238,238),RGB(237,141,133),RGB(233,163,200)};
static const uint16_t backgrounds[]={RGB(8,15,32),RGB(24,17,8),RGB(20,12,32),RGB(7,25,30),RGB(9,26,18),RGB(16,16,16),RGB(32,12,12),RGB(32,14,26)};
static const char *const themes[]={"HEAVEN","GLORY","PRESENCE","OCEAN","EDEN","PURE","EMBER","ROSE"};
static int clamp(int x,int a,int b){return x<a?a:x>b?b:x;}
static int wrap(int x,int n){x%=n;return x<0?x+n:x;}
static void publish(void){request=(uint32_t)root|((uint32_t)preset<<4)|((uint32_t)(oct+2)<<9)|((uint32_t)playing<<12)|((uint32_t)mode<<13);}
static int32_t wave(uint32_t p,int sh,int br){int32_t a=sine(p),b=sine(p*2u),c=sine(p*3u);switch(sh){
case 1:return (a*8+b*2+c)/11;
case 2:return (a*10+b+c)/12;
case 3:return (a*8+b+c)/10;
case 4:return (a*6+b*2+c*2+sine(p*5u))/11;
case 5:return (a*9+c*2+sine(p*5u))/12;
case 6:return (a*8+b*4+c*2+sine(p*4u)+sine(p*5u))/16;
case 7:return (a*9-c)/10;
case 8:return (a*12+b*3+c*2+sine(p*4u))/18;
case 9:return (a*16+b*2-c)/19;
case 10:return (a*12+sine(p*4u)*2+sine(p*6u))/15;
case 11:return (a*24+b)/25;
case 12:return (a*10+b*5+c*3+sine(p*4u)*2+sine(p*5u))/21;
case 13:return (a*12+b*4-c*2+sine(p*5u))/19;
default:return (a*512+b*br)/(512+br);}}
static void tune(Bank *b,int r,int p,int o,int m){b->mode=m;b->clock=0;for(int h=0;h<3;h++){b->hp[h]=0;b->hi[h]=note_inc(clamp(48+r+o*12+(h==0?(m==2?3:4):h==1?5:2),0,127));}b->root=r;b->preset=p;b->oct=o;for(int j=0;j<N;j++){uint32_t inc=note_inc(clamp(36+r+o*12+patches[p].o[j],0,127));if(j==1)inc+=(inc/10000u)*patches[p].detune;if(j==4)inc-=(inc/10000u)*patches[p].detune;b->inc[j]=inc;}}
static int match(Bank *b,int r,int p,int o,int m){return b->mode==m&&b->root==r&&b->preset==p&&b->oct==o;}
static int32_t approach(int32_t a,int32_t b,int32_t s){return a<b?(a+s>b?b:a+s):a>b?(a-s<b?b:a-s):a;}
void audio(int32_t *out,uint32_t frames){uint32_t q=request;int r=q&15,p=(q>>4)&31,o=((q>>9)&7)-2,on=(q>>12)&1,m=(q>>13)&3;int vol=level,br=tone,dep=motion;int32_t step=FULL/(SAMPLE_RATE*fade);if(step<1)step=1;
if(on&&!match(&bank[current],r,p,o,m)){int next=1-current;if(match(&bank[next],r,p,o,m))current=next;else if(!bank[next].amp){tune(&bank[next],r,p,o,m);current=next;}}
for(uint32_t i=0;i<frames;i++){int32_t L=0,R=0,airL=0,airR=0;lfo+=6000u;int32_t mv=(sine(lfo)*dep)>>15;for(int k=0;k<2;k++){Bank *b=&bank[k];b->amp=approach(b->amp,on&&k==current?FULL:0,step);if(!b->amp)continue;int32_t l=0,rr=0;const Patch *pt=&patches[b->preset];for(int j=0;j<N;j++){b->ph[j]+=b->inc[j];int32_t w=pt->w[j];if(j>=3)w+=(w*mv)>>10;int32_t s=(wave(b->ph[j],pt->shape,br)*w)>>15;if(j==1||j==4){l+=s/2;rr+=s;}else if(j==2||j==5){l+=s;rr+=s/2;}else{l+=s;rr+=s;}}if(b->mode){
uint32_t cycle=(uint32_t)SAMPLE_RATE*period;if(++b->clock>=cycle){b->clock=0;b->alternate=!b->alternate;}
unsigned pos=(unsigned)(((uint64_t)b->clock*1024)/cycle);
int mix=0;if(harmonic){if(pos>=256&&pos<384)mix=(pos-256)*8;else if(pos>=384&&pos<640)mix=1024;else if(pos>=640&&pos<768)mix=(768-pos)*8;}
mix=mix*depth/127;int h=(harmonic==2||(harmonic==3&&b->alternate))?2:1;
for(int a=0;a<3;a++)b->hp[a]+=b->hi[a];
int32_t hs=(sine(b->hp[0])*(1024-mix)+sine(b->hp[h])*mix)>>10;
hs=(hs*3200)>>15;l+=hs;rr+=hs;
}int32_t g=b->amp>>9;
/* Octave-up synthesis fed only into the ambient return, not pitch-shifted feedback. */
if(shimmer){int32_t a=sine(b->ph[0]*2u)/5+sine(b->ph[2]*2u)/8;int32_t z=sine(b->ph[0]*4u)/10+sine(b->ph[2]*4u)/16;airL+=((a+z/2)*g)>>15;airR+=((a/2+z)*g)>>15;}
L+=(l*g)>>15;R+=(rr*g)>>15;}int lp=8+lowpass*lowpass/16;fl+=((L-fl)*lp)>>10;fr+=((R-fr)*lp)>>10;
int hp=1+highpass*highpass/64;hl+=((fl-hl)*hp)>>14;hr+=((fr-hr)*hp)>>14;
L=fl-(hl*highpass/127);R=fr-(hr*highpass/127);
/* Modulated short stereo delay. */
cp+=19000u;
unsigned tapL=320+((sine(cp)+32768)*90u>>16),tapR=410+((sine(cp+0x40000000u)+32768)*90u>>16);
int32_t chL=cl[(ci+1024-tapL)&1023],chR=cr[(ci+1024-tapR)&1023];
cl[ci]=clamp(L,-30000,30000);cr[ci]=clamp(R,-30000,30000);ci=(ci+1)&1023;
L=(L*(254-chorus)+chL*chorus)/254;R=(R*(254-chorus)+chR*chorus)/254;
int32_t a=dl[il],b=dr[ir],c=el[jl],d=er[jr];
int fb=35+feedback*55/127;
dampL+=((b+c)/2-dampL)*(150-space)/256;dampR+=((a+d)/2-dampR)*(150-space)/256;
int32_t sendL=L/2+airL*shimmer/254,sendR=R/2+airR*shimmer/254;
dl[il]=clamp(sendL+dampL*fb/100,-30000,30000);dr[ir]=clamp(sendR+dampR*fb/100,-30000,30000);
el[jl]=clamp(sendL/2+dampR*fb/100,-30000,30000);er[jr]=clamp(sendR/2+dampL*fb/100,-30000,30000);
if(++il==4093)il=0;if(++ir==3907)ir=0;if(++jl==2719)jl=0;if(++jr==3251)jr=0;
L=L*(254-verb)/254+(a+c)*verb/190;R=R*(254-verb)/254+(b+d)*verb/190;L=L*(pan>64?127-pan:63)/63;R=R*(pan<64?pan:64)/64;
out[2*i]+=clamp((L*vol)>>7,-30000,30000);out[2*i+1]+=clamp((R*vol)>>7,-30000,30000);}
quiet=!bank[0].amp&&!bank[1].amp;}
static void center(int y,const char*s,uint16_t c,int font){text((240-text_width(s,font))/2,y,s,c,font);}
static void ring(int radius,uint16_t c,int dotted){for(int i=0;i<128;i++){if(dotted&&i%3)continue;uint32_t p=(uint32_t)i<<25;int x=120+((sine(p)*radius)>>15),y=105+((sine(p+0x40000000u)*radius)>>15);pixel(x,y,c);}}
static void screen(void){uint16_t bg=backgrounds[theme],ac=colors[theme];clear(bg);center(4,"G L O R Y  P A D",ac,FONT_SMALL);center(25,names[preset],WHITE,FONT_SMALL);for(int i=0;i<52;i++){int x=8+(i*73)%224,y=46+(i*41)%115;pixel(x,y,ac);}ring(51,ac,1);ring(46,ac,0);ring(42,ac,1);rect(92,88,56,34,bg);center(89,notes[root],WHITE,FONT_BIG);text(8,87,"OCT",ac,FONT_SMALL);char v[12];format_int(v,oct);text(16,106,v,WHITE,FONT_SMALL);text(192,87,"V0.33",ac,FONT_SMALL);rect(60,152,120,18,bg);center(152,playing?"PAD ACTIVE":"STANDBY",ac,FONT_SMALL);center(170,modes[mode],ac,FONT_SMALL);line(8,188,232,188,ac);const char *labels[]={"LEVEL","TONE","MOTION","FADE"};int values[]={level,tone,motion,fade};if(page==1){labels[0]="REVERB";labels[1]="TAIL";labels[2]="LOW";labels[3]="HIGH";values[0]=verb;values[1]=feedback;values[2]=lowpass;values[3]=highpass;}if(page==2){labels[0]="HARM";labels[1]="CYCLE";labels[2]="DEPTH";labels[3]="MODE";values[0]=harmonic;values[1]=period;values[2]=depth;values[3]=mode;center(138,motions[harmonic],ac,FONT_SMALL);}if(page==3){labels[0]="SHIM";labels[1]="CHORUS";labels[2]="SPACE";labels[3]="PAN";values[0]=shimmer;values[1]=chorus;values[2]=space;values[3]=pan;}for(int k=0;k<4;k++){int x=k*60;format_int(v,values[k]);text(x+(60-text_width(labels[k],FONT_SMALL))/2,192,labels[k],ac,FONT_SMALL);text(x+(60-text_width(v,FONT_SMALL))/2,210,v,WHITE,FONT_SMALL);rect(x+5,232,(page==0&&k==3?values[k]*50/10:values[k]*50/127),2,ac);}}
void setup(void){uint8_t saved[2]={0,0};if(!settings_load(saved,sizeof saved)&&saved[0]==37&&saved[1]<8)theme=saved[1];tune(&bank[0],root,preset,oct,mode);tune(&bank[1],root,preset,oct,mode);preset_fx(preset);publish();screen();}
void loop(void){int changed=0;for(int k=0;k<KEY_COUNT;k++)if(key_pressed(k)){root=(KEY_FIRST_NOTE+k)%12;playing=1;changed=1;}uint8_t st,d1,d2;while(midi_read(&st,&d1,&d2))if((st&0xF0)==0x90&&d2){root=d1%12;playing=1;changed=1;}int d=encoder(ENC_SELECT);if(d){root=wrap(root+d,12);changed=1;}d=encoder(ENC_PRESETS);if(d){preset=wrap(preset+d,COUNT);preset_fx(preset);changed=1;}int o=clamp(oct+button_pressed(BTN_OCT_UP)-button_pressed(BTN_OCT_DOWN),-2,2);if(o!=oct){oct=o;changed=1;}if(button_pressed(BTN_PLAY)){playing=!playing;changed=1;}if(button_pressed(BTN_GLO)){theme=(theme+1)%8;changed=1;}if(button_pressed(BTN_HOME)){page=(page+1)%4;changed=1;}d=encoder(ENC_KNOB1);if(d){if(page==3){shimmer=clamp(shimmer+d,0,127);}else if(page==1){verb=clamp(verb+d,0,127);}else if(page==2){harmonic=wrap(harmonic+d,4);}else{level=clamp(level+d,0,127);}changed=1;}d=encoder(ENC_KNOB2);if(d){if(page==3){chorus=clamp(chorus+d,0,127);}else if(page==1){feedback=clamp(feedback+d,0,127);}else if(page==2){period=clamp(period+d,8,40);}else{tone=clamp(tone+d,0,127);}changed=1;}d=encoder(ENC_KNOB3);if(d){if(page==3){space=clamp(space+d,0,127);}else if(page==1){lowpass=clamp(lowpass+d,0,127);}else if(page==2){depth=clamp(depth+d,0,127);}else{motion=clamp(motion+d,0,127);}changed=1;}d=encoder(ENC_KNOB4);if(d){if(page==3){pan=clamp(pan+d,0,127);}else if(page==1){highpass=clamp(highpass+d,0,127);}else if(page==2){mode=wrap(mode+d,3);}else{fade=clamp(fade+d,1,10);}changed=1;}if(button_pressed(BTN_SAVE)&&!playing&&quiet){uint8_t saved[2]={37,(uint8_t)theme};settings_save(saved,sizeof saved);}if(changed){publish();screen();}led_button(BTN_PLAY,playing);led_button(BTN_OCT_DOWN,oct<0);led_button(BTN_OCT_UP,oct>0);for(int k=0;k<KEY_COUNT;k++)led_key(k,playing&&(KEY_FIRST_NOTE+k)%12==root);}
