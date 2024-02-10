
#include <string.h>

#include <intuition/intuition.h>

#include <clib/graphics_protos.h>
#include <clib/exec_protos.h>

#include "Game.h"

#define ESC_KEY (0x45)
#define F1_KEY  (0x50)

enum Strings
{
	MENU_TITLE,
	MENU_PLAY,
	MENU_OPTIONS,	
	MENU_QUIT,
	MENU_STRINGS
};

enum Signals
{
	SIGNAL_USER,
	SIGNALS
};

static void displayMenu(struct Game *game)
{
	/* Display title menu */
	struct Window *w = game->window;
	struct RastPort *rp = w->RPort;
	struct BitMap *gfx = game->gfxBitMap;
	struct TextFont *font = rp->Font;
	WORD len, pos, y, gap = 8;
	WORD i;
	UBYTE menuPen = w->DetailPen, backPen = w->BlockPen;
	STRPTR text[] =
	{
		"Blaster v1.0",
		"F1 - Play",
		"F2 - Options",
		"ESC - Quit"
	};

	SetRast(w->RPort, 0);

	y = font->tf_Baseline;
	SetABPenDrMd(rp, menuPen, backPen, JAM1);
	for (i = 0; i < MENU_STRINGS; i++)
	{
		len = strlen(text[i]);
		pos = (w->Width - TextLength(rp, text[i], len)) / 2;
		Move(rp, pos, y);
		Text(rp, text[i], len);

		y += font->tf_YSize + gap;
	}
	game->part = PART_MENU;
}

static void startGame(struct Game *game)
{
	struct Window *w = game->window;

	SetRast(w->RPort, 0);

	/* Draw space ship */

	BltBitMapRastPort(game->gfxBitMap, 0, 0, w->RPort, 0, 0, 32, 64, 0xc0);
	game->part = PART_GAME;
}

static void quitGame(struct Game *game)
{
	game->part = PART_QUIT;
}

static BOOL handleUser(struct Game *game)
{
	struct IntuiMessage *msg;
	BOOL skip = FALSE;
	
	while (msg = (struct IntuiMessage *)GetMsg(game->window->UserPort))
	{
		ULONG cls = msg->Class;
		UWORD code = msg->Code;
		WORD mx = msg->MouseX;
		WORD my = msg->MouseY;

		ReplyMsg((struct Message *)msg);

		if (skip)
		{
			/* Clean pending messages queue */
			continue;
		}

		if (cls == IDCMP_RAWKEY)
		{
			switch (game->part)
			{
			case PART_MENU:
				switch (code)
				{
				case ESC_KEY:
					quitGame(game);
					return(FALSE);
				case F1_KEY:
					startGame(game);
					skip = TRUE;
					break;
				}
				break;
			case PART_GAME:
				switch (code)
				{
				case ESC_KEY:
					displayMenu(game);
					skip = TRUE;
					break;
				}
				break;
			}
		}
	}
	return(TRUE);
}

static void handleAnim(struct Game *game)
{

}

void menu(struct Game *game)
{
	ULONG sig[] =
	{
		1L << game->window->UserPort->mp_SigBit
	}, total = 0, result;
	WORD i;
	BOOL done = FALSE;
	BOOL (*sigHandler[SIGNALS])(struct Game *game) = 
	{ 
		handleUser
	};

	displayMenu(game);

	for (i = 0; i < SIGNALS; i++)
	{
		total |= sig[i];
	}
	while (!done)
	{
		WaitTOF();
		/* Animation */
		handleAnim(game);

		result = SetSignal(0L, total);

		for (i = 0; i < SIGNALS; i++)
		{
			if (result & sig[i])
			{
				if (!sigHandler[i](game))
				{
					break;
				}
			}
		}
		if (game->part == PART_QUIT)
		{
			done = TRUE;
		}
	}
}
