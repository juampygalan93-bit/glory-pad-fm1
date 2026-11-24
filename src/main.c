
/* GLORY PAD FM-1
 * V0.2 - Worship Pad Engine
 */

#include "fm1.h"

#define OSC 6
#define PRESET_COUNT 10
#define FULL 16777216
#define ACCENT RGB(155,185,255)
#define DIM RGB(95,115,145)
#define PANEL RGB(15,23,36)

static const char *const NOTES[] = {
  "C","C#","D","D#","E","F",
  "F#","G","G#","A","A#","B"
};

static const char *const NAMES[] = {
  "ANALOG SINE",
  "CATHEDRAL",
  "CLEAN ORGAN",
  "DARK ANALOG",
  "DARK UNO",
  "DEFAULT",
  "RESONANT",
  "SOFT SQUARE",
  "STRINGS",
  "TRIANGLE CORE"
};

typedef struct {
  int weight[OSC];
  int offset[OSC];
  int shape;
  int detune;
} Patch;

static const Patch patches[PRESET_COUNT] = {
  {{9000,3500,4200,1800,900,500},
   {0,0,7,12,12,19},0,10},

  {{6500,3500,4200,3300,2400,1800},
   {0,0,7,12,19,24},1,12},

  {{6500,5000,4000,2800,2200,1200},
   {0,0,7,12,12,19},0,8},

  {{8500,4500,3500,1600,900,500},
   {0,0,7,12,12,19},2,18},

  {{10000,5000,2800,1200,600,300},
   {-12,0,7,12,12,19},0,5},

  {{7000,4000,4200,2500,1800,900},
   {0,0,7,12,12,19},3,16},

  {{6500,3500,4000,2800,2300,1200},
   {0,0,7,12,19,24},4,12},

  {{7500,4000,4000,2000,1200,600},
   {0,0,7,12,12,19},5,15},

  {{5200,4800,4200,3500,2400,1600},
   {0,0,7,12,12,19},6,25},

  {{8500,4000,3600,2200,1200,600},
   {0,0,7,12,12,19},7,10}
};

typedef struct {
  uint32_t phase[OSC];
  uint32_t inc[OSC];
  int32_t amp;
  int root;
  int preset;
  int octave;
} Bank;

static Bank bank[2];
static int current = 0;

static volatile uint32_t request = 2u << 4 | 2u << 8;

static volatile int level = 85;
static volatile int tone = 55;
static volatile int motion = 0;
static volatile int fade = 5;

static volatile int quiet = 1;

static int root = 0;
static int preset = 2;
static int octave = 0;
static int playing = 0;

static uint32_t lfo = 0;

static int clamp(int x,int lo,int hi) {
  if(x<lo) return lo;
  if(x>hi) return hi;
  return x;
}

static int wrap(int x,int n) {
  x %= n;
  return x<0 ? x+n : x;
}

static void publish(void) {
  request = (uint32_t)root |
    ((uint32_t)preset << 4) |
    ((uint32_t)(octave+2) << 8) |
    ((uint32_t)playing << 12);
}

static int32_t wave_sample(
  uint32_t p,int shape,int bright
) {
  int32_t a = sine(p);
  int32_t b = sine(p*2u);
  int32_t c = sine(p*3u);

  switch(shape) {
    case 1:
      return (a*8+b*2+c)/11;

    case 2:
      return (a*10+b+c)/12;

    case 3:
      return (a*8+b+c)/10;

    case 4:
      return (a*6+b*2+c*2+
              sine(p*5u))/11;

    case 5:
      return (a*9+c*2+
              sine(p*5u))/12;

    case 6:
      return (a*8+b*4+c*2+
              sine(p*4u)+
              sine(p*5u))/16;

    case 7:
      return (a*9-c)/10;

    default:
      return (a*512+b*bright)/
             (512+bright);
  }
}

static void tune(
  Bank *b,int r,int pr,int oct
) {
  const Patch *p = &patches[pr];

  b->root = r;
  b->preset = pr;
  b->octave = oct;

  for(int j=0;j<OSC;j++) {
    int n = 36+r+oct*12+p->offset[j];
    uint32_t inc = note_inc(clamp(n,0,127));

    if(j==1)
      inc += (inc/10000u)*p->detune;

    if(j==4)
      inc -= (inc/10000u)*p->detune;

    b->inc[j] = inc;
  }
}

static int32_t approach(
  int32_t value,int32_t target,int32_t step
) {
  if(value<target) {
    value += step;
    if(value>target) value=target;
  } else if(value>target) {
    value -= step;
    if(value<target) value=target;
  }
  return value;
}

static int32_t limit(int32_t s) {
  if(s>30000) return 30000;
  if(s<-30000) return -30000;
  return s;
}

void audio(int32_t *out,uint32_t frames) {

  uint32_t req = request;

  int r = req & 15;
  int pr = (req>>4)&15;
  int oct = ((req>>8)&7)-2;
  int active = (req>>12)&1;

  int32_t step =
    FULL/(SAMPLE_RATE*fade);

  if(step<1) step=1;

  int bright=tone;
  int depth=motion;
  int vol=level;

  if(active &&
    (bank[current].root!=r ||
     bank[current].preset!=pr ||
     bank[current].octave!=oct)) {

    int next=1-current;

    if(bank[next].root==r &&
       bank[next].preset==pr &&
       bank[next].octave==oct) {
      current=next;
    } else if(bank[next].amp==0) {
      tune(&bank[next],r,pr,oct);
      current=next;
    }
  }

  for(uint32_t i=0;i<frames;i++) {

    int32_t left=0,right=0;

    lfo+=6000u;

    int32_t move =
      (sine(lfo)*depth)>>15;

    for(int b=0;b<2;b++) {

      Bank *v=&bank[b];

      int32_t goal =
        active && b==current ? FULL:0;

      v->amp=approach(v->amp,goal,step);

      if(v->amp==0) continue;

      const Patch *p=&patches[v->preset];

      int32_t l=0,rr=0;

      for(int j=0;j<OSC;j++) {

        v->phase[j]+=v->inc[j];

        int32_t s=wave_sample(
          v->phase[j],p->shape,bright
        );

        int32_t w=p->weight[j];

        if(j>=3) {
          w+=(w*move)>>10;
        }

        s=(s*w)>>15;

        if(j==1 || j==4) {
          l+=s/2;
          rr+=s;
        } else if(j==2 || j==5) {
          l+=s;
          rr+=s/2;
        } else {
          l+=s;
          rr+=s;
        }
      }

      int32_t gain=v->amp>>9;

      left+=(l*gain)>>15;
      right+=(rr*gain)>>15;
    }

    left=(left*vol)>>7;
    right=(right*vol)>>7;

    out[2*i]+=limit(left);
    out[2*i+1]+=limit(right);
  }

  quiet = bank[0].amp==0 &&
          bank[1].amp==0;
}

static void centered(
  int y,const char *s,
  uint16_t color,int font
) {
  rect(0,y,240,text_height(font),BLACK);

  text(
    (240-text_width(s,font))/2,
    y,s,color,font
  );
}

static void draw_header(void) {

  centered(8,"GLORY PAD 0.2",
           ACCENT,FONT_SMALL);

  centered(34,NAMES[preset],
           WHITE,FONT_SMALL);
}

static void draw_note(void) {

  rect(0,60,240,100,BLACK);

  frame(78,62,84,72,DIM);
  frame(81,65,78,66,ACCENT);

  text(
    (240-text_width(NOTES[root],
      FONT_BIG))/2,
    82,NOTES[root],WHITE,FONT_BIG
  );

  centered(
    143,
    playing?"PAD ACTIVE":"STANDBY",
    playing?ACCENT:DIM,
    FONT_SMALL
  );
}

static void draw_knobs(void) {

  const char *labels[]={
    "LEVEL","TONE","MOTION","FADE"
  };

  int values[]={level,tone,motion,fade};

  for(int k=0;k<4;k++) {

    int x=k*60;
    char val[12];

    format_int(val,values[k]);

    rect(x,177,60,60,BLACK);

    text(
      x+(60-text_width(labels[k],
        FONT_SMALL))/2,
      177,labels[k],DIM,FONT_SMALL
    );

    text(
      x+(60-text_width(val,
        FONT_BIG))/2,
      198,val,ACCENT,FONT_BIG
    );

    int width=k==3 ?
      values[k]*50/10 :
      values[k]*50/127;

    rect(x+5,232,50,3,PANEL);
    rect(x+5,232,width,3,ACCENT);
  }
}

void setup(void) {

  tune(&bank[0],root,preset,octave);
  tune(&bank[1],root,preset,octave);

  publish();

  clear(BLACK);
  draw_header();
  draw_note();
  draw_knobs();
}

void loop(void) {

  int update=0;
  int redraw=0;

  for(int k=0;k<KEY_COUNT;k++) {

    if(key_pressed(k)) {
      root=(KEY_FIRST_NOTE+k)%12;
      playing=1;
      update=1;
    }
  }

  uint8_t st,d1,d2;

  while(midi_read(&st,&d1,&d2)) {

    if((st&0xF0)==0x90 && d2) {
      root=d1%12;
      playing=1;
      update=1;
    }
  }

  int d=encoder(ENC_SELECT);

  if(d) {
    root=wrap(root+d,12);
    update=1;
  }

  d=encoder(ENC_PRESETS);

  if(d) {
    preset=wrap(preset+d,PRESET_COUNT);
    update=1;
  }

  int o=octave+
    button_pressed(BTN_OCT_UP)-
    button_pressed(BTN_OCT_DOWN);

  o=clamp(o,-2,2);

  if(o!=octave) {
    octave=o;
    update=1;
  }

  if(button_pressed(BTN_PLAY)) {
    playing=!playing;
    update=1;
  }

  d=encoder(ENC_KNOB1);
  if(d) {
    level=clamp(level+d,0,127);
    redraw=1;
  }

  d=encoder(ENC_KNOB2);
  if(d) {
    tone=clamp(tone+d,0,127);
    redraw=1;
  }

  d=encoder(ENC_KNOB3);
  if(d) {
    motion=clamp(motion+d,0,127);
    redraw=1;
  }

  d=encoder(ENC_KNOB4);
  if(d) {
    fade=clamp(fade+d,1,10);
    redraw=1;
  }

  if(button_pressed(BTN_HOME)) {
    update=1;
    redraw=1;
  }

  if(update) {
    publish();
    draw_header();
    draw_note();
  }

  if(redraw) draw_knobs();

  led_button(BTN_PLAY,playing);
  led_button(BTN_OCT_DOWN,octave<0);
  led_button(BTN_OCT_UP,octave>0);

  for(int k=0;k<KEY_COUNT;k++) {
    led_key(k,playing &&
      (KEY_FIRST_NOTE+k)%12==root);
  }
}
