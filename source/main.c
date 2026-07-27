
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <3ds.h>
#include <citro2d.h>

#define TOP_SCREEN_WIDTH 400
#define SCREEN_HEIGHT 240
#define BOTTOM_SCREEN_WIDTH 320

//player data struct
typedef struct
{
	int life, commanderDamage[4];
} PlayerData;

//populate player data starting values
PlayerData players[4];
void initPlayers(void *data)
{
	(void)data;
	for (int i = 0; i < (sizeof(players)/sizeof(players[0])); i++){

		players[i].life = 40;
		for (int j = 0; j < 4; j++){
			players[i].commanderDamage[j] = 0;
		}
	}
}

//sprite struct
typedef struct
{
	C2D_Sprite spr; //create C2D sprite thingy
	float x, y; //location
	int spriteIndex; //sprite sheet index
} Sprite;

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
	"-",
	"C. Damage",
	"x",
	"SETTINGS",
	"RESET"
};

C2D_TextBuf textBuffer, dynamicBuffer;
C2D_Text text[sizeof(staticStrings)/sizeof(staticStrings[0])];
C2D_Font font;

//init text
static void staticTextInit(void)
{
	font = C2D_FontLoad("romfs:/dimurphic.bcfnt");
	
	//create text buffer for 2048 total glyphs
	textBuffer = C2D_TextBufNew(2048);
	
	//parse string to C2D stuff (and optimize)
	for(int i = 0; i < sizeof(staticStrings)/sizeof(staticStrings[0]); i++){
		C2D_TextFontParse(&text[i], font, textBuffer, staticStrings[i]);
		C2D_TextOptimize(&text[i]);
	}
}

//drawing shortcuts
void drawBox(int x, int y, int w, int h, int thickness, u32 col)
{
	//top arm
	C2D_DrawRectSolid(x, y, 0.0f, w, thickness, col);
	//left arm
	C2D_DrawRectSolid(x, y, 0.0f, thickness, h, col);
	//right arm
	C2D_DrawRectSolid(x + w - thickness, y, 0.0f, thickness, h, col);
	//bottom arm
	C2D_DrawRectSolid(x, y + h - thickness, 0.0f, w, thickness, col);
}

//input reading
u32 down, held, up;
touchPosition touch;

void readInputs(void)
{
	//Scan all the inputs. This should be done once for each frame
	hidScanInput();
		//read touchscreen
	hidTouchRead(&touch);

	//keys and touchscreen shorthands
	down = hidKeysDown();
	held = hidKeysHeld();
	up = hidKeysUp();
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

	int textIndex;
	float textScale;

	void (*onPress)(void *);
	void *data;

	bool pressed;
} Button;

Button *activeButton = NULL;

Button makeButton(
	int x,
	int y,
	int w, 
	int h,
	u32 c1,
	u32 c2,
	int text,
	float scale,
	void (*onPress)(void *),
	void *data)
{
	Button b;

	b.x = x;
	b.y = y;
	b.width = w;
	b.height = h;

	b.color1 = c1;
	b.color2 = c2;

	b.textIndex = text;
	b.textScale = scale;

	b.onPress = onPress;
	b.data = data;

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

void buttonUpdate(Button buttons[], int count)
{

	// Touch just started
    if (down & KEY_TOUCH)
    {
        activeButton = NULL;

        for (int i = 0; i < count; i++)
        {
            Button *b = &buttons[i];

            if (buttonContains(b, touch.px, touch.py))
            {
                activeButton = b;
                b->pressed = true;
            }
        }
    }

    // Touch is being held
    if (held & KEY_TOUCH)
    {
        if (activeButton)
        {
			if (!buttonContains(activeButton, touch.px, touch.py))
			{
				activeButton->pressed = false;
			}
        }
    }

    // Touch released
    if (up & KEY_TOUCH)
    {
        if (activeButton && activeButton->pressed)
        {
            activeButton->onPress(activeButton->data);
            activeButton->pressed = false;
        }
		if (activeButton)
		{
			activeButton->pressed = false;
		}
		activeButton = NULL;
    }
}

void buttonDraw(Button buttons[], int count)
{
	for (int i = 0; i < count; i++)
	{
		Button* b = &buttons[i];

		u32 col = b->pressed ? b->color2 : b->color1;

		C2D_DrawRectSolid(
			b->x, b->y,
			0.0f,
			b->width, b->height,
			col
		);

		float textWidth, textHeight;
		C2D_TextGetDimensions(&text[b->textIndex], b->textScale, b->textScale, &textWidth, &textHeight);
		float textYOffset = (textHeight) / 6;

		C2D_DrawText(&text[b->textIndex], C2D_AlignCenter | C2D_AtBaseline, b->x + (b->width / 2), b->y + ((b->height / 2)+textYOffset), 0.0f, b->textScale, b->textScale);
	}
}

//button pressed function(s)

int buttonState = 0;
int cDamageIndex;
int ind[4] = {0, 1, 2, 3};

typedef struct {
    int commander;
    bool increment;
} cButtonStruct;

cButtonStruct cButtons[] = {
	{0, true}, //1+
	{0, false}, //1-
	{1, true}, //2+
	{1, false},//2-
	{2, true},//3+
	{2, false},//3-
	{3, true},//4+
	{3, false}//4-
};

int coordsIndex[4][2] = {//for C damage box indicator
	{0, 0},
	{200, 0},
	{0, 120},
	{200, 120}
};

void incrimentInt(void *data){
	int *v = data;
	(*v)++;
}

void decrimentInt(void *data){
	int *v = data;
	(*v)--;
}

void zero(void *data){
	int *v = data;
	(*v) = 0;
}

void settingsToggle(void *data){
	int *v = data;
	(*v) = 2;
}

void cDamageToggle(void *data){
	buttonState = 1;
	int *v = data;
	cDamageIndex = (*v);
}

void commanderButton(void *data)
{
    cButtonStruct *b = data;

    if (b->increment){
        players[cDamageIndex].commanderDamage[b->commander]++;
	} else {
        players[cDamageIndex].commanderDamage[b->commander]--;
	}
}

//------------------------------------------------------------------------------------
//MAIN
//------------------------------------------------------------------------------------

int main(int argc, char **argv)
{
	// Initialize the libs
	romfsInit();
	gfxInitDefault();
	C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
	C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
	C2D_Prepare();

	// Create screens
	C3D_RenderTarget* top = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
	C3D_RenderTarget* bottom = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);
	
	// color shorthands
	u32 clrWhite = C2D_Color32(255, 255, 255, 255);
	u32 clrBlack = C2D_Color32(0, 0, 0, 255);
	u32 clrOldWhite = C2D_Color32(231, 196, 119,255);
	u32 clrOldRed = C2D_Color32(216, 97, 45, 255);
	u32 clrOldGreen = C2D_Color32(131, 151, 0, 255);
	u32 clrOldBlue = C2D_Color32(53, 170, 181, 255);
	u32 clrBrown = C2D_Color32(96, 86, 50, 255);
	u32 clrTransRed = C2D_Color32(216, 97, 45, 100);

	//init sprite stuff
	spriteSheet = C2D_SpriteSheetLoad("romfs:/gfx/sprites.t3x");
	if (!spriteSheet) svcBreak(USERBREAK_PANIC);

	initSprite(0, TOP_SCREEN_WIDTH/2, SCREEN_HEIGHT/2);
	initSprite(1, 50, 50);

	//init text
	staticTextInit();
	dynamicBuffer = C2D_TextBufNew(2048);

	initPlayers(NULL);

	//make the buttons exist

	Button buttonsDefault[]={// STATE 0
		makeButton(//player 1 +
			6, 6,
			73, 50,
			clrOldRed,
			clrBrown,
			0,
			2.0f,
			incrimentInt,
			&players[0].life
		),
		makeButton(//player 1 -
			84, 6,
			73, 50,
			clrOldRed,
			clrBrown,
			1,
			2.0f,
			decrimentInt,
			&players[0].life
		),
		makeButton(//player 2 +
			163, 6,
			73, 50,
			clrOldBlue,
			clrBrown,
			0,
			2.0f,
			incrimentInt,
			&players[1].life
		),
		makeButton(//player 2 -
			241, 6,
			73, 50,
			clrOldBlue,
			clrBrown,
			1,
			2.0f,
			decrimentInt,
			&players[1].life
		),
		makeButton(//player 3 +
			6, 95,
			73, 50,
			clrOldWhite,
			clrBrown,
			0,
			2.0f,
			incrimentInt,
			&players[2].life
		),
		makeButton(//player 3 -
			84, 95,
			73, 50,
			clrOldWhite,
			clrBrown,
			1,
			2.0f,
			decrimentInt,
			&players[2].life
		),
		makeButton(//player 4 +
			163, 95,
			73, 50,
			clrOldGreen,
			clrBrown,
			0,
			2.0f,
			incrimentInt,
			&players[3].life
		),
		makeButton(//player 4 -
			241, 95,
			73, 50,
			clrOldGreen,
			clrBrown,
			1,
			2.0f,
			decrimentInt,
			&players[3].life
		),
		makeButton(//settings
			6, SCREEN_HEIGHT - 56,
			BOTTOM_SCREEN_WIDTH - 12, 50,
			clrBrown, clrBlack,
			4, 0.8f,
			settingsToggle, &buttonState
		),
		makeButton(//p1 commander damage
			6, 61,
			151, 28,
			clrOldRed,
			clrBrown,
			2, 0.8f,
			cDamageToggle,
			&ind[0]
		),
		makeButton(//p2 commander damage
			163, 61,
			151, 28,
			clrOldBlue,
			clrBrown,
			2, 0.8f,
			cDamageToggle,
			&ind[1]
		),
		makeButton(//p3 commander damage
			6, 150,
			151, 28,
			clrOldWhite,
			clrBrown,
			2, 0.8f,
			cDamageToggle,
			&ind[2]
		),
		makeButton(//p4 commander damage
			163, 150,
			151, 28,
			clrOldGreen,
			clrBrown,
			2, 0.8f,
			cDamageToggle,
			&ind[3]
		)
	};

	Button buttonsCDamage[]={//STATE 1
		makeButton(//X button
			284, 6,
			30, 30,
			clrTransRed,
			clrOldRed,
			3,
			1.2f,
			zero,
			&buttonState
		),
		makeButton(//player 1 +
			6, 26,
			73, 50,
			clrOldRed,
			clrBrown,
			0,
			2.0f,
			commanderButton,
			&cButtons[0]
		),
		makeButton(//player 1 -
			84, 26,
			73, 50,
			clrOldRed,
			clrBrown,
			1,
			2.0f,
			commanderButton,
			&cButtons[1]
		),
		makeButton(//player 2 +
			163, 26,
			73, 50,
			clrOldBlue,
			clrBrown,
			0,
			2.0f,
			commanderButton,
			&cButtons[2]
		),
		makeButton(//player 2 -
			241, 26,
			73, 50,
			clrOldBlue,
			clrBrown,
			1,
			2.0f,
			commanderButton,
			&cButtons[3]
		),
		makeButton(//player 3 +
			6, 115,
			73, 50,
			clrOldWhite,
			clrBrown,
			0,
			2.0f,
			commanderButton,
			&cButtons[4]
		),
		makeButton(//player 3 -
			84, 115,
			73, 50,
			clrOldWhite,
			clrBrown,
			1,
			2.0f,
			commanderButton,
			&cButtons[5]
		),
		makeButton(//player 4 +
			163, 115,
			73, 50,
			clrOldGreen,
			clrBrown,
			0,
			2.0f,
			commanderButton,
			&cButtons[6]
		),
		makeButton(//player 4 -
			241, 115,
			73, 50,
			clrOldGreen,
			clrBrown,
			1,
			2.0f,
			commanderButton,
			&cButtons[7]
		)
	};

	Button buttonsSettings[]={//STATE 2
		makeButton(//reset
			20, 200,
			100, 20,
			clrWhite, clrBrown,
			5, 0.8f,
			initPlayers, NULL
		),
		makeButton(//X button
			284, 6,
			30, 30,
			clrTransRed,
			clrOldRed,
			3,
			1.2f,
			zero,
			&buttonState
		)
	};

	int buttonsDefaultSize = sizeof(buttonsDefault)/sizeof(buttonsDefault[0]);
	int buttonsCDamageSize = sizeof(buttonsCDamage)/sizeof(buttonsCDamage[0]);
	int buttonsSettingsSize = sizeof(buttonsSettings)/sizeof(buttonsSettings[0]);
	

	// Main loop
	while (aptMainLoop())
	{
		readInputs();

		if (down & KEY_START) break; // break in order to return to hbmenu

		//buttons update
		if (buttonState == 0){
			buttonUpdate(buttonsDefault, buttonsDefaultSize);
		} else if(buttonState == 1) {
			buttonUpdate(buttonsCDamage, buttonsCDamageSize);
		} else if(buttonState == 2) {
			buttonUpdate(buttonsSettings, buttonsSettingsSize);
		}
		
		//clamp c damage
		for (int i = 0; i < sizeof(players)/sizeof(players[0]); i++){
			for (int j = 0; j < sizeof(players[i].commanderDamage)/sizeof(players[i].commanderDamage[0]); j++){
				if (players[i].commanderDamage[j] < 0){
					players[i].commanderDamage[j] = 0;
				}
			}
		}

		
		//------------------------------------------------------------------------------------
		//rendering start
		//------------------------------------------------------------------------------------
		C2D_Prepare();
		C3D_DepthTest(false, GPU_ALWAYS, GPU_WRITE_ALL);
		C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

		//draw dynamic text
		C2D_TextBufClear(dynamicBuffer);

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

		//numbers
		char lifeBuf[256];
		char cBuf[256];
		C2D_Text playerLifeText[4];
		C2D_Text playerCDamageText[4];

		//write life text
		for (int i = 0; i < sizeof(players)/sizeof(players[0]); i++){
			snprintf(lifeBuf, sizeof(lifeBuf), "%d", players[i].life);
			C2D_TextFontParse(&playerLifeText[i], font, dynamicBuffer, lifeBuf);
			C2D_TextOptimize(&playerLifeText[i]);
		}

		for (int i = 0; i < (sizeof(players[cDamageIndex].commanderDamage)/sizeof(players[cDamageIndex].commanderDamage[0])); i++){
			snprintf(cBuf, sizeof(cBuf), "%d", players[cDamageIndex].commanderDamage[i]);
			C2D_TextFontParse(&playerCDamageText[i], font, dynamicBuffer, cBuf);
			C2D_TextOptimize(&playerCDamageText[i]);
		}

		//state machine
		if (buttonState == 0){//default screen

			C2D_DrawText(&playerLifeText[0], C2D_WithColor | C2D_AtBaseline | C2D_AlignCenter, 100.0f, 40.0f, 0.0f, -3.0f, -3.0f, clrBlack);
			C2D_DrawText(&playerLifeText[1], C2D_WithColor | C2D_AtBaseline | C2D_AlignCenter, 300.0f, 40.0f, 0.0f, -3.0f, -3.0f, clrBlack);
			C2D_DrawText(&playerLifeText[2], C2D_WithColor | C2D_AtBaseline | C2D_AlignCenter, 100.0f, 200.0f, 0.0f, 3.0f, 3.0f, clrBlack);
			C2D_DrawText(&playerLifeText[3], C2D_WithColor | C2D_AtBaseline | C2D_AlignCenter, 300.0f, 200.0f, 0.0f, 3.0f, 3.0f, clrBlack);

		} else if (buttonState == 1){//C damage

			C2D_DrawRectSolid(0, 0, 0.0f, TOP_SCREEN_WIDTH, SCREEN_HEIGHT, C2D_Color32(0, 0, 0, 180));
			C2D_DrawRectSolid(coordsIndex[cDamageIndex][0], coordsIndex[cDamageIndex][1], 0.0f, 200, 120, C2D_Color32(255, 255, 255, 50));

			C2D_DrawText(&playerCDamageText[0], C2D_WithColor | C2D_AtBaseline | C2D_AlignCenter, 100.0f, 40.0f, 0.0f, -3.0f, -3.0f, clrOldRed);
			C2D_DrawText(&playerCDamageText[1], C2D_WithColor | C2D_AtBaseline | C2D_AlignCenter, 300.0f, 40.0f, 0.0f, -3.0f, -3.0f, clrOldBlue);
			C2D_DrawText(&playerCDamageText[2], C2D_WithColor | C2D_AtBaseline | C2D_AlignCenter, 100.0f, 200.0f, 0.0f, 3.0f, 3.0f, clrOldWhite);
			C2D_DrawText(&playerCDamageText[3], C2D_WithColor | C2D_AtBaseline | C2D_AlignCenter, 300.0f, 200.0f, 0.0f, 3.0f, 3.0f, clrOldGreen);

		} else if(buttonState == 2) {//settings

			C2D_DrawText(&playerLifeText[0], C2D_WithColor | C2D_AtBaseline | C2D_AlignCenter, 100.0f, 40.0f, 0.0f, -3.0f, -3.0f, clrBlack);
			C2D_DrawText(&playerLifeText[1], C2D_WithColor | C2D_AtBaseline | C2D_AlignCenter, 300.0f, 40.0f, 0.0f, -3.0f, -3.0f, clrBlack);
			C2D_DrawText(&playerLifeText[2], C2D_WithColor | C2D_AtBaseline | C2D_AlignCenter, 100.0f, 200.0f, 0.0f, 3.0f, 3.0f, clrBlack);
			C2D_DrawText(&playerLifeText[3], C2D_WithColor | C2D_AtBaseline | C2D_AlignCenter, 300.0f, 200.0f, 0.0f, 3.0f, 3.0f, clrBlack);
		}
		
		
		

		//------------------------------------------------------------------------------------
		//bottom screen
		C2D_TargetClear(bottom, clrBlack);
		C2D_SceneBegin(bottom);

		//state machine
		if (buttonState == 0){//default screen
			buttonDraw(buttonsDefault, buttonsDefaultSize);
		} else if (buttonState == 1){//C damage
			buttonDraw(buttonsCDamage, buttonsCDamageSize);
		} else if(buttonState == 2) {//settings
			buttonDraw(buttonsSettings, buttonsSettingsSize);
		}

		/*
		char buf[160];
		C2D_Text debugText;
		snprintf(buf, sizeof(buf),
			"CPU:     %6.2f%%\nGPU:     %6.2f%%\n", 
			C3D_GetProcessingTime()*6.0f,
			C3D_GetDrawingTime()*6.0f
		);
		C2D_TextParse(&debugText, dynamicBuffer, buf);
		C2D_TextOptimize(&debugText);
		C2D_DrawText(&debugText, C2D_WithColor, 3.0f, 3.0f, 0.0f, 0.75f, 0.75f, clrWhite);
		*/
		
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