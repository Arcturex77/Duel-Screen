
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <3ds.h>
#include <citro2d.h>

#define TOP_SCREEN_WIDTH 400
#define SCREEN_HEIGHT 240
#define BOTTOM_SCREEN_WIDTH 320



static C2D_SpriteSheet spriteSheet; //create sheet object
static Sprite sprites[768]; //make sprites array

void initSprite(int spriteIndex, float x, float y)
{
	Sprite* sprite = &sprites[spriteIndex];

	C2D_SpriteFromSheet(&sprite->spr, spriteSheet, spriteIndex);
	C2D_SpriteSetCenter(&sprite->spr, 0.5f, 0.5f);
	C2D_SpriteSetPos(&sprite->spr, x, y);
}

//initializing text and text buffer for C2D
static const char *staticStrings[] = {
	"+",
	"-"
};

C2D_TextBuf textBuffer, debugBuffer;
C2D_Text text[sizeof(staticStrings)/sizeof(staticStrings[0])];

//init text
void staticTextInit(void)
{
	//create text buffer for 2048 total glyphs
	textBuffer = C2D_TextBufNew(2048);
	
	//parse string to C2D stuff (and optimize)
	for(int i = 0; i < sizeof(staticStrings)/sizeof(staticStrings[0]); i++){
		C2D_TextParse(&text[i], textBuffer, staticStrings[i]);
		C2D_TextOptimize(&text[i]);
	}
}

//input reading
u32 down, held;
touchPosition touch;

void readInputs()
{
	//Scan all the inputs. This should be done once for each frame
	hidScanInput();
		//read touchscreen
	hidTouchRead(&touch);

	//keys and touchscreen shorthands
	down = hidKeysDown();
	held = hidKeysHeld();
}

//------------------------------------------------------------------------------------
//BUTTON STUFF
//------------------------------------------------------------------------------------
typedef struct
{
	int x;
	int y;
	int width;
	int height;
	
	u32 color1;
	u32 color2;

	bool pressed;

	int textIndex;
	void (*onPress)(void);
} Button;

Button makeButton(
	int x,
	int y,
	int w, 
	int h,
	u32 c1,
	u32 c2,
	int text,
	void (*onPress)(void))
{
	Button b;

	b.x = x;
	b.y = y;
	b.width = w;
	b.height = h;

	b.color1 = c1;
	b.color2 = c2;

	b.textIndex = text;
	b.onPress = onPress;

	b.pressed = false;

	return b;
}

//is stylus over button
bool buttonContains(Button* b, int x, int y)
{
	return (
		x >= b->x &&
		x <= b->x + b->width &&
		y >= b->y &&
		y <= b->y + b->height
	);
}

void buttonUpdate(Button* b)
{
	if (held & KEY_TOUCH)
	{
		if (buttonContains(b, touch.px, touch.py))
		{
			b->pressed = true;
		}
		else 
		{
			b->pressed = false;
		}
	}
	else
	{
		b->pressed = false;
	}

	if (down & KEY_TOUCH)
	{
		if (buttonContains(b, touch.px, touch.py))
		{
			b->onPress();
		}
	}
}

void buttonDraw(Button* b)
{
	u32 col = b->pressed ? b->color2 : b->color1;

	C2D_DrawRectSolid(
		b->x, b->y,
		0.0f,
		b->width, b->height,
		col
	);

	//TODO: FIX TEXT SO IT RENDERS VERTICALLY IN THE CENTER USING DEFAULT FONT SIZE
	C2D_DrawText(&text[b->textIndex], C2D_AlignCenter | C2D_AtBaseline, b->x + (b->width / 2), b->y + (b->height / 2), 0.0f, 0.5f, 0.5f);
}

//button pressed function(s)

void doNothing(void){}

int life = 40; //placeholder life total
void incrimentLife(void){
	life++;
}

void decrimentLife(void){
	life--;
}
//------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------
//MAIN
//------------------------------------------------------------------------------------
int main(int argc, char **argv)
{
	

	// Create screens
	C3D_RenderTarget* top = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
	C3D_RenderTarget* bottom = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);
	
	// color shorthands
	u32 clrWhite = C2D_Color32(255, 255, 255, 255);
	u32 clrGreen = C2D_Color32(0, 255, 0, 255);
	//u32 clrRed   = C2D_Color32(255, 0, 0, 255);
	u32 clrBlue  = C2D_Color32(0, 0, 255, 255);
	u32 clrBlack = C2D_Color32(0, 0, 0, 255);

	//init sprite stuff
	spriteSheet = C2D_SpriteSheetLoad("romfs:/gfx/sprites.t3x");
	if (!spriteSheet) svcBreak(USERBREAK_PANIC);

	initSprite(0, TOP_SCREEN_WIDTH/2, SCREEN_HEIGHT/2);
	initSprite(1, 140, 210);

	//init text
	staticTextInit();
	debugBuffer = C2D_TextBufNew(1028);

	//make the buttons exist
	Button buttons[]={
		//Incriment life
		makeButton(
			20, 80,
			20, 20,
			clrGreen,
			clrBlue,
			0,
			incrimentLife
		),
		//decriment life
		makeButton(
			50, 80,
			20, 20,
			clrGreen,
			clrBlue,
			1,
			decrimentLife
		)
	};
	

	// Main loop
	while (aptMainLoop())
	{
		readInputs();

		if (down & KEY_START) break; // break in order to return to hbmenu

		//update buttons
		for(int i = 0; i < sizeof(buttons)/sizeof(buttons[0]); i++){
			buttonUpdate(&buttons[i]);
		};


		
		//------------------------------------------------------------------------------------
		//rendering start
		//------------------------------------------------------------------------------------\
		C3D_DepthTest(false, GPU_ALWAYS, GPU_WRITE_ALL);
		C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
		
		//------------------------------------------------------------------------------------
		//top screen
		C2D_TargetClear(top, clrBlack);
		C2D_SceneBegin(top);


		C2D_DrawSprite(&sprites[0].spr);
		//color boxes
		C2D_DrawRectSolid(0.0f, 0.0f, 0.0f, 200.0f, 120.0f, C2D_Color32(255, 0, 0, 23));//red
		C2D_DrawRectSolid(200.0f, 0.0f, 0.0f, 200.0f, 120.0f, C2D_Color32(0, 114, 255, 23));//blue
		C2D_DrawRectSolid(0.0f, 120.0f, 0.0f, 200.0f, 120.0f, C2D_Color32(245, 228, 154, 23));//yellow
		C2D_DrawRectSolid(200.0f, 120.0f, 0.0f, 200.0f, 120.0f, C2D_Color32(52, 134, 0, 23));//green
		
		//------------------------------------------------------------------------------------
		//bottom screeen
		C2D_TargetClear(bottom, clrBlack);
		C2D_SceneBegin(bottom);

		//draw debug text
		C2D_TextBufClear(debugBuffer);

		char buf[160];
		C2D_Text debugText;
		snprintf(buf, sizeof(buf),
			"CPU:     %6.2f%%\nGPU:     %6.2f%%\nCmdBuf:  %6.2f%%\nLife: %d", 
			C3D_GetProcessingTime()*6.0f,
			C3D_GetDrawingTime()*6.0f,
			C3D_GetCmdBufUsage()*100.0f,
			life
		);
		C2D_TextParse(&debugText, debugBuffer, buf);
		C2D_TextOptimize(&debugText);
		C2D_DrawText(&debugText, C2D_WithColor, 3.0f, 3.0f, 0.0f, 0.5f, 0.5f, clrWhite);

		//draw buttons
		for(int i = 0; i < sizeof(buttons)/sizeof(buttons[0]); i++){
			buttonDraw(&buttons[i]);
		};

		C2D_DrawSprite(&sprites[1].spr);
		
		C3D_FrameEnd(0);
		
	}

	//de-init scene
	C2D_TextBufDelete(textBuffer);
	C2D_SpriteSheetFree(spriteSheet);

	//de-init libs
	C2D_Fini();
	C3D_Fini();
	gfxExit();
	romfsExit();
	return 0;
}

