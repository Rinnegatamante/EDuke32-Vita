#include "compat.h"
#include "renderlayer.h"
#include "cache1d.h"
#include "a.h"
#include "build.h"
#include "osd.h"
#include "scancodes.h"
#include "engine_priv.h"

#include <vitasdk.h>
#include <vita2d.h>

int32_t xres=960, yres=544, bpp=8, fullscreen=1, bytesperline = 960;

char quitevent=0, appactive=1, novideo=0;

intptr_t frameplace;

uint8_t *framebuffer;
uint16_t *fb;
uint16_t ctrlayer_pal[256];

char modechange=1;
char videomodereset = 0;

char offscreenrendering=0;

int32_t inputchecked = 0;

int32_t lockcount=0;


// Joystick dead and saturation zones
uint16_t *joydead, *joysatur;

vita2d_texture *fb_texture;

int _newlib_heap_size_user = 192 * 1024 * 1024;

int main(int argc, char **argv){

	scePowerSetArmClockFrequency(444);
	scePowerSetBusClockFrequency(222);
	scePowerSetGpuClockFrequency(222);
	scePowerSetGpuXbarClockFrequency(166);
	sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG_WIDE);
	vita2d_init();
	
	fb_texture = vita2d_create_empty_texture_format(960, 544, SCE_GXM_TEXTURE_FORMAT_P8_1BGR);
	//vita2d_texture_set_alloc_memblock_type(SCE_KERNEL_MEMBLOCK_TYPE_USER_RW);
	
	framebuffer = vita2d_texture_get_datap(fb_texture);

	baselayer_init();

	int r = app_main(argc, (const char **)argv);

	return r;
}

//
// initsystem() -- init 3DS systems
//
int32_t initsystem(void)
{
    frameplace = 0;
    return 0;
}


//
// uninitsystem() -- uninit 3DS systems
//
void uninitsystem(void)
{
    uninitinput();
    uninittimer();
    vita2d_fini();
}

//
// initprintf() -- prints a formatted string to the intitialization window
//

void initprintf(const char *f, ...)
{
    va_list va;
    char buf[2048];

    va_start(va, f);
    Bvsnprintf(buf, sizeof(buf), f, va);
    va_end(va);

    initputs(buf);
}

//
// initputs() -- prints a string to the intitialization window
//
void initputs(const char *buf)
{
    static char dabuf[2048];

    OSD_Puts(buf);
    handleevents();

}

//
// debugprintf() -- prints a formatted debug string to stderr
//
void debugprintf(const char *f, ...)
{
	va_list va;
    char buf[2048];

    va_start(va, f);
    Bvsnprintf(buf, sizeof(buf), f, va);
    va_end(va);

    initputs(buf);
}

int32_t wm_msgbox(const char *name, const char *fmt, ...)
{
    char buf[2048];
    va_list va;

    UNREFERENCED_PARAMETER(name);

    va_start(va,fmt);
    vsnprintf(buf,sizeof(buf),fmt,va);
    va_end(va);

    printf(buf);
    return 0;
}

int32_t wm_ynbox(const char *name, const char *fmt, ...)
{
    char buf[2048];
    va_list va;

    UNREFERENCED_PARAMETER(name);

    va_start(va,fmt);
    vsprintf(buf,fmt,va);
    va_end(va);

    printf(buf);
    return 0;
}

void wm_setapptitle(const char *name)
{
    UNREFERENCED_PARAMETER(name);
}

//
// initinput() -- init input system
//
int32_t initinput(void)
{
    int32_t i, j;


    if (!keyremapinit)
        for (i = 0; i < 256; i++) keyremap[i] = i;
    keyremapinit = 1;

    inputdevices = 1 | 2 | 4;  // keyboard (1) mouse (2) joystick (4)
    mousegrab = 0;

    memset(key_names, 0, sizeof(key_names));

    joynumbuttons = 13;
    joynumhats = 1;
    joynumaxes = 4;
    joyhat = (int32_t *)Bcalloc(1, sizeof(int32_t));

    joyaxis = (int32_t *)Bcalloc(joynumaxes, sizeof(int32_t));
    joydead = (uint16_t *)Bcalloc(joynumaxes, sizeof(uint16_t));
    joysatur = (uint16_t *)Bcalloc(joynumaxes, sizeof(uint16_t));

    joyhat[0] = -1;

    return 0;
}

//
// uninitinput() -- uninit input system
//
void uninitinput(void)
{
    uninitmouse();
}

static const char *joynames[13] =
{
    "Cross", "Circle", "Square", "Triangle", "L", "R", "Unused", "Unused", "T1", "T2", "T3", "T4", "T5"
};

const char *getjoyname(int32_t what, int32_t num)
{
    static char tmp[64];

    switch (what)
    {
        case 0:  // axis
            if ((unsigned)num > (unsigned)joynumaxes)
                return NULL;
            Bsprintf(tmp, "Axis %d", num);
            return (char *)tmp;

        case 1:  // button
            if ((unsigned)num > (unsigned)joynumbuttons)
                return NULL;
            return joynames[num];

        case 2:  // hat
            if ((unsigned)num > (unsigned)joynumhats)
                return NULL;
            Bsprintf(tmp, "Hat %d", num);
            return (char *)tmp;

        default: return NULL;
    }
}

//
// setjoydeadzone() -- sets the dead and saturation zones for the joystick
//
void setjoydeadzone(int32_t axis, uint16_t dead, uint16_t satur)
{
    joydead[axis] = dead;
    joysatur[axis] = satur;
}


//
// getjoydeadzone() -- gets the dead and saturation zones for the joystick
//
void getjoydeadzone(int32_t axis, uint16_t *dead, uint16_t *satur)
{
    *dead = joydead[axis];
    *satur = joysatur[axis];
}

//
// initmouse() -- init mouse input
//
int32_t initmouse(void)
{
    moustat=AppMouseGrab;
    grabmouse(AppMouseGrab); // FIXME - SA
    return 0;
}

//
// uninitmouse() -- uninit mouse input
//
void uninitmouse(void)
{
    grabmouse(0);
    moustat = 0;
}

//
// grabmouse() -- show/hide mouse cursor
//
void grabmouse(char a)
{
    if (appactive && moustat)
    {
            mousegrab = a;
    }
    else
        mousegrab = a;

    mousex = mousey = 0;
}

void AppGrabMouse(char a)
{
    if (!(a & 2))
    {
        grabmouse(a);
        AppMouseGrab = mousegrab;
    }

}

void setkey(uint32_t keycode, int state){
    if(state)
        joyb |=  1 << keycode;
    else
        joyb &=  ~(1 << keycode);
}
static int32_t hatpos =0;

static int32_t hatvals[16] = {
    -1,     // centre
    0,      // up 1
    9000,   // right 2
    4500,   // up+right 3
    18000,  // down 4
    -1,     // down+up!! 5
    13500,  // down+right 6
    -1,     // down+right+up!! 7
    27000,  // left 8
    27500,  // left+up 9
    -1,     // left+right!! 10
    -1,     // left+right+up!! 11
    22500,  // left+down 12
    -1,     // left+down+up!! 13
    -1,     // left+down+right!! 14
    -1,     // left+down+right+up!! 15
};

void handleevents_buttons(int keys, int state){

    if( keys & SCE_CTRL_SELECT){
        if (OSD_HandleScanCode(sc_Escape, state))
        {
            SetKey(sc_Escape, state);

            if (keypresscallback)
                keypresscallback(sc_Escape, state);
        }
    }
    if( keys & SCE_CTRL_START){
        if (OSD_HandleScanCode(sc_Return, state))
        {
            SetKey(sc_Return, state);

            if (keypresscallback)
                keypresscallback(sc_Return, state);
        }
    }

    //Buttons

    if( keys & SCE_CTRL_CROSS)
        setkey(0, state);
    if( keys & SCE_CTRL_CIRCLE)
        setkey(1, state);
    if( keys & SCE_CTRL_SQUARE)
        setkey(2, state);
    if( keys & SCE_CTRL_TRIANGLE)
        setkey(3, state);
    if( keys & SCE_CTRL_LTRIGGER)
        setkey(4, state);
    if( keys & SCE_CTRL_RTRIGGER)
        setkey(5, state);
    /*if( keys & KEY_ZL)
        setkey(6, state);
    if( keys & KEY_ZR)
        setkey(7, state);*/

    //Hat (Dpad)
	

	if (state){
		hatpos = 0;
		if( keys & SCE_CTRL_LEFT)
			hatpos += 8;
		if( keys & SCE_CTRL_RIGHT)
			hatpos += 2;
		if( keys & SCE_CTRL_UP)
			hatpos += 1;
		if( keys & SCE_CTRL_DOWN)
			hatpos += 4;
		if (joyhat) joyhat[0] = hatvals[hatpos];
	} 
	
}

void IN_RescaleAnalog(int *x, int *y, int dead) {
	//radial and scaled deadzone
	//http://www.third-helix.com/2013/04/12/doing-thumbstick-dead-zones-right.html

	float analogX = (float) *x;
	float analogY = (float) *y;
	float deadZone = (float) dead;
	float maximum = 128.0f;
	float magnitude = sqrt(analogX * analogX + analogY * analogY);
	if (magnitude >= deadZone)
	{
		float scalingFactor = maximum / magnitude * (magnitude - deadZone) / (maximum - deadZone);
		*x = (int) (analogX * scalingFactor);
		*y = (int) (analogY * scalingFactor);
	} else {
		*x = 0;
		*y = 0;
	}
}

void handleevents_axes(void){
    if(!joyaxis)
        return;
	
	SceCtrlData padt;
	sceCtrlPeekBufferPositive(0, &padt, 1);

	int lx, ly, rx, ry;
	lx = (int)padt.lx - 127;
	ly = (int)padt.ly - 127;
	rx = (int)padt.rx - 127;
	ry = (int)padt.ry - 127;
	
	IN_RescaleAnalog(&lx, &ly, 30);
	IN_RescaleAnalog(&rx, &ry, 30);
    joyaxis[0] =  lx * 5;
    joyaxis[1] =  ly * 20;
    joyaxis[2] =  rx * 5;
    joyaxis[3] =  ry * 10;

    for(int i = 0; i < 4; i++){
        if((-joydead[i]/100 < joyaxis[i]) && (joydead[i]/100 > joyaxis[i]))
            joyaxis[i] = 0;
    }

}

#define MAX_PSVITA_KEYS 12
uint32_t KeyTable[MAX_PSVITA_KEYS] =
{
	SCE_CTRL_SELECT,
	SCE_CTRL_START,
	SCE_CTRL_UP,
	SCE_CTRL_DOWN,
	SCE_CTRL_LEFT,
	SCE_CTRL_RIGHT,
	SCE_CTRL_LTRIGGER,
	SCE_CTRL_RTRIGGER,
	SCE_CTRL_SQUARE,
	SCE_CTRL_TRIANGLE,
	SCE_CTRL_CROSS,
	SCE_CTRL_CIRCLE
};

void PSP2_KeyDown(uint32_t keys, uint32_t oldkeys) {
	int i;
	uint32_t bitmask = 0;
	for (i = 0; i < MAX_PSVITA_KEYS; i++) {
		if (keys & KeyTable[i] && (!(oldkeys & KeyTable[i]))) {
			bitmask += KeyTable[i];
		}
	}
	if (bitmask > 0) handleevents_buttons(bitmask, 1);
}

void PSP2_KeyUp(uint32_t keys, uint32_t oldkeys) {
	int i;
	uint32_t bitmask = 0;
	for (i = 0; i < MAX_PSVITA_KEYS; i++) {
		if ((!(keys & KeyTable[i])) && (oldkeys & KeyTable[i])) {
			bitmask += KeyTable[i];
		}
	}
	if (bitmask > 0) handleevents_buttons(bitmask, 0);
}

uint32_t oldpad = 0;

int32_t handleevents(void)
{
	SceCtrlData pad;
	sceCtrlPeekBufferPositive(0, &pad, 1);
    uint32_t kDown = pad.buttons;
    uint32_t kUp = oldpad;
	
	if (joyhat) joyhat[0] = -1;
	
	if (kUp != kDown){
		PSP2_KeyDown(kDown, kUp);
		PSP2_KeyUp(kDown, kUp);
	}

    handleevents_axes();

    sampletimer();
	oldpad = pad.buttons;
    return 0;
}

int32_t handleevents_peekkeys(void)
{
    return 0;
}

//
// releaseallbuttons()
//
void releaseallbuttons(void)
{
    joyb = 0;
    hatpos = 0;
}

static uint32_t timerfreq;
static uint32_t timerlastsample;
int32_t timerticspersec=0;
static double msperu64tick = 0;
static void(*usertimercallback)(void) = NULL;


//
// inittimer() -- initialize timer
//
int32_t inittimer(int32_t tickspersecond)
{
    if (timerfreq) return 0;	// already installed

    initprintf("Initializing timer\n");

    totalclock = 0;
    timerfreq = 1000000.0;
    timerticspersec = tickspersecond;
	SceRtcTick ticks;
	sceRtcGetCurrentTick(&ticks);
    timerlastsample = ticks.tick * timerticspersec / timerfreq;

    usertimercallback = NULL;

    msperu64tick = 0.000001;

    return 0;
}

//
// uninittimer() -- shut down timer
//
void uninittimer(void)
{
    if (!timerfreq) return;

    timerfreq=0;

    msperu64tick = 0;
}

//
// sampletimer() -- update totalclock
//
void sampletimer(void)
{
    uint64_t i;
    int32_t n;

    if (!timerfreq) return;
	SceRtcTick ticks;
	sceRtcGetCurrentTick(&ticks);
    n = (int32_t)((ticks.tick*timerticspersec / timerfreq) - timerlastsample);
    //printf("tick\n");
    if (n <= 0) return;

    totalclock += n;
    timerlastsample += n;

    if (usertimercallback)
        for (; n > 0; n--) usertimercallback();
}

//
// getticks() -- returns the ticks count
//
uint32_t getticks(void)
{
	SceRtcTick ticks;
	sceRtcGetCurrentTick(&ticks);
    return (uint32_t)ticks.tick;
}

//
// gettimerfreq() -- returns the number of ticks per second the timer is configured to generate
//
int32_t gettimerfreq(void)
{
    return 1000000.0;
}

static inline double u64_to_double(uint64_t value) {
    return (((double)(uint32_t)(value >> 32))*0x100000000ULL+(uint32_t)value);
}

// Returns the time since an unspecified starting time in milliseconds.
ATTRIBUTE((flatten))
double gethiticks(void)
{
	SceRtcTick ticks;
	sceRtcGetCurrentTick(&ticks);
    return (double)u64_to_double(ticks.tick * msperu64tick);
}

//
// installusertimercallback() -- set up a callback function to be called when the timer is fired
//
void(*installusertimercallback(void(*callback)(void)))(void)
{
    void(*oldtimercallback)(void);

    oldtimercallback = usertimercallback;
    usertimercallback = callback;

    return oldtimercallback;
}


//
// system_getcvars() -- propagate any cvars that are read post-initialization
//
void system_getcvars(void)
{

}

//
// resetvideomode() -- resets the video system
//
void resetvideomode(void)
{

}

// begindrawing()

void begindrawing(void)
{

    if (offscreenrendering) return;

    frameplace = (intptr_t)framebuffer;
    //printf("Begin Drawing");
    if(modechange){
        calc_ylookup(960, 544);
        modechange=0;
    }
}

// enddrawing()

void enddrawing(void){
}
//
// showframe() -- update the display
//
void showframe(int32_t w)
{
    UNREFERENCED_PARAMETER(w);
	if (offscreenrendering) return;
	int x,y;

	vita2d_start_drawing();
	vita2d_draw_texture(fb_texture, 0, 0);
	vita2d_end_drawing();
	vita2d_wait_rendering_done();
	vita2d_swap_buffers();

}

#define ADDMODE(x,y,c,f,n) if (validmodecnt<MAXVALIDMODES) { \
    validmode[validmodecnt].xdim=x; \
    validmode[validmodecnt].ydim=y; \
    validmode[validmodecnt].bpp=c; \
    validmode[validmodecnt].fs=f; \
    validmode[validmodecnt].extra=n; \
    validmodecnt++; \
}

static char modeschecked=0;

void getvalidmodes(void)
{

    if (modeschecked) return;
    validmodecnt=0;
    ADDMODE(960,544,8,1,-1)
    modeschecked=1;
}

//
// checkvideomode() -- makes sure the video mode passed is legal
//
int32_t checkvideomode(int32_t *x, int32_t *y, int32_t c, int32_t fs, int32_t forced)
{
    int32_t i, nearest=-1, dx, dy, odx=9999, ody=9999;
    getvalidmodes();
    if ( c > 8 ) return -1;
    // fix up the passed resolution values to be multiples of 8
    // and at least 320x200 or at most MAXXDIMxMAXYDIM
    *x = clamp(*x, 960, MAXXDIM);
    *y = clamp(*y, 544, MAXYDIM);

    for (i = 0; i < validmodecnt; i++)
    {
        if (validmode[i].bpp != c || validmode[i].fs != fs)
            continue;

        dx = klabs(validmode[i].xdim - *x);
        dy = klabs(validmode[i].ydim - *y);

        if (!(dx | dy))
        {
            // perfect match
            nearest = i;
            break;
        }

        if ((dx <= odx) && (dy <= ody))
        {
            nearest = i;
            odx = dx;
            ody = dy;
        }
    }

    if (nearest < 0)
        return -1;

    *x = validmode[nearest].xdim;
    *y = validmode[nearest].ydim;
    return nearest;
}

int32_t setvideomode(int32_t x, int32_t y, int32_t c, int32_t fs)
{
	xres = x;
    yres = y;
    bpp =  c;
    fullscreen = fs;
    bytesperline = 960;
    numpages =  1;
    frameplace = 0;
    modechange = 1;
    videomodereset = 0;

    OSD_ResizeDisplay(x, y);

    return 0;
}

//
// setgamma
//
int32_t setgamma(void)
{
    return 0;
}

//
// setpalette() -- set palette values
//
int32_t setpalette(int32_t start, int32_t num)
{
    int32_t i, n;

    if (bpp > 8)
        return 0;

	
	uint8_t *pal = curpalettefaded;
	uint8_t r, g, b;
	uint32_t* palette_tbl = vita2d_texture_get_palette(fb_texture);
	for(i=0; i<256; i++){
		r = pal[0];
		g = pal[1];
		b = pal[2];
		palette_tbl[i] = r | (g << 8) | (b << 16) | (0xFF << 24);
		pal += 4;
	}

    return 0;
}
