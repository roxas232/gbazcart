#ifdef _MSC_VER
#define PACK4
#define ALIGN4
#else
#define PACK4 __attribute__((packed, aligned(4)))
#define ALIGN4 __attribute__((aligned(4)))
#endif

#include "car.h"
#include <string.h>

#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 160

#define MEM_IO   0x04000000
#define MEM_PAL  0x05000000
#define MEM_VRAM 0x06000000
#define MEM_OAM  0x07000000

#define REG_DISPLAY        (*((volatile uint32 *)(MEM_IO)))
#define REG_DISPLAY_VCOUNT (*((volatile uint32 *)(MEM_IO + 0x0006)))
#define REG_KEY_INPUT      (*((volatile uint32 *)(MEM_IO + 0x0130)))
#define REG_BG0_CONTROL    (*((volatile uint32*)(0x04000008)))

#define KEY_A       0x0001
#define KEY_B       0x0002
#define KEY_SELECT  0x0004
#define KEY_START   0x0008
#define KEY_RIGHT   0x0010
#define KEY_LEFT    0x0020
#define KEY_UP      0x0040
#define KEY_DOWN    0x0080
#define KEY_R       0x0100
#define KEY_L       0x0200
#define KEY_ANY     0x03FF

#define OBJECT_ATTR0_Y_MASK 0x0FF
#define OBJECT_ATTR1_X_MASK 0x1FF

typedef unsigned char uint8;
typedef unsigned short uint16;
typedef unsigned int uint32;
typedef uint16 rgb15;

typedef struct obj_attrs
{
	uint16 attr0;
	uint16 attr1;
	uint16 attr2;
	uint16 pad;
} PACK4 obj_attrs;

//typedef uint32 tile_4bpp[8];
//typedef tile_4bpp tile_block[512];

typedef uint32 tile_8bpp[16];
typedef tile_8bpp tile_block[256];

#define oam_mem				((volatile obj_attrs*)MEM_OAM)
#define tile_mem 			((volatile tile_block*)MEM_VRAM)
#define object_palette_mem	((volatile rgb15*)(MEM_PAL + 0x200))
#define bg_palette_mem		((volatile rgb15*)(MEM_PAL))

#define BG_TILES_LEN SCREEN_HEIGHT * SCREEN_WIDTH / 16
//#define BG_TILES_LEN 1
#define BG_PAL_LEN 8

const unsigned short bgPal[4] ALIGN4 =
{
	//0x4DA0,0x0000,0xFFFF,0x0000,
	0xFFFF,0x0000,0xFFFF,0x0000,
};

// Form a 16-bit BGR GBA color from 3 component vals
static inline rgb15 RGB15(int r, int g, int b)
{
	return r | (g << 5) | (b << 10);
}

// Set the position of an object to specified x and y coordinates
static inline void set_object_position(volatile obj_attrs* object, int x, int y)
{
	object->attr0 = (object->attr0 & ~OBJECT_ATTR0_Y_MASK) |
					(y & OBJECT_ATTR0_Y_MASK);
	object->attr1 = (object->attr1 & ~OBJECT_ATTR1_X_MASK) |
					(x & OBJECT_ATTR1_X_MASK);
}

// Clamp value to min - max inclusive
static inline int clamp(int value, int min, int max)
{
	return value < min ? min : value > max ? max : value;
}

static inline void loadCar()
{
    memcpy((void*)object_palette_mem, carPal,  carPalLen);
    memcpy((void*)&tile_mem[4][1], carTiles, carTilesLen);
}

static inline void drawBg()
{
	//REG_BG0_CONTROL = 0x0180;// 0000 0001 1000 0000;

	memcpy((void*)bg_palette_mem, bgPal, BG_PAL_LEN);

	uint16 tile[BG_TILES_LEN];
	for (int i = 0; i < BG_TILES_LEN; ++i)
	{
		tile[i] = 0x0202; // Some color
	}
	memcpy((void*)&tile_mem[0][0], tile, BG_TILES_LEN);
}

inline void vsync()
{
    // Skip past the rest of any current VBlank, then skip VDraw
    while (REG_DISPLAY_VCOUNT >= 160);
    while (REG_DISPLAY_VCOUNT < 160);
}

int main()
{
	drawBg();
	loadCar();

	// Create our sprites by writing their object attributes into OAM
	// memory
	volatile obj_attrs* paddle_attrs = &oam_mem[0];
    // 0010 0000 0000 0000
	paddle_attrs->attr0 = 0x2000; 	// 8bpp tiles, SQUARE shape, y coord 00
	// 1000 00000 0000 0000
    //http://www.coranac.com/tonc/text/regobj.htm
    paddle_attrs->attr1 = 0x8000; 	// 32x32 size when using the SQUARE shape
	paddle_attrs->attr2 = 2;		// Start at 1st tile in tile block 4, use color palatte 0

	// Init vars to keep track of the state of the apaddle and ball,
	// and set their initial positions (by modifying thier attrs in OAM)
	const int player_width 	= 32;
	const int player_height = 32;

	int player_velocity = 3;
	int player_x = 5;
	int player_y = SCREEN_HEIGHT - player_height;

	set_object_position(paddle_attrs, player_x, player_y);

	// Set the display parameters to enable objects, and use a 1D obj->tile mapping
	// and bg 0
	REG_DISPLAY = 0x1000 | 0x0040 | 0x0100;

	// It's go time
	uint32 key_states = 0;

	while (1)
	{
		vsync();

		// Get current key states (REG_KEY_INPUT stores the states inverted)
		key_states = ~REG_KEY_INPUT & KEY_ANY;

		// Note that our physics is tied to the framerate which is dumb.
		// Also this is awful collision handling

		int player_max_clamp_x = SCREEN_WIDTH - player_width;
		if (key_states & KEY_LEFT)
		{
			player_x = clamp(player_x - player_velocity, -1,
			                 player_max_clamp_x);
		}
		if (key_states & KEY_RIGHT)
		{
			player_x = clamp(player_x + player_velocity, -1,
			                 player_max_clamp_x);
		}
		if (key_states & KEY_LEFT || key_states & KEY_RIGHT)
		{
			set_object_position(paddle_attrs, player_x, player_y);
		}
	}

	return 0;
}
