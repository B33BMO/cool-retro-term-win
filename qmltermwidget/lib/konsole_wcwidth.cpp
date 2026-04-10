/* $XFree86: xc/programs/xterm/wcwidth.character,v 1.3 2001/07/29 22:08:16 tsi Exp $ */
/*
 * This is an implementation of wcwidth() and wcswidth() as defined in
 * "The Single UNIX Specification, Version 2, The Open Group, 1997"
 * <http://www.UNIX-systems.org/online.html>
 *
 * Markus Kuhn -- 2001-01-12 -- public domain
 */

#include <QString>

#ifdef HAVE_UTF8PROC
#include <utf8proc.h>
#else
#include <cwchar>
#endif

#include "konsole_wcwidth.h"

#if defined(Q_OS_WIN) || defined(_WIN32)
// MSVC does not provide wcwidth(). Minimal implementation based on
// Markus Kuhn's public-domain wcwidth() (2007-05-26 version).
// Returns 0 for combining/zero-width, 2 for wide, 1 otherwise, -1 for unprintable.
static int mk_wcwidth(wchar_t ucs)
{
    // Non-spacing characters (table from Markus Kuhn's wcwidth.c, abbreviated)
    struct interval { int first, last; };
    static const interval combining[] = {
        {0x0300,0x036F}, {0x0483,0x0489}, {0x0591,0x05BD}, {0x05BF,0x05BF},
        {0x05C1,0x05C2}, {0x05C4,0x05C5}, {0x05C7,0x05C7}, {0x0600,0x0605},
        {0x0610,0x061A}, {0x061C,0x061C}, {0x064B,0x065F}, {0x0670,0x0670},
        {0x06D6,0x06DD}, {0x06DF,0x06E4}, {0x06E7,0x06E8}, {0x06EA,0x06ED},
        {0x070F,0x070F}, {0x0711,0x0711}, {0x0730,0x074A}, {0x07A6,0x07B0},
        {0x07EB,0x07F3}, {0x0816,0x0819}, {0x081B,0x0823}, {0x0825,0x0827},
        {0x0829,0x082D}, {0x0859,0x085B}, {0x08D4,0x0902}, {0x093A,0x093A},
        {0x093C,0x093C}, {0x0941,0x0948}, {0x094D,0x094D}, {0x0951,0x0957},
        {0x0962,0x0963}, {0x0981,0x0981}, {0x09BC,0x09BC}, {0x09C1,0x09C4},
        {0x200B,0x200F}, {0x202A,0x202E}, {0x2060,0x2064}, {0x2066,0x206F},
        {0xFE00,0xFE0F}, {0xFEFF,0xFEFF}, {0xFFF9,0xFFFB}
    };
    // Binary search for combining code points
    if (ucs == 0) return 0;
    if (ucs < 32 || (ucs >= 0x7f && ucs < 0xa0)) return -1;

    int min = 0;
    int max = (int)(sizeof(combining) / sizeof(combining[0])) - 1;
    if (ucs >= combining[0].first && ucs <= combining[max].last) {
        while (max >= min) {
            int mid = (min + max) / 2;
            if (ucs > combining[mid].last) min = mid + 1;
            else if (ucs < combining[mid].first) max = mid - 1;
            else return 0;
        }
    }

    // Wide characters (East Asian Wide/Fullwidth)
    return 1 +
        (ucs >= 0x1100 &&
         (ucs <= 0x115F ||                    // Hangul Jamo
          ucs == 0x2329 || ucs == 0x232A ||
          (ucs >= 0x2E80 && ucs <= 0xA4CF && ucs != 0x303F) || // CJK
          (ucs >= 0xAC00 && ucs <= 0xD7A3) || // Hangul Syllables
          (ucs >= 0xF900 && ucs <= 0xFAFF) || // CJK Compatibility Ideographs
          (ucs >= 0xFE30 && ucs <= 0xFE4F) || // CJK Compatibility Forms
          (ucs >= 0xFF00 && ucs <= 0xFF60) || // Fullwidth Forms
          (ucs >= 0xFFE0 && ucs <= 0xFFE6)));
}
#endif

int konsole_wcwidth(wchar_t ucs)
{
#ifdef HAVE_UTF8PROC
    utf8proc_category_t cat = utf8proc_category( ucs );
    if (cat == UTF8PROC_CATEGORY_CO) {
        // Co: Private use area. libutf8proc makes them zero width, while tmux
        // assumes them to be width 1, and glibc's default width is also 1
        return 1;
    }
    return utf8proc_charwidth( ucs );
#elif defined(Q_OS_WIN) || defined(_WIN32)
    return mk_wcwidth( ucs );
#else
    return wcwidth( ucs );
#endif
}

// single byte char: +1, multi byte char: +2
int string_width( const std::wstring & wstr )
{
    int w = 0;
    for ( size_t i = 0; i < wstr.length(); ++i ) {
        w += konsole_wcwidth( wstr[ i ] );
    }
    return w;
}
