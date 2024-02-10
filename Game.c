
#include <stdio.h>

#include <intuition/screens.h>

#include <clib/exec_protos.h>
#include <clib/graphics_protos.h>
#include <clib/intuition_protos.h>
#include <clib/diskfont_protos.h>

#include "Game.h"

struct Library *IntuitionBase;

static void closeLibs(void)
{
	CloseLibrary(IntuitionBase);
}

static BOOL openLibs(void)
{
	if (IntuitionBase = OpenLibrary("intuition.library", KICKVERSION))
	{
		return(TRUE);
	}
	else
		printf("Could not open intuition.library V%ld!\n", KICKVERSION);

	return(FALSE);
}

static void closeScreen(struct Game *game)
{	
	CloseScreen(game->screen);
	CloseFont(game->font);
}

static struct Screen *openScreen(struct Game *game)
{
	struct Screen *pubs, *s;
	static UWORD pens[] = { ~0 };
	static struct TextAttr ta =
	{
		"centurion.font", 9, FS_NORMAL, FPF_DISKFONT | FPF_DESIGNED
	};

	if (pubs = LockPubScreen(NULL))
	{
		ULONG pubsID = GetVPModeID(&pubs->ViewPort);
		ULONG monID = pubsID & MONITOR_ID_MASK;
		ULONG modeID = BestModeID(
			BIDTAG_MonitorID, monID,
			BIDTAG_SourceID, game->modeID,
			BIDTAG_Depth, game->colorDepth,
			TAG_DONE);

		UnlockPubScreen(NULL, pubs);

		if (modeID != INVALID_ID)
		{
			if (game->font = OpenDiskFont(&ta))
			{
				if (s = OpenScreenTags(NULL,
					SA_DisplayID, modeID,
					SA_Depth, game->colorDepth,
					SA_BackFill, LAYERS_NOBACKFILL,
					SA_Title, game->gameTitle,
					SA_Font, &ta,
					SA_Pens, pens,
					SA_SharePens, TRUE,
					SA_Exclusive, TRUE,
					SA_Quiet, TRUE,
					SA_ShowTitle, FALSE,
					SA_Colors32, game->colorPalette,
					SA_BlockPen, 0,
					SA_DetailPen, 1,
					SA_Interleaved, TRUE,
					TAG_DONE))
				{
					return(s);
				}
				else
					printf("Could not open screen!\n");
				CloseFont(game->font);
			}
			else
				printf("Could not open disk font %s size %d!\n", ta.ta_Name, ta.ta_YSize);
		}
		else
			printf("Could not find best display mode!\n");
	}
	else
		printf("Could not lock default public screen!\n");
	return(NULL);
}

static void closeWindow(struct Game *game)
{
	CloseWindow(game->window);
}

static struct Window *openWindow(struct Game *game) 
{
	struct Screen *s = game->screen;
	struct Window *w;

	if (w = OpenWindowTags(NULL,
		WA_CustomScreen, s,
		WA_BackFill, LAYERS_NOBACKFILL,
		WA_SimpleRefresh, TRUE,
		WA_Backdrop, TRUE,
		WA_Borderless, TRUE,
		WA_Left, 0,
		WA_Top, 0,
		WA_Width, s->Width,
		WA_Height, s->Height,
		WA_Activate, TRUE,
		WA_RMBTrap, TRUE,
		WA_IDCMP, IDCMP_FLAGS,
		WA_ReportMouse, TRUE,
		TAG_DONE))
	{
		return(w);
	}
	else
		printf("Could not open window!\n");
	return(NULL);
}

static void cleanup(struct Game *game)
{
	closeWindow(game);
	closeScreen(game);
	freeBitMap(game);
	closeLibs();
}

static BOOL init(struct Game *game)
{
	if (openLibs())
	{
		if (loadBitMap(game, "Base/Graphics.iff"))
		{
		    game->colorDepth = game->gfxBitMap->Depth;
			if (game->screen = openScreen(game))
			{
				if (game->window = openWindow(game))
				{
					return(TRUE);
				}
				closeScreen(game);
			}
			freeBitMap(game);
		}
		closeLibs();
	}
	return(FALSE);
}

int main(void)
{
	static struct Game game;

	if (init(&game))
	{
		menu(&game);
		cleanup(&game);
	}
	return(0);
}
