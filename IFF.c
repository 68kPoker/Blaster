
#include <stdio.h>

#include <dos/dos.h>
#include <libraries/iffparse.h>
#include <datatypes/pictureclass.h>
#include <exec/memory.h>

#include <clib/dos_protos.h>
#include <clib/iffparse_protos.h>
#include <clib/graphics_protos.h>
#include <clib/exec_protos.h>

#include "Game.h"

#define RGB(c) ((c)|((c)<<8)|((c)<<16)|((c)<<24))

#define RowBytes(w) ((((w)+15)>>4)<<1)

static void closeIFF(struct Game *game)
{
	CloseIFF(game->iff);
	Close(game->iff->iff_Stream);
}

static BOOL openIFF(struct Game *game, STRPTR name, LONG mode)
{
	struct IFFHandle *iff = game->iff;
	LONG dosMode[] =
	{
		MODE_OLDFILE,
		MODE_NEWFILE
	};
	STRPTR modeText[] =
	{
		"reading",
		"writing"
	};

	if (iff->iff_Stream = Open(name, dosMode[mode]))
	{
		InitIFFasDOS(iff);
		if ((game->err = OpenIFF(iff, mode)) == 0)
		{
			return(TRUE);
		}
		else
			printf("Could not open IFF stream for %s (error code %ld)!\n", modeText[mode], game->err);
		Close(iff->iff_Stream);
	}
	else
		printf("Could not open IFF file '%s' for %s!\n", name, modeText[mode]);
}

static BOOL parseILBM(struct Game *game)
{
	struct IFFHandle *iff = game->iff;
	struct ContextNode *cn;
	LONG props[] =
	{
		ID_ILBM, ID_BMHD,
		ID_ILBM, ID_CMAP, /* Color palette */
		ID_ILBM, ID_CAMG /* DisplayID */
	};
	const WORD propCount = 3;

	if ((game->err = ParseIFF(iff, IFFPARSE_STEP)) == 0)
	{
		if (cn = CurrentChunk(iff))
		{
			if (cn->cn_Type == ID_ILBM && cn->cn_ID == ID_FORM)
			{
				if ((game->err = PropChunks(iff, props, propCount)) == 0)
				{
					if ((game->err = StopChunk(iff, ID_ILBM, ID_BODY)) == 0)
					{
						game->err = ParseIFF(iff, IFFPARSE_SCAN);
						if (game->err == 0 || game->err == IFFERR_EOC || game->err == IFFERR_EOF)
						{
							return(game->err);
						}
						else
							printf("Couldn't scan IFF file (error code %ld)!\n", game->err);
					}
					else
						printf("Could not install stop chunk (error code %ld)!\n", game->err);
				}
				else
					printf("Could not install property chunks (error code %ld)!\n", game->err);
			}
			else
				printf("Not an ILBM file!\n");
		}
		else
			printf("Could not get current chunk!\n");
	}
	else
		printf("Couldn't step into IFF file (error code %ld)!\n", game->err);
	return(FALSE);
}

static void freeColorMap(struct Game *game)
{
	FreeVec(game->colorPalette);
}

static ULONG *loadColorMap(struct Game *game, struct StoredProperty *sp)
{
	WORD colorCount = sp->sp_Size / 3;
	WORD c;
	UBYTE *data = sp->sp_Data;
	ULONG *palette;

	if (palette = AllocVec((colorCount * 3 + 2) * sizeof(ULONG), MEMF_PUBLIC))
	{
		palette[0] = colorCount << 16;
		for (c = 0; c < colorCount; c++)
		{
			UBYTE red, green, blue;
			red = *data++;
			green = *data++;
			blue = *data++;
			palette[(c * 3) + 1] = RGB(red);
			palette[(c * 3) + 2] = RGB(green);
			palette[(c * 3) + 3] = RGB(blue);
		}
		palette[(colorCount * 3) + 1] = 0L;
		return(palette);
	}
	else
		printf("Could not alloc memory for palette!\n");
	return(NULL);
}

static BOOL readRow(BYTE **bufPtr, LONG *sizePtr, BYTE *plane, WORD bpr, UBYTE cmp)
{
	BYTE *buf = *bufPtr;
	LONG size = *sizePtr;
	BYTE c, data;
	WORD count;
	if (cmp == cmpNone)
	{
		if (size < bpr)
		{
			return(FALSE);
		}
		CopyMem(buf, plane, bpr);
		size -= bpr;
		buf += bpr;
	}
	else if (cmp == cmpByteRun1)
	{
		while (bpr > 0)
		{
			if (size < 1)
			{
				return(FALSE);
			}
			size--;
			if ((c = *buf++) >= 0)
			{
				count = c + 1;
				if (size < count || bpr < count)
				{
					return(FALSE);
				}
				size -= count;
				bpr -= count;
				while (count-- > 0)
				{
					*plane++ = *buf++;
				}
			}
			else if (c != -128)
			{
				count = (-c) + 1;
				if (size < 1 || bpr < count)
				{
					return(FALSE);
				}
				size--;
				bpr -= count;
				data = *buf++;
				while (count-- > 0)
				{
					*plane++ = data;
				}
			}
		}
	}
	else
	{
		return(FALSE);
	}
	*bufPtr = buf;
	*sizePtr = size;
	return(TRUE);
}

static struct BitMap *readBitMap(struct Game *game)
{
	struct BitMap *bm;
	struct BitMapHeader *bmhd = game->bmhd;
	WORD width = bmhd->bmh_Width;
	WORD height = bmhd->bmh_Height;
	WORD depth = bmhd->bmh_Depth;
	UBYTE cmp = bmhd->bmh_Compression;
	UBYTE msk = bmhd->bmh_Masking;
	struct ContextNode *cn;
	BYTE *buf, *curBuf;
	LONG curSize;
	WORD plane, row;
	PLANEPTR planes[8];
	WORD bpr = RowBytes(width);
	BOOL success = FALSE;

	if (msk == mskNone || msk == mskHasTransparentColor)
	{
		if (bm = AllocBitMap(width, height, depth, BMF_INTERLEAVED, NULL))
		{
			if (cn = CurrentChunk(game->iff))
			{
				if (buf = AllocMem(cn->cn_Size, MEMF_PUBLIC))
				{
					if (ReadChunkBytes(game->iff, buf, cn->cn_Size) == cn->cn_Size)
					{
						curBuf = buf;
						curSize = cn->cn_Size;
						for (plane = 0; plane < depth; plane++)
						{
							planes[plane] = bm->Planes[plane];
						}
						for (row = 0; row < height; row++)
						{
							for (plane = 0; plane < depth; plane++)
							{
								if (!(success = readRow(&curBuf, &curSize, planes[plane], bpr, cmp)))
								{
									break;
								}
								planes[plane] += bm->BytesPerRow;
							}
							if (!success)
							{
								break;
							}
						}
						if (success)
						{
							FreeMem(buf, cn->cn_Size);
							return(bm);
						}
						else
							printf("ILBM decompression error!\n");
					}
					else
						printf("Could not read %ld bytes from IFF file!\n", cn->cn_Size);
					FreeMem(buf, cn->cn_Size);
				}
				else
					printf("Could not alloc BODY buffer!\n");
			}
			else
				printf("Could not get current chunk!\n");
			FreeBitMap(bm);
		}
		else
			printf("Could not alloc bitmap for graphics!\n");		
	}
	else
		printf("Unsupported ILBM masking!\n");
	return(NULL);
}

void freeBitMap(struct Game *game)
{
	FreeBitMap(game->gfxBitMap);
	freeColorMap(game);
}

struct BitMap *loadBitMap(struct Game *game, STRPTR name)
{
	struct IFFHandle *iff;
	struct StoredProperty *sp;

	if (game->iff = iff = AllocIFF())
	{
		if (openIFF(game, name, IFFF_READ))
		{
			if (parseILBM(game) == 0)
			{
				if (sp = FindProp(iff, ID_ILBM, ID_BMHD))
				{
					game->bmhd = (struct BitMapHeader *)sp->sp_Data;
					if (sp = FindProp(iff, ID_ILBM, ID_CAMG))
					{
						game->modeID = *(ULONG *)sp->sp_Data;
						if (sp = FindProp(iff, ID_ILBM, ID_CMAP))
						{
							if (game->colorPalette = loadColorMap(game, sp))
							{
								if (game->gfxBitMap = readBitMap(game))
								{
									closeIFF(game);
									FreeIFF(iff);
									return(game->gfxBitMap);
								}
								freeColorMap(game);
							}
						}
						else
							printf("CMAP chunk missing!\n");
					}
					else
						printf("CAMG chunk missing!\n");
				}
				else
					printf("BMHD chunk missing!\n");
			}
			closeIFF(game);
		}
		FreeIFF(iff);
	}
	else
		printf("Could not alloc IFF!\n");
	return(NULL);
}
