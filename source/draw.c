// Copyright 2013 Normmatt
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

#include "font.h"
#include "draw.h"
#include "fs.h"
#ifdef USE_THEME
#include "theme.h"
#endif

ssize_t
decode_utf8(u32      *out,
            const u8 *in)
{
  u8 code1, code2, code3, code4;

  code1 = *in++;
  if(code1 < 0x80)
  {
    /* 1-byte sequence */
    *out = code1;
    return 1;
  }
  else if(code1 < 0xC2)
  {
    return -1;
  }
  else if(code1 < 0xE0)
  {
    /* 2-byte sequence */
    code2 = *in++;
    if((code2 & 0xC0) != 0x80)
    {
      return -1;
    }

    *out = (code1 << 6) + code2 - 0x3080;
    return 2;
  }
  else if(code1 < 0xF0)
  {
    /* 3-byte sequence */
    code2 = *in++;
    if((code2 & 0xC0) != 0x80)
    {
      return -1;
    }
    if(code1 == 0xE0 && code2 < 0xA0)
    {
      return -1;
    }

    code3 = *in++;
    if((code3 & 0xC0) != 0x80)
    {
      return -1;
    }

    *out = (code1 << 12) + (code2 << 6) + code3 - 0xE2080;
    return 3;
  }
  else if(code1 < 0xF5)
  {
    /* 4-byte sequence */
    code2 = *in++;
    if((code2 & 0xC0) != 0x80)
    {
      return -1;
    }
    if(code1 == 0xF0 && code2 < 0x90)
    {
      return -1;
    }
    if(code1 == 0xF4 && code2 >= 0x90)
    {
      return -1;
    }

    code3 = *in++;
    if((code3 & 0xC0) != 0x80)
    {
      return -1;
    }

    code4 = *in++;
    if((code4 & 0xC0) != 0x80)
    {
      return -1;
    }

    *out = (code1 << 18) + (code2 << 12) + (code3 << 6) + code4 - 0x3C82080;
    return 4;
  }

  return -1;
}

static char debugstr[DBG_N_CHARS_X * DBG_N_CHARS_Y] = { 0 };
static u32 debugcol[DBG_N_CHARS_Y] = { DBG_COLOR_FONT };

void ClearScreen(u8* screen, int width, int color)
{
    if (color == COLOR_TRANSPARENT) color = COLOR_BLACK;
    for (int i = 0; i < (width * SCREEN_HEIGHT); i++) {
        *(screen++) = color >> 16;  // B
        *(screen++) = color >> 8;   // G
        *(screen++) = color & 0xFF; // R
    }
}

void ClearScreenFull(bool clear_top, bool clear_bottom)
{
    if (clear_top)
        ClearScreen(TOP_SCREEN, SCREEN_WIDTH_TOP, STD_COLOR_BG);
    if (clear_bottom)
        ClearScreen(BOT_SCREEN, SCREEN_WIDTH_BOT, STD_COLOR_BG);
}

int DrawCharacter(u8* screen, u32 character, int x, int y, int color, int bgcolor)
{
#ifdef FONT_UNICODE
	FontIndex index;
	if(character >= 0xf900) {
		index = indies[character-0x2100];
	} else {
		index = indies[character];
	}
	int fontWidth = index.width;
	u8* data = index.data;
#else
	int fontWidth = FONT_WIDTH;
#endif

	u8 charPos;
	int xDisplacement;
	int yDisplacement;
	u8* screenPos;
	int col = fontWidth/8;
	int end = 0;
	if(fontWidth<8){
		end = 8 - fontWidth;
	}

    for (int yy = 0; yy < FONT_HEIGHT; yy++) {
		xDisplacement = (x * BYTES_PER_PIXEL * SCREEN_HEIGHT);
        yDisplacement = ((SCREEN_HEIGHT - (y + yy) - 1) * BYTES_PER_PIXEL);
        screenPos = screen + xDisplacement + yDisplacement;

		for(int c = 0; c < col; c++) {
#ifdef FONT_UNICODE
		charPos = data[FONT_HEIGHT * yy + c];
#else
        charPos = font[character * FONT_HEIGHT + yy];
#endif
        for (int xx = 7; xx >= end; xx--) {
            if ((charPos >> xx) & 1) {
                *(screenPos + 0) = color >> 16;  // B
                *(screenPos + 1) = color >> 8;   // G
                *(screenPos + 2) = color & 0xFF; // R
            } else if (bgcolor != COLOR_TRANSPARENT) {
                *(screenPos + 0) = bgcolor >> 16;  // B
                *(screenPos + 1) = bgcolor >> 8;   // G
                *(screenPos + 2) = bgcolor & 0xFF; // R
            }
            screenPos += BYTES_PER_PIXEL * SCREEN_HEIGHT;
        }
		}
    }

	return fontWidth;
}

void DrawString(u8* screen, const char *str, int x, int y, int color, int bgcolor)
{
	size_t len = strlen(str);
	u32 character;
	ssize_t  units = 0;
	int currentX = x;

    for (size_t i = 0; i < len ; i++) {
		units = decode_utf8(&character,str);
		if(units == -1) break;
		str += units;
		if(character>=0xd800 && i<0xf900) {
			continue;
		}else {
        	currentX += DrawCharacter(screen, str[i], currentX, y, color, bgcolor);
		}
	}
}

void DrawStringF(int x, int y, bool use_top, const char *format, ...)
{
    char str[512] = { 0 }; // 512 should be more than enough
    va_list va;

    va_start(va, format);
    vsnprintf(str, 512, format, va);
    va_end(va);

    for (char* text = strtok(str, "\n"); text != NULL; text = strtok(NULL, "\n"), y += 10)
        DrawString((use_top) ? TOP_SCREEN : BOT_SCREEN, text, x, y, STD_COLOR_FONT, STD_COLOR_BG);
}

void DrawStringFC(int x, int y, bool use_top, u32 color, const char *format, ...)
{
    char str[512] = { 0 }; // 512 should be more than enough
    va_list va;

    va_start(va, format);
    vsnprintf(str, 512, format, va);
    va_end(va);

    for (char* text = strtok(str, "\n"); text != NULL; text = strtok(NULL, "\n"), y += 10)
        DrawString((use_top) ? TOP_SCREEN : BOT_SCREEN, text, x, y, color, STD_COLOR_BG);
}

void Screenshot(const char* path)
{
    u8* buffer = (u8*) 0x21000000; // careful, this area is used by other functions in Decrypt9
    u8* buffer_t = buffer + (400 * 240 * 3);
    u8 bmp_header[54] = {
        0x42, 0x4D, 0x36, 0xCA, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x36, 0x00, 0x00, 0x00, 0x28, 0x00,
        0x00, 0x00, 0x90, 0x01, 0x00, 0x00, 0xE0, 0x01, 0x00, 0x00, 0x01, 0x00, 0x18, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0xCA, 0x08, 0x00, 0x12, 0x0B, 0x00, 0x00, 0x12, 0x0B, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };
    
    if (path == NULL) {
        static u32 n = 0;
        for (; n < 1000; n++) {
            char filename[16];
            snprintf(filename, 16, "snap%03i.bmp", (int) n);
            if (!FileOpen(filename)) {
                FileCreate(filename, true);
                break;
            }
            FileClose();
        }
        if (n >= 1000)
            return;
    } else {
        FileCreate(path, true);
    }
    
    memset(buffer, 0x1F, 400 * 240 * 3 * 2);
    for (u32 x = 0; x < 400; x++)
        for (u32 y = 0; y < 240; y++)
            memcpy(buffer_t + (y*400 + x) * 3, TOP_SCREEN + (x*240 + y) * 3, 3);
    for (u32 x = 0; x < 320; x++)
        for (u32 y = 0; y < 240; y++)
            memcpy(buffer + (y*400 + x + 40) * 3, BOT_SCREEN + (x*240 + y) * 3, 3);
    FileWrite(bmp_header, 54, 0);
    FileWrite(buffer, 400 * 240 * 3 * 2, 54);
    FileClose();
}

void DebugClear()
{
    memset(debugstr, 0x00, DBG_N_CHARS_X * DBG_N_CHARS_Y);
    for (u32 y = 0; y < DBG_N_CHARS_Y; y++)
        debugcol[y] = DBG_COLOR_FONT;
    ClearScreen(TOP_SCREEN, SCREEN_WIDTH_TOP, DBG_COLOR_BG);
    #if defined USE_THEME && defined GFX_DEBUG_BG
    LoadThemeGfx(GFX_DEBUG_BG, true);
    #endif
    LogWrite("");
    LogWrite(NULL);
}

void DebugSet(const char **strs)
{
    if (strs != NULL) for (int y = 0; y < DBG_N_CHARS_Y; y++) {
        int pos_dbgstr = DBG_N_CHARS_X * (DBG_N_CHARS_Y - 1 - y);
        snprintf(debugstr + pos_dbgstr, DBG_N_CHARS_X, "%-*.*s", DBG_N_CHARS_X - 1, DBG_N_CHARS_X - 1, strs[y]);
        debugcol[y] = DBG_COLOR_FONT;
    }
    
    int pos_y = DBG_START_Y;
    u32* col = debugcol + (DBG_N_CHARS_Y - 1);
    for (char* str = debugstr + (DBG_N_CHARS_X * (DBG_N_CHARS_Y - 1)); str >= debugstr; str -= DBG_N_CHARS_X, col--) {
        if (*str != '\0') {
            DrawString(TOP_SCREEN, str, DBG_START_X, pos_y, *col, DBG_COLOR_BG);
            pos_y += DBG_STEP_Y;
        }
    }
}

void DebugColor(u32 color, const char *format, ...)
{
    static bool adv_output = true;
    char tempstr[128] = { 0 }; // 128 instead of DBG_N_CHARS_X for log file 
    va_list va;
    
    va_start(va, format);
    vsnprintf(tempstr, 128, format, va);
    va_end(va);
    
    if (adv_output) {
        memmove(debugstr + DBG_N_CHARS_X, debugstr, DBG_N_CHARS_X * (DBG_N_CHARS_Y - 1));
        memmove(debugcol + 1, debugcol, (DBG_N_CHARS_Y - 1) * sizeof(u32));
    } else {
        adv_output = true;
    }
    
    *debugcol = color;
    if (*tempstr != '\r') { // not a good way of doing this - improve this later
        snprintf(debugstr, DBG_N_CHARS_X, "%-*.*s", DBG_N_CHARS_X - 1, DBG_N_CHARS_X - 1, tempstr);
        LogWrite(tempstr);
    } else {
        snprintf(debugstr, DBG_N_CHARS_X, "%-*.*s", DBG_N_CHARS_X - 1, DBG_N_CHARS_X - 1, tempstr + 1);
        adv_output = false;
    }
    
    DebugSet(NULL);
}

void Debug(const char *format, ...)
{
    char tempstr[128] = { 0 }; // 128 instead of DBG_N_CHARS_X for log file 
    va_list va;
    
    va_start(va, format);
    vsnprintf(tempstr, 128, format, va);
    DebugColor(DBG_COLOR_FONT, tempstr);
    va_end(va);
}

#if !defined(USE_THEME) || !defined(ALT_PROGRESS)
void ShowProgress(u64 current, u64 total)
{
    const u32 progX = SCREEN_WIDTH_TOP - 40;
    const u32 progY = SCREEN_HEIGHT - 20;
    
    if (total > 0) {
        char progStr[8];
        snprintf(progStr, 8, "%3llu%%", (current * 100) / total);
        DrawString(TOP_SCREEN, progStr, progX, progY, DBG_COLOR_FONT, DBG_COLOR_BG);
    } else {
        DrawString(TOP_SCREEN, "    ", progX, progY, DBG_COLOR_FONT, DBG_COLOR_BG);
    }
}
#endif
