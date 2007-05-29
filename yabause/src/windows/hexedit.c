/*  Copyright 2007 Theo Berkau

    This file is part of Yabause.

    Yabause is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    Yabause is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Yabause; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include <windows.h>
#include "../core.h"
#include "../memory.h"
#include "../debug.h"
#include "hexedit.h"

typedef struct
{
   HWND hwnd;
   HFONT font;
   COLORREF text_color1;
   COLORREF text_color2;
   COLORREF text_color3;
   COLORREF edit_color;
   COLORREF bg_color;
   int hasfocus;
   u32 addr;
   SCROLLINFO scrollinfo;
   TEXTMETRIC fontmetric;
   int curx, cury;
   int maxcurx, maxcury;
   int curmode;
   int editmode;
   addrlist_struct *addrlist;
   int numaddr;
} HexEditCtl_struct;

//////////////////////////////////////////////////////////////////////////////

enum CURMODE { HEXMODE = 0x00, ASCIIMODE };

//////////////////////////////////////////////////////////////////////////////

void HexSetAddr(HexEditCtl_struct *cc, int num, u32 start, u32 end, const char *typetext)
{
   cc->addrlist[num].start = start;
   cc->addrlist[num].end = end;
}

//////////////////////////////////////////////////////////////////////////////

LRESULT InitHexEditCtl(HWND hwnd, WPARAM wParam, LPARAM lParam)
{
   HexEditCtl_struct *cc;

   if ((cc = (HexEditCtl_struct *)malloc(sizeof(HexEditCtl_struct))) == NULL)
      return FALSE;

   cc->numaddr = 13;

   if ((cc->addrlist = (addrlist_struct *)malloc(sizeof(addrlist_struct) * cc->numaddr)) == NULL)
   {
      free(cc);
      return FALSE;
   }

   HexSetAddr(cc, 0, 0x00000000, 0x0007FFFF, "Bios ROM"); 
   HexSetAddr(cc, 1, 0x00180000, 0x0018FFFF, "Backup RAM");
   HexSetAddr(cc, 2, 0x00200000, 0x002FFFFF, "Low Work RAM");
   HexSetAddr(cc, 3, 0x02000000, 0x03FFFFFF, "A-bus CS0");
   HexSetAddr(cc, 4, 0x04000000, 0x04FFFFFF, "A-bus CS1");
   HexSetAddr(cc, 5, 0x05000000, 0x057FFFFF, "A-bus Dummy");
   HexSetAddr(cc, 6, 0x05800000, 0x058FFFFF, "A-bus CS2");
   HexSetAddr(cc, 7, 0x05A00000, 0x05AFFFFF, "68k RAM");
   HexSetAddr(cc, 8, 0x05C00000, 0x05C7FFFF, "VDP1 RAM");
   HexSetAddr(cc, 9, 0x05C80000, 0x05CBFFFF, "VDP1 Framebuffer");
   HexSetAddr(cc, 10, 0x05E00000, 0x05E7FFFF, "VDP2 RAM");
   HexSetAddr(cc, 11, 0x05F00000, 0x05F00FFF, "VDP2 Color RAM");
   HexSetAddr(cc, 12, 0x06000000, 0x060FFFFF, "High Work RAM");

   cc->hwnd = hwnd;
   cc->font = GetStockObject(DEFAULT_GUI_FONT);
   cc->text_color1 = GetSysColor(COLOR_WINDOWTEXT);
   cc->text_color2 = RGB(0, 0, 255);
   cc->text_color3 = RGB(0, 0, 128);
   cc->edit_color = GetSysColor(COLOR_WINDOWTEXT);
   cc->bg_color = GetSysColor(COLOR_WINDOW);
   cc->hasfocus = FALSE;
   cc->addr = 0;
   cc->curx = 0;
   cc->cury = 0;
   cc->maxcurx = 16;
   cc->maxcury = 16;
   cc->curmode = HEXMODE;
   cc->editmode = 0;

   // Set the text
   SetWindowText(hwnd, ((CREATESTRUCT *)lParam)->lpszName);

   // Retrieve scroll info
   cc->scrollinfo.cbSize = sizeof(SCROLLINFO);
   cc->scrollinfo.fMask = SIF_RANGE;
   GetScrollInfo(hwnd, SB_VERT, &cc->scrollinfo);

   // Set our newly created structure to the extra area                                                                                                SetCustCtrl(hwnd, ccp);
   SetWindowLong(hwnd, 0, (LONG)cc);
   return TRUE;
}

//////////////////////////////////////////////////////////////////////////////

void DestroyHexEditCtl(HexEditCtl_struct *cc)
{
   if (cc)
   {
      if (cc->addrlist)
         free(cc->addrlist);
      free(cc);
   }
}

//////////////////////////////////////////////////////////////////////////////

LRESULT HexEditCtl_SetFont(HexEditCtl_struct *cc, WPARAM wParam, LPARAM lParam)
{
   HDC hdc;
   HFONT oldfont;

   cc->font = (HFONT)wParam;
   hdc = GetDC(cc->hwnd);
   oldfont = SelectObject(hdc, cc->font);
   GetTextMetrics(hdc, &cc->fontmetric);
   SelectObject(hdc, oldfont);
   ReleaseDC(cc->hwnd, hdc);
   return FALSE;
}

//////////////////////////////////////////////////////////////////////////////

void HexEditCtl_SetCaretPos(HexEditCtl_struct *cc)
{
   if (cc->curmode == HEXMODE)   
      SetCaretPos((10 + (cc->curx*2) + (cc->curx / 2) + cc->editmode) * cc->fontmetric.tmAveCharWidth,
                  cc->cury*cc->fontmetric.tmHeight);
   else
      SetCaretPos((10+32+8+ cc->curx) * cc->fontmetric.tmAveCharWidth,
                  cc->cury*cc->fontmetric.tmHeight);
}

//////////////////////////////////////////////////////////////////////////////

void HexEditCtl_SetFocus(HexEditCtl_struct *cc)
{
   cc->hasfocus = TRUE;
   CreateCaret(cc->hwnd, NULL, 2, cc->fontmetric.tmHeight);
   HexEditCtl_SetCaretPos(cc);
   ShowCaret(cc->hwnd);

   InvalidateRect(cc->hwnd, NULL, FALSE);
   UpdateWindow(cc->hwnd); 
}

//////////////////////////////////////////////////////////////////////////////

void HexEditCtl_KillFocus(HexEditCtl_struct *cc)
{
   cc->hasfocus = FALSE;
   DestroyCaret(); 
   InvalidateRect(cc->hwnd, NULL, FALSE);
   UpdateWindow(cc->hwnd); 
}

//////////////////////////////////////////////////////////////////////////////

int IsAddressValid(HexEditCtl_struct *cc, u32 addr, int *pos)
{
   int i;

   for (i = 0; i < cc->numaddr; i++)
   {
      if (addr >= cc->addrlist[i].start && addr <= cc->addrlist[i].end)
      {
         if (pos)
            *pos = i;
         return TRUE;
      }
   }

   return FALSE;
}

//////////////////////////////////////////////////////////////////////////////

int GetLinesAvailable(HexEditCtl_struct *cc, u32 addr, u32 endaddr)
{
   int i, i2;

   if (IsAddressValid(cc, addr, &i) == TRUE)
   {
      int lines=0;

      for (i2 = i; i2 < cc->numaddr; i2++)
      {           
         // figure out how far away from the end we are
         if (endaddr >= cc->addrlist[i2].start && endaddr <= cc->addrlist[i2].end)
         {
            if (addr < cc->addrlist[i2].start || addr > cc->addrlist[i2].end)
               addr = cc->addrlist[i2].start;
            lines += ((endaddr - addr) / cc->maxcurx);
            break;
         }
         lines += ((cc->addrlist[i2].end + 1 - addr) / cc->maxcurx);

         if (i2 != (cc->numaddr-1))
            addr = cc->addrlist[i2].start;
      }

      return lines;
   }

   return -1;
}

//////////////////////////////////////////////////////////////////////////////

int CalcCurPosFromAddr(HexEditCtl_struct *cc, u32 addr)
{
   int lines=GetLinesAvailable(cc, addr, cc->addrlist[cc->numaddr-1].end+1);

   if (lines != -1)
   {
      // figure out the cury position
      cc->curx = addr & 0xF;
      if (lines >= cc->maxcury)
         cc->cury = 0;
      else
         cc->cury = cc->maxcury - lines;

      return TRUE;
   }

   return FALSE;
}

//////////////////////////////////////////////////////////////////////////////

LRESULT HexEditCtl_OnPaint(HexEditCtl_struct *cc, WPARAM wParam, LPARAM lParam)
{
   HDC hdc=(HDC)wParam;
   PAINTSTRUCT ps;
   RECT rect;
   HANDLE oldfont;
   int x=0, y=0;
   char text[MAX_PATH];
   u32 addr;
   SIZE size;
   int i;
   RECT clip;
   int curaddr=-1;

   addr = cc->addr;
   if (IsAddressValid(cc, addr, &curaddr) == FALSE)
      return FALSE;

   // Setup everything for rendering
   if (hdc == NULL)
      hdc = BeginPaint(cc->hwnd, &ps);
   oldfont = SelectObject(hdc, cc->font);

   GetClientRect(cc->hwnd, &rect);

   for(;;)
   {
      x = 0;
      sprintf(text, "%08X  ", (int)addr);
      GetTextExtentPoint32(hdc, text, strlen(text), &size);
      if (size.cy+y >= rect.bottom)
         break;

      // adjust clipping values
      if (y+(size.cy*2) >= rect.bottom)
         clip.bottom = rect.bottom;
      else
         clip.bottom = y+size.cy;

      // Draw the address text
      clip.left = x;
      clip.top = y;
      clip.right = clip.left+size.cx;
      SetTextColor(hdc, cc->text_color1);
      ExtTextOut(hdc, x, y, ETO_OPAQUE | ETO_CLIPPED, &clip, text, lstrlen(text), 0);
      x += size.cx;

      // Draw the Hex values for the address
      for (i = 0; i < cc->maxcurx; i+=2)
      {
         if ((i % 4) == 0)
            SetTextColor(hdc, cc->text_color2);
         else
            SetTextColor(hdc, cc->text_color3);
         sprintf(text, "%02X%02X ", MappedMemoryReadByte(addr), MappedMemoryReadByte(addr+1));
         GetTextExtentPoint32(hdc, text, strlen(text), &size);
         clip.left = x;
         clip.top = y;
         clip.right = clip.left+size.cx;
         ExtTextOut(hdc, x, y, ETO_OPAQUE | ETO_CLIPPED, &clip, text, lstrlen(text), 0);
         x += size.cx;
         addr += 2;
      }
      addr -= cc->maxcurx;

      // Draw the ANSI equivalents
      SetTextColor(hdc, cc->text_color1);

      for (i = 0; i < cc->maxcurx; i++)
      {
         u8 byte=MappedMemoryReadByte(addr);

         if (byte < 0x20 || byte >= 0x7F)
            byte = '.';

         text[0] = byte;
         text[1] = '\0';
         GetTextExtentPoint32(hdc, text, strlen(text), &size);
         clip.left = x;
         clip.top = y;
         clip.right = rect.right;
         ExtTextOut(hdc, x, y, ETO_OPAQUE | ETO_CLIPPED, &clip, text, lstrlen(text), 0);
         x += size.cx;
         addr++;
      }
      y += size.cy;

      if (addr > cc->addrlist[curaddr].end)
      {
         curaddr++;
         addr = cc->addrlist[curaddr].start;
      }
   }

   // Let's clean up, and we're done
   SelectObject(hdc, oldfont);

   if ((HDC)wParam == NULL)
      EndPaint(cc->hwnd, &ps);

   return FALSE;
}

//////////////////////////////////////////////////////////////////////////////

void MoveCursor(HexEditCtl_struct *cc, int offset)
{
   static int pos;
   u32 addr;
   int i;

   if (offset < 0)
   {
      // See if the address is actually in our address list

      if (offset == -(cc->maxcurx * cc->maxcury))
         addr = cc->addr + offset;
      else
         addr = cc->addr + (offset+(cc->cury * cc->maxcurx)+cc->curx);

      if (IsAddressValid(cc, addr, NULL))
      {
         // Yes it is, so we can go through and setup the next address and cursor position
         if (offset == -1)
         {
            // Left press
            if (cc->curx == 0)
            {
               if (cc->cury == 0)
               {
                  cc->addr -= cc->maxcurx;
                  InvalidateRect(cc->hwnd, NULL, FALSE);
               }
               else
                  cc->cury--;

               cc->curx = cc->maxcurx-1;
            }
            else
               cc->curx--;

            HexEditCtl_SetCaretPos(cc);
         }
         else
         {
            // Up/Page Up press
            if ((-offset) > (cc->cury * cc->maxcurx))
               cc->addr += offset;
            if (offset == -(cc->maxcurx))
            {
               // Up press
               if (cc->cury != 0)
                  cc->cury--;
               HexEditCtl_SetCaretPos(cc);
            }
            InvalidateRect(cc->hwnd, NULL, FALSE);
         }
      }
      else
      {
         // Phew, we're going to have to figure out if there's a previous
         // memory to move to

         IsAddressValid(cc, cc->addr, &pos);
          
         if (offset == -1)
         {
            cc->curx = cc->maxcurx-1;
            offset = -cc->maxcurx;
            HexEditCtl_SetCaretPos(cc);
         }

         if (pos > 0 &&
             GetLinesAvailable(cc, cc->addrlist[0].start, cc->addr) >= ((-offset) / cc->maxcurx))
         {
            // Figure out which one it is
            for (i = pos-1; i >= 0; i--)
            {
               int lines;

               if ((lines = GetLinesAvailable(cc, cc->addrlist[i].start, cc->addr)) >= ((-offset) / cc->maxcurx))
               {
                  // Now figure out the new address
                  cc->addr = cc->addrlist[i].start + ((lines * cc->maxcurx) - (-offset));
                  break;
               }
            }

            InvalidateRect(cc->hwnd, NULL, FALSE);
         }
         else
         {
            cc->addr = cc->addrlist[0].start;
            cc->curx = cc->cury = 0;
            InvalidateRect(cc->hwnd, NULL, FALSE);
            HexEditCtl_SetCaretPos(cc);
         }
      }
   }
   else if (offset > 0)
   {
      // See if the address is actually in our address list

      if ((offset+(cc->maxcurx * cc->cury) + cc->curx) >= (cc->maxcurx * cc->maxcury))
         addr = cc->addr + offset;
      else
         addr = cc->addr;

      if (IsAddressValid(cc, addr, NULL) &&
          GetLinesAvailable(cc, addr, cc->addrlist[cc->numaddr-1].end+1) >= cc->maxcury)
      {
         // Yes it is, so we can go through and setup the next address and cursor position
         if (offset == 1)
         {
            // Right press
            if (cc->curx == (cc->maxcurx-1))
            {
               if (cc->cury == (cc->maxcury-1))
               {
                  cc->addr += cc->maxcurx;
                  cc->curx = 0;
                  InvalidateRect(cc->hwnd, NULL, FALSE);
               }
               else
               {
                  cc->cury++;
                  cc->curx = 0;
               }
            }
            else
               cc->curx++;

            HexEditCtl_SetCaretPos(cc);
         }
         else
         {
            // Down/Page Down press
            if (((cc->cury * cc->maxcurx)+offset) >= (cc->maxcurx * cc->maxcury))
               cc->addr += offset;
            if (offset == cc->maxcurx)
            {
               // Down press
               if (cc->cury < (cc->maxcury-1))
                  cc->cury++;
               HexEditCtl_SetCaretPos(cc);
            }
            InvalidateRect(cc->hwnd, NULL, FALSE);
         }
      }
      else
      {
         // Phew, we're going to have to figure out if there's a next memory
         // to move to

         IsAddressValid(cc, cc->addr, &pos);

         if (pos > 0 &&
             GetLinesAvailable(cc, cc->addr, cc->addrlist[cc->numaddr-1].end+1) >= (cc->maxcury + (offset / cc->maxcurx)))
         {
            // Figure out which one it is
            for (i = pos+1; i < cc->numaddr; i++)
            {
               int lines;

               if ((lines = GetLinesAvailable(cc, cc->addr, cc->addrlist[i].end+1)) >= (offset / cc->maxcurx))
               {
                  // Now figure out the new address
                  cc->addr = cc->addrlist[i].start+offset-(cc->addrlist[pos].end+1-cc->addr);
                  break;
               }
            }

            InvalidateRect(cc->hwnd, NULL, FALSE);
         }
         else
         {
            for (i = cc->numaddr-1; i >= 0; i--)
            {
               int lines;

               if ((lines = GetLinesAvailable(cc, cc->addrlist[i].start, cc->addrlist[cc->numaddr-1].end+1)) >= cc->maxcury)
               {
                  // Now figure out the new address
                  cc->addr = cc->addrlist[i].start + ((lines-cc->maxcury) * cc->maxcurx);
                  break;
               }
            }

            // adjust cursor position
            cc->curx = cc->maxcurx-1;
            cc->cury = cc->maxcury-1;
           
            InvalidateRect(cc->hwnd, NULL, FALSE);
            HexEditCtl_SetCaretPos(cc);
         }
      }
   }
}

//////////////////////////////////////////////////////////////////////////////

LRESULT HexEditCtl_Vscroll(HexEditCtl_struct *cc, WPARAM wParam, LPARAM lParam)
{
   switch (LOWORD(wParam))
   {
      case SB_LINEDOWN:
      {
         int oldcury=cc->cury;
         cc->cury = cc->maxcury-1;
         MoveCursor(cc, cc->maxcurx);
         cc->cury=oldcury;
         HexEditCtl_SetCaretPos(cc);
         return 0;
      }
      case SB_LINEUP:
      {
         int oldcury=cc->cury;
         cc->cury = 0;
         MoveCursor(cc, -cc->maxcurx);
         cc->cury=oldcury;
         HexEditCtl_SetCaretPos(cc);
         return 0;
      }
      case SB_PAGEDOWN:
         MoveCursor(cc, cc->maxcurx*cc->maxcury);
         return 0;
      case SB_PAGEUP:
         MoveCursor(cc, -(cc->maxcurx*cc->maxcury));
         return 0;
      case SB_THUMBTRACK:
//         cc->addr = 0xFFFFFF00 / 64 * HIWORD(wParam);
//         InvalidateRect(cc->hwnd, NULL, FALSE);
         return 0;
      case SB_THUMBPOSITION:
         SetScrollPos(cc->hwnd, SB_VERT, HIWORD(wParam), TRUE);
         return 0;
      default:
         break;
   }

   return 0;
}

//////////////////////////////////////////////////////////////////////////////

LRESULT HexEditCtl_KeyDown(HexEditCtl_struct *cc, WPARAM wParam, LPARAM lParam)
{
   switch (wParam)
   {
      case VK_LEFT:
         MoveCursor(cc, -1);
         break;
      case VK_UP:
         MoveCursor(cc, -cc->maxcurx);
         break;
      case VK_DOWN:
         MoveCursor(cc, cc->maxcurx);
         break;
      case VK_RIGHT:
         MoveCursor(cc, 1);
         break;
      case VK_PRIOR:
         MoveCursor(cc, -(cc->maxcurx*cc->maxcury));
         break;
      case VK_NEXT:
         MoveCursor(cc, (cc->maxcurx*cc->maxcury));
         break;
      case VK_END:
      {
         int i;
         for (i = cc->numaddr-1; i >= 0; i--)
         {
            int lines;

            if ((lines = GetLinesAvailable(cc, cc->addrlist[i].start, cc->addrlist[cc->numaddr-1].end+1)) >= cc->maxcury)
            {
               // Now figure out the new address
               cc->addr = cc->addrlist[i].start + ((lines-cc->maxcury) * cc->maxcurx);
               break;
            }
         }

         // adjust cursor position
         cc->curx = cc->maxcurx-1;
         cc->cury = cc->maxcury-1;
           
         InvalidateRect(cc->hwnd, NULL, FALSE);
         HexEditCtl_SetCaretPos(cc);
         break;
      }
      case VK_HOME:
         cc->addr = cc->addrlist[0].start;
         cc->curx = cc->cury = 0;
         InvalidateRect(cc->hwnd, NULL, FALSE);
         HexEditCtl_SetCaretPos(cc);
         break;
      case VK_TAB:
         cc->curmode ^= ASCIIMODE;
         HexEditCtl_SetCaretPos(cc);
         break;
      default:
         if (cc->curmode == HEXMODE)
         {
            if ((wParam >= '0' && wParam <= '9') ||
                (wParam >= 'A' && wParam <= 'F'))
            {
               u32 addr;
               u8 data;

               if (wParam >= '0' && wParam <= '9')
                  data = wParam - 0x30;
               else
                  data = wParam - 0x37;

               // Modify data in memory
               addr = cc->addr + (cc->cury * cc->maxcurx) + cc->curx;
               if (cc->editmode == 0)
                  data = (data << 4) | (MappedMemoryReadByte(addr) & 0x0F);
               else
                  data = (MappedMemoryReadByte(addr) & 0xF0) | data;

               MappedMemoryWriteByte(addr, data);

               cc->editmode ^= 1;
               if (cc->editmode == 0)
                  HexEditCtl_KeyDown(cc, VK_RIGHT, 0);                  
               InvalidateRect(cc->hwnd, NULL, FALSE);
               HexEditCtl_SetCaretPos(cc);
            }
         }
         else
         {
            u32 addr;
            u8 data;

            // So long as it's an ANSI character, we're all good

            // Modify data in memory
//            addr = cc->addr + (cc->cury * cc->maxcurx) + cc->curx;
//            MappedMemoryWriteByte(addr, data);
            HexEditCtl_KeyDown(cc, VK_RIGHT, 0);                  
            InvalidateRect(cc->hwnd, NULL, FALSE);
            HexEditCtl_SetCaretPos(cc);
         }
         break;
   }
   return FALSE;
}

//////////////////////////////////////////////////////////////////////////////

LRESULT CALLBACK HexEditCtl(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
   HexEditCtl_struct *cc=(HexEditCtl_struct *)GetWindowLong(hwnd, 0);

   switch(message)
   {
      case WM_NCCREATE:
         return InitHexEditCtl(hwnd, wParam, lParam);
      case WM_NCDESTROY:
         DestroyHexEditCtl((HexEditCtl_struct *)cc);
         break;
      case WM_PAINT:
         return HexEditCtl_OnPaint(cc, wParam, lParam);
      case WM_ERASEBKGND:
         return TRUE;
      case WM_SETFONT:
         return HexEditCtl_SetFont(cc, wParam, lParam);
      case WM_SETFOCUS:
         HexEditCtl_SetFocus(cc);
         break;
      case WM_KILLFOCUS:
         HexEditCtl_KillFocus(cc);
         break;
      case WM_VSCROLL:
         return HexEditCtl_Vscroll(cc, wParam, lParam);
      case WM_GETDLGCODE:
         return DLGC_WANTALLKEYS;
      case WM_KEYDOWN:
         return HexEditCtl_KeyDown(cc, wParam, lParam);
      case HEX_SETADDRESSLIST:
      {
         if (((addrlist_struct *)lParam) != NULL && wParam > 0)
         {
            if (cc->addrlist)
               free(cc->addrlist);
            cc->addrlist = (addrlist_struct *)lParam;
            cc->numaddr = (int)wParam;
            return 0;
         }
         return -1;
      }
      case HEX_GOTOADDRESS:
         // Make sure address is valid         
         if (CalcCurPosFromAddr(cc, lParam))
         {
            cc->addr = (u32)lParam;
            InvalidateRect(cc->hwnd, NULL, FALSE);
            HexEditCtl_SetCaretPos(cc);
         }
         return 0;
      case HEX_GETSELECTED:
         return 0;
      default:
         break;
   }

   return DefWindowProc(hwnd, message, wParam, lParam);
}

//////////////////////////////////////////////////////////////////////////////

void InitHexEdit()
{
   WNDCLASSEX wc;

   wc.cbSize         = sizeof(wc);
   wc.lpszClassName  = "YabauseHexEdit";
   wc.hInstance      = GetModuleHandle(0);
   wc.lpfnWndProc    = HexEditCtl;
   wc.hCursor        = LoadCursor(NULL, IDC_ARROW);
   wc.hIcon          = 0;
   wc.lpszMenuName   = 0;
   wc.hbrBackground  = (HBRUSH)GetSysColorBrush(COLOR_WINDOW);
   wc.style          = 0;
   wc.cbClsExtra     = 0;
   wc.cbWndExtra     = sizeof(HexEditCtl_struct *);
   wc.hIconSm        = 0;

   RegisterClassEx(&wc);
}

//////////////////////////////////////////////////////////////////////////////
