
#include <exec/types.h>

#define KICKVERSION (39L) /* Amiga OS Release 3.0 */

#define IDCMP_FLAGS (IDCMP_RAWKEY | IDCMP_MOUSEBUTTONS | IDCMP_MOUSEMOVE)

enum Parts
{
	PART_MENU,
	PART_GAME,
	PART_QUIT
};

struct Game
{
	UBYTE colorDepth; /* Taken from graphics file */
	ULONG modeID; /* Taken from graphics file */
	ULONG *colorPalette;
	STRPTR gameTitle;
	struct TextFont *font;
	struct Screen *screen;
	struct Window *window;
	struct IFFHandle *iff;
	LONG err;
	struct BitMapHeader *bmhd;
	struct BitMap *gfxBitMap;
	WORD part;
};

struct BitMap *loadBitMap(struct Game *game, STRPTR name);
void freeBitMap(struct Game *game);
void menu(struct Game *game);
