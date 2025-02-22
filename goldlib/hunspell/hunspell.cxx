#include "license.hun"
#include "license.mys"

#include <cstring>
#include <cstdlib>
#include <cstdio>

#include "hunspell.hxx"

#if !defined(_MSC_VER)
    using namespace std;
#endif

Hunspell::Hunspell(const char * affpath, const char * dpath)
{
    encoding = NULL;
    csconv = NULL;
    utfconv = NULL;
    utf8 = 0;
    complexprefixes = 0;

    /* first set up the hash manager */
    pHMgr = new HashMgr(dpath, affpath);

    /* next set up the affix manager */
    /* it needs access to the hash manager lookup methods */
    pAMgr = new AffixMgr(affpath,pHMgr);

    /* get the preferred try string and the dictionary */
    /* encoding from the Affix Manager for that dictionary */
    char * try_string = pAMgr->get_try_string();
    encoding = pAMgr->get_encoding();
    csconv = get_current_cs(encoding);
    langnum = pAMgr->get_langnum();
    utf8 = pAMgr->get_utf8();
    utfconv = pAMgr->get_utf_conv();
    complexprefixes = pAMgr->get_complexprefixes();
    wordbreak = pAMgr->get_breaktable();

    /* and finally set up the suggestion manager */
    pSMgr = new SuggestMgr(try_string, MAXSUGGESTION, pAMgr);
    if (try_string) free(try_string);

    prevroot = NULL;
    prevcompound = 0;
    forbidden_compound = 0;
}

Hunspell::~Hunspell()
{
    if (pSMgr) delete pSMgr;
    if (pAMgr) delete pAMgr;
    if (pHMgr) delete pHMgr;
    pSMgr = NULL;
    pAMgr = NULL;
    pHMgr = NULL;
    csconv= NULL;
    if (encoding) free(encoding);
    encoding = NULL;
}


// make a copy of src at destination while removing all leading
// blanks and removing any trailing periods after recording
// their presence with the abbreviation flag
// also since already going through character by character,
// set the capitalization type
// return the length of the "cleaned" (and UTF-8 encoded) word

int Hunspell::cleanword2(char * dest, const char * src,
                         w_char * dest_utf, int * nc, int * pcaptype, int * pabbrev)
{
    unsigned char * p = (unsigned char *) dest;
    const unsigned char * q = (const unsigned char * ) src;
    int firstcap = 0;

    // first skip over any leading blanks
    while ((*q != '\0') && (*q == ' ')) q++;

    // now strip off any trailing periods (recording their presence)
    *pabbrev = 0;
    int nl = strlen((const char *)q);
    while ((nl > 0) && (*(q+nl-1)=='.'))
    {
        nl--;
        (*pabbrev)++;
    }

    // if no characters are left it can't be capitalized
    if (nl <= 0)
    {
        *pcaptype = NOCAP;
        *p = '\0';
        return 0;
    }

    // now determine the capitalization type of the first nl letters
    int ncap = 0;
    int nneutral = 0;
    *nc = 0;

    if (!utf8)
    {
        while (nl > 0)
        {
            (*nc)++;
            if (csconv[(*q)].ccase) ncap++;
            if (csconv[(*q)].cupper == csconv[(*q)].clower) nneutral++;
            *p++ = *q++;
            nl--;
        }
        // remember to terminate the destination string
        *p = '\0';
        if (ncap)
        {
            firstcap = csconv[(unsigned char)(*dest)].ccase;
        }
    }
    else
    {
        unsigned short idx;
        *nc = u8_u16(dest_utf, MAXWORDLEN, (const char *) q);
        // don't check too long words
        if (*nc >= MAXWORDLEN) return 0;
        *nc -= *pabbrev;
        for (int i = 0; i < *nc; i++)
        {
            idx = (dest_utf[i].h << 8) + dest_utf[i].l;
            if (idx != utfconv[idx].clower) ncap++;
            if (utfconv[idx].cupper == utfconv[idx].clower) nneutral++;
        }
        u16_u8(dest, MAXWORDUTF8LEN, dest_utf, *nc);
        if (ncap)
        {
            idx = (dest_utf[0].h << 8) + dest_utf[0].l;
            firstcap = (idx != utfconv[idx].clower);
        }
    }

    // now finally set the captype
    if (ncap == 0)
    {
        *pcaptype = NOCAP;
    }
    else if ((ncap == 1) && firstcap)
    {
        *pcaptype = INITCAP;
    }
    else if ((ncap == *nc) || ((ncap + nneutral) == *nc))
    {
        *pcaptype = ALLCAP;
    }
    else if ((ncap > 1) && firstcap)
    {
        *pcaptype = HUHINITCAP;
    }
    else
    {
        *pcaptype = HUHCAP;
    }
    return strlen(dest);
}

int Hunspell::cleanword(char * dest, const char * src,
                        int * pcaptype, int * pabbrev)
{
    unsigned char * p = (unsigned char *) dest;
    const unsigned char * q = (const unsigned char * ) src;
    int firstcap = 0;

    // first skip over any leading blanks
    while ((*q != '\0') && (*q == ' ')) q++;

    // now strip off any trailing periods (recording their presence)
    *pabbrev = 0;
    int nl = strlen((const char *)q);
    while ((nl > 0) && (*(q+nl-1)=='.'))
    {
        nl--;
        (*pabbrev)++;
    }

    // if no characters are left it can't be capitalized
    if (nl <= 0)
    {
        *pcaptype = NOCAP;
        *p = '\0';
        return 0;
    }

    // now determine the capitalization type of the first nl letters
    int ncap = 0;
    int nneutral = 0;
    int nc = 0;

    if (!utf8)
    {
        while (nl > 0)
        {
            nc++;
            if (csconv[(*q)].ccase) ncap++;
            if (csconv[(*q)].cupper == csconv[(*q)].clower) nneutral++;
            *p++ = *q++;
            nl--;
        }
        // remember to terminate the destination string
        *p = '\0';
        firstcap = csconv[(unsigned char)(*dest)].ccase;
    }
    else
    {
        unsigned short idx;
        w_char t[MAXWORDLEN];
        nc = u8_u16(t, MAXWORDLEN, src);
        for (int i = 0; i < nc; i++)
        {
            idx = (t[i].h << 8) + t[i].l;
            if (idx != utfconv[idx].clower) ncap++;
            if (utfconv[idx].cupper == utfconv[idx].clower) nneutral++;
        }
        u16_u8(dest, MAXWORDUTF8LEN, t, nc);
        if (ncap)
        {
            idx = (t[0].h << 8) + t[0].l;
            firstcap = (idx != utfconv[idx].clower);
        }
    }

    // now finally set the captype
    if (ncap == 0)
    {
        *pcaptype = NOCAP;
    }
    else if ((ncap == 1) && firstcap)
    {
        *pcaptype = INITCAP;
    }
    else if ((ncap == nc) || ((ncap + nneutral) == nc))
    {
        *pcaptype = ALLCAP;
    }
    else if ((ncap > 1) && firstcap)
    {
        *pcaptype = HUHINITCAP;
    }
    else
    {
        *pcaptype = HUHCAP;
    }
    return strlen(dest);
}


void Hunspell::mkallcap(char * p)
{
    if (utf8)
    {
        w_char u[MAXWORDLEN];
        int nc = u8_u16(u, MAXWORDLEN, p);
        unsigned short idx;
        for (int i = 0; i < nc; i++)
        {
            idx = (u[i].h << 8) + u[i].l;
            if (idx != utfconv[idx].cupper)
            {
                u[i].h = (unsigned char) (utfconv[idx].cupper >> 8);
                u[i].l = (unsigned char) (utfconv[idx].cupper & 0x00FF);
            }
        }
        u16_u8(p, MAXWORDUTF8LEN, u, nc);
    }
    else
    {
        while (*p != '\0')
        {
            *p = csconv[((unsigned char) *p)].cupper;
            p++;
        }
    }
}

int Hunspell::mkallcap2(char * p, w_char * u, int nc)
{
    if (utf8)
    {
        unsigned short idx;
        for (int i = 0; i < nc; i++)
        {
            idx = (u[i].h << 8) + u[i].l;
            if (idx != utfconv[idx].cupper)
            {
                u[i].h = (unsigned char) (utfconv[idx].cupper >> 8);
                u[i].l = (unsigned char) (utfconv[idx].cupper & 0x00FF);
            }
        }
        u16_u8(p, MAXWORDUTF8LEN, u, nc);
        return strlen(p);
    }
    else
    {
        while (*p != '\0')
        {
            *p = csconv[((unsigned char) *p)].cupper;
            p++;
        }
    }
    return nc;
}


void Hunspell::mkallsmall(char * p)
{
    while (*p != '\0')
    {
        *p = csconv[((unsigned char) *p)].clower;
        p++;
    }
}

int Hunspell::mkallsmall2(char * p, w_char * u, int nc)
{
    if (utf8)
    {
        unsigned short idx;
        for (int i = 0; i < nc; i++)
        {
            idx = (u[i].h << 8) + u[i].l;
            if (idx != utfconv[idx].clower)
            {
                u[i].h = (unsigned char) (utfconv[idx].clower >> 8);
                u[i].l = (unsigned char) (utfconv[idx].clower & 0x00FF);
            }
        }
        u16_u8(p, MAXWORDUTF8LEN, u, nc);
        return strlen(p);
    }
    else
    {
        while (*p != '\0')
        {
            *p = csconv[((unsigned char) *p)].clower;
            p++;
        }
    }
    return nc;
}

// convert UTF-8 sharp S codes to latin 1
char * Hunspell::sharps_u8_l1(char * dest, char * source)
{
    char * p = dest;
    *p = *source;
    for (p++, source++; *(source - 1); p++, source++)
    {
        *p = *source;
        if (*source == '�') *--p = '�';
    }
    return dest;
}

// recursive search for right ss-� permutations
hentry * Hunspell::spellsharps(char * base, char * pos, int n, int repnum, char * tmp)
{
    if ((pos = strstr(pos, "ss")) && (n < MAXSHARPS))
    {
        hentry * h;
        *pos = '�';
        *(pos + 1) = '�';
        if ((h = spellsharps(base, pos + 2, n + 1, repnum + 1, tmp))) return h;
        *pos = 's';
        *(pos + 1) = 's';
        if ((h = spellsharps(base, pos + 2, n + 1, repnum, tmp))) return h;
    }
    else if (repnum > 0)
    {
        if (utf8) return check(base);
        return check(sharps_u8_l1(tmp, base));
    }
    return NULL;
}

int Hunspell::is_keepcase(const hentry * rv)
{
    return pAMgr && rv->astr && pAMgr->get_keepcase() &&
           TESTAFF(rv->astr, pAMgr->get_keepcase(), rv->alen);
}

/* check and insert a word to beginning of the suggestion array */
int Hunspell::insert_sug(char ***slst, char * word, int *ns)
{
    if (spell(word))
    {
        if (*ns == MAXSUGGESTION)
        {
            (*ns)--;
            free((*slst)[*ns]);
        }
        for (int k = *ns; k > 0; k--) (*slst)[k] = (*slst)[k - 1];
        (*slst)[0] = mystrdup(word);
        (*ns)++;
    }
    return 0;
}

int Hunspell::spell(const char * word)
{
    struct hentry * rv=NULL;
    // need larger vector. For example, Turkish capital letter I converted a
    // 2-byte UTF-8 character (dotless i) by mkallsmall.
    char cw[MAXWORDUTF8LEN + 4];
    char wspace[MAXWORDUTF8LEN + 4];
    w_char unicw[MAXWORDLEN + 1];
    int nc = strlen(word);
    int wl2=0;
    if (utf8)
    {
        if (nc >= MAXWORDUTF8LEN) return 0;
    }
    else
    {
        if (nc >= MAXWORDLEN) return 0;
    }
    int captype = 0;
    int abbv = 0;
    int wl = cleanword2(cw, word, unicw, &nc, &captype, &abbv);

    if (wl == 0) return 1;

    // allow numbers with dots and commas (but forbid double separators: "..", ",," etc.)
    enum { NBEGIN, NNUM, NSEP };
    int nstate = NBEGIN;
    int i;

    for (i = 0; (i < wl) &&
            (((cw[i] <= '9') && (cw[i] >= '0') && (nstate = NNUM)) ||
             ((nstate == NNUM) && ((cw[i] == ',') ||
                                   (cw[i] == '.') || (cw[i] == '-')) && (nstate = NSEP))); i++);
    if ((i == wl) && (nstate == NNUM)) return 1;

    // LANG_hu section: number(s) + (percent or degree) with suffixes
    if (langnum == LANG_hu)
    {
        if ((nstate == NNUM) && ((cw[i] == '%') || (cw[i] == '�')) && check(cw + i)) return 1;
    }
    // END of LANG_hu section

    switch(captype)
    {
    case HUHCAP:
    case HUHINITCAP:
    case NOCAP:
    {
        rv = check(cw);
        if ((abbv) && !(rv))
        {
            memcpy(wspace,cw,wl);
            *(wspace+wl) = '.';
            *(wspace+wl+1) = '\0';
            rv = check(wspace);
        }
        break;
    }
    case ALLCAP:
    {
        rv = check(cw);
        if (rv) break;
        if (abbv)
        {
            memcpy(wspace,cw,wl);
            *(wspace+wl) = '.';
            *(wspace+wl+1) = '\0';
            rv = check(wspace);
            if (rv) break;
        }
        if (pAMgr && pAMgr->get_checksharps() && strstr(cw, "SS"))
        {
            char tmpword[MAXWORDUTF8LEN];
            wl = mkallsmall2(cw, unicw, nc);
            memcpy(wspace,cw,(wl+1));
            rv = spellsharps(wspace, wspace, 0, 0, tmpword);
            if (!rv)
            {
                wl2 = mkinitcap2(cw, unicw, nc);
                rv = spellsharps(cw, cw, 0, 0, tmpword);
            }
            if ((abbv) && !(rv))
            {
                *(wspace+wl) = '.';
                *(wspace+wl+1) = '\0';
                rv = spellsharps(wspace, wspace, 0, 0, tmpword);
                if (!rv)
                {
                    memcpy(wspace, cw, wl2);
                    *(wspace+wl2) = '.';
                    *(wspace+wl2+1) = '\0';
                    rv = spellsharps(wspace, wspace, 0, 0, tmpword);
                }
            }
            if (rv) break;
        }
    }
    case INITCAP:
    {
        wl = mkallsmall2(cw, unicw, nc);
        memcpy(wspace,cw,(wl+1));
        rv = check(wspace);
        if (!rv || (is_keepcase(rv) && !((captype == INITCAP) &&
                                         // if CHECKSHARPS: KEEPCASE words with � are allowed
                                         // in INITCAP form, too.
                                         pAMgr->get_checksharps() && ((utf8 && strstr(wspace, "ß")) ||
                                                 (!utf8 && strchr(wspace, '�'))))))
        {
            wl2 = mkinitcap2(cw, unicw, nc);
            rv = check(cw);
            if (rv && (captype == ALLCAP) && is_keepcase(rv)) rv = NULL;
        }
        if (abbv && !rv)
        {
            *(wspace+wl) = '.';
            *(wspace+wl+1) = '\0';
            rv = check(wspace);
            if (!rv || is_keepcase(rv))
            {
                memcpy(wspace, cw, wl2);
                *(wspace+wl2) = '.';
                *(wspace+wl2+1) = '\0';
                rv = check(wspace);
                if (rv && ((captype == ALLCAP) && is_keepcase(rv))) rv = NULL;
            }
        }
        break;
    }
    }

    if (rv) return 1;

    // recursive breaking at break points (not good for morphological analysis)
    if (wordbreak)
    {
        char * s;
        char r;
        for (int i = 0; i < pAMgr->get_numbreak(); i++)
        {
            if ((s=(char *) strstr(cw, wordbreak[i])))
            {
                r = *s;
                *s = '\0';
                // examine 2 sides of the break point
                if (spell(cw) && spell(s + strlen(wordbreak[i])))
                {
                    *s = r;
                    return 1;
                }
                *s = r;
            }
        }
    }

    // LANG_hu: compoundings with dashes and n-dashes XXX deprecated!
    if (langnum == LANG_hu)
    {
        int n;
        // compound word with dash (HU) I18n
        char * dash;
        int result = 0;
        // n-dash
        if (!wordbreak && (dash=(char *) strstr(cw,"–")))
        {
            *dash = '\0';
            // examine 2 sides of the dash
            if (spell(cw) && spell(dash + 3))
            {
                *dash = '�';
                return 1;
            }
            *dash = '�';
        }
        if ((dash=(char *) strchr(cw,'-')))
        {
            *dash='\0';
            // examine 2 sides of the dash
            if (dash[1] == '\0')   // base word ending with dash
            {
                if (spell(cw)) return 1;
            }
            else
            {
                // first word ending with dash: word-
                char r2 = *(dash + 1);
                dash[0]='-';
                dash[1]='\0';
                result = spell(cw);
                dash[1] = r2;
                dash[0]='\0';
                if (result && spell(dash+1) && ((strlen(dash+1) > 1) || (dash[1] == 'e') ||
                                                ((dash[1] > '0') && (dash[1] < '9')))) return 1;
            }
            // affixed number in correct word
            if (result && (dash > cw) && (((*(dash-1)<='9') && (*(dash-1)>='0')) || (*(dash-1)>='.')))
            {
                *dash='-';
                n = 1;
                if (*(dash - n) == '.') n++;
                // search first not a number character to left from dash
                while (((dash - n)>=cw) && ((*(dash - n)=='0') || (n < 3)) && (n < 6))
                {
                    n++;
                }
                if ((dash - n) < cw) n--;
                // numbers: deprecated
                for(; n >= 1; n--)
                {
                    if ((*(dash - n) >= '0') && (*(dash - n) <= '9') && check(dash - n)) return 1;
                }
            }
        }
    }
    return 0;
}

struct hentry * Hunspell::check(const char * w)
{
    struct hentry * he = NULL;
    int len;
    char w2[MAXWORDUTF8LEN];
    const char * word = w;

    // word reversing wrapper for complex prefixes
    if (complexprefixes)
    {
        strcpy(w2, w);
        if (utf8) reverseword_utf(w2);
        else reverseword(w2);
        word = w2;
    }

    forbidden_compound = 0; // XXX LANG_hu class variable for suggestions (not threadsafe)
    prevcompound = 0;       // compounding information for Hunspell's pipe interface (not threadsafe)
    prevroot = NULL;        // root information for Hunspell's pipe interface (not threadsafe)

    // look word in hash table
    if (pHMgr) he = pHMgr->lookup(word);

    // check forbidden and onlyincompound words
    if ((he) && (he->astr) && (pAMgr) && TESTAFF(he->astr, pAMgr->get_forbiddenword(), he->alen))
    {
        // LANG_hu section: set dash information for suggestions
        if (langnum == LANG_hu)
        {
            forbidden_compound = 1;
            if (pAMgr->get_compoundflag() &&
                    TESTAFF(he->astr, pAMgr->get_compoundflag(), he->alen))
            {
                forbidden_compound = 2;
            }
        }
        return NULL;
    }

    // he = next not pseudoroot and not onlyincompound homonym or NULL
    while (he && (he->astr) &&
            ((pAMgr->get_pseudoroot() && TESTAFF(he->astr, pAMgr->get_pseudoroot(), he->alen)) ||
             (pAMgr->get_onlyincompound() && TESTAFF(he->astr, pAMgr->get_onlyincompound(), he->alen))
            )) he = he->next_homonym;

    // check with affixes
    if (!he && pAMgr)
    {
        // try stripping off affixes */
        len = strlen(word);
        he = pAMgr->affix_check(word, len, 0);

        // check compound restriction
        if (he && he->astr && pAMgr->get_onlyincompound() &&
                TESTAFF(he->astr, pAMgr->get_onlyincompound(), he->alen)) he = NULL;

        // try check compound word
        if (he)
        {
            if ((he->astr) && (pAMgr) && TESTAFF(he->astr, pAMgr->get_forbiddenword(), he->alen))
            {
                forbidden_compound = 1; // LANG_hu
                return NULL;
            }
            prevroot = he->word;
        }
        else if (pAMgr->get_compound())
        {
            he = pAMgr->compound_check(word, len,
                                       0,0,100,0,NULL,0,NULL,NULL,0);
            // LANG_hu section: `moving rule' with last dash
            if ((!he) && (langnum == LANG_hu) && (word[len-1]=='-'))
            {
                char * dup = mystrdup(word);
                dup[len-1] = '\0';
                he = pAMgr->compound_check(dup, len-1,
                                           -5,0,100,0,NULL,1,NULL,NULL,0);
                free(dup);
            }
            // end of LANG speficic region
            if (he)
            {
                prevroot = he->word;
                prevcompound = 1;
            }
        }

    }

    return he;
}

int Hunspell::suggest(char*** slst, const char * word)
{
    char cw[MAXWORDUTF8LEN + 4];
    char wspace[MAXWORDUTF8LEN + 4];
    if (! pSMgr) return 0;
    w_char unicw[MAXWORDLEN + 1];
    int nc = strlen(word);
    if (utf8)
    {
        if (nc >= MAXWORDUTF8LEN) return 0;
    }
    else
    {
        if (nc >= MAXWORDLEN) return 0;
    }
    int captype = 0;
    int abbv = 0;
    int wl = cleanword2(cw, word, unicw, &nc, &captype, &abbv);
    if (wl == 0) return 0;
    int ns = 0;
    *slst = NULL;
    int capwords = 0;

    switch(captype)
    {
    case NOCAP:
    {
        ns = pSMgr->suggest(slst, cw, ns);
        break;
    }

    case INITCAP:
    {
        capwords = 1;
        ns = pSMgr->suggest(slst, cw, ns);
        if (ns == -1) break;
        memcpy(wspace,cw,(wl+1));
        mkallsmall2(wspace, unicw, nc);
        ns = pSMgr->suggest(slst, wspace, ns);
        break;
    }
    case HUHINITCAP:
        capwords = 1;
    case HUHCAP:
    {
        ns = pSMgr->suggest(slst, cw, ns);
        if (ns != -1)
        {
            int prevns;
            if (captype == HUHINITCAP)
            {
                // TheOpenOffice.org -> The OpenOffice.org
                memcpy(wspace,cw,(wl+1));
                mkinitsmall2(wspace, unicw, nc);
                ns = pSMgr->suggest(slst, wspace, ns);
            }
            memcpy(wspace,cw,(wl+1));
            mkallsmall2(wspace, unicw, nc);
            insert_sug(slst, wspace, &ns);
            prevns = ns;
            ns = pSMgr->suggest(slst, wspace, ns);
            if (captype == HUHINITCAP)
            {
                mkinitcap2(wspace, unicw, nc);
                insert_sug(slst, wspace, &ns);
                ns = pSMgr->suggest(slst, wspace, ns);
            }
            // aNew -> "a New" (instead of "a new")
            for (int j = prevns; j < ns; j++)
            {
                char * space;
                if ( (space = strchr((*slst)[j],' ')) )
                {
                    int slen = strlen(space + 1);
                    // different case after space (need capitalisation)
                    if ((slen < wl) && strcmp(cw + wl - slen, space + 1))
                    {
                        w_char w[MAXWORDLEN + 1];
                        int wc = 0;
                        char * r = (*slst)[j];
                        if (utf8) wc = u8_u16(w, MAXWORDLEN, space + 1);
                        mkinitcap2(space + 1, w, wc);
                        // set as first suggestion
                        for (int k = j; k > 0; k--) (*slst)[k] = (*slst)[k - 1];
                        (*slst)[0] = r;
                    }
                }
            }
        }
        break;
    }

    case ALLCAP:
    {
        memcpy(wspace, cw, (wl+1));
        mkallsmall2(wspace, unicw, nc);
        ns = pSMgr->suggest(slst, wspace, ns);
        if (ns == -1) break;
        if (pAMgr && pAMgr->get_keepcase()) insert_sug(slst, wspace, &ns);
        mkinitcap2(wspace, unicw, nc);
        ns = pSMgr->suggest(slst, wspace, ns);
        for (int j=0; j < ns; j++)
        {
            mkallcap((*slst)[j]);
            if (pAMgr && pAMgr->get_checksharps())
            {
                char * pos;
                if (utf8)
                {
                    while ( (pos = strstr((*slst)[j], "ß")) )
                    {
                        *pos = 'S';
                        *(pos+1) = 'S';
                    }
                }
                else
                {
                    while ( (pos = strchr((*slst)[j], '�')) )
                    {
                        (*slst)[j] = (char *) realloc((*slst)[j], strlen((*slst)[j]) + 2);
                        mystrrep((*slst)[j], "�", "SS");
                    }
                }
            }
        }
        break;
    }
    }

    // LANG_hu section: replace '-' with ' ' in Hungarian
    if ((langnum == LANG_hu) && (forbidden_compound == 2))
    {
        for (int j=0; j < ns; j++)
        {
            char * pos = strchr((*slst)[j],'-');
            if (pos) *pos = ' ';
        }
    }
    // END OF LANG_hu section

    // try ngram approach since found nothing
    if ((ns == 0) && pAMgr && (pAMgr->get_maxngramsugs() != 0))
    {
        switch(captype)
        {
        case NOCAP:
        {
            ns = pSMgr->ngsuggest(*slst, cw, pHMgr);
            break;
        }
        case HUHCAP:
        {
            memcpy(wspace,cw,(wl+1));
            mkallsmall2(wspace, unicw, nc);
            ns = pSMgr->ngsuggest(*slst, wspace, pHMgr);
            break;
        }
        case INITCAP:
        {
            capwords = 1;
            memcpy(wspace,cw,(wl+1));
            mkallsmall2(wspace, unicw, nc);
            ns = pSMgr->ngsuggest(*slst, wspace, pHMgr);
            break;
        }
        case ALLCAP:
        {
            memcpy(wspace,cw,(wl+1));
            mkallsmall2(wspace, unicw, nc);
            ns = pSMgr->ngsuggest(*slst, wspace, pHMgr);
            for (int j=0; j < ns; j++)
                mkallcap((*slst)[j]);
            break;
        }
        }
    }

    // word reversing wrapper for complex prefixes
    if (complexprefixes)
    {
        for (int j = 0; j < ns; j++)
        {
            if (utf8) reverseword_utf((*slst)[j]);
            else reverseword((*slst)[j]);
        }
    }

    // capitalize
    if (capwords) for (int j=0; j < ns; j++)
        {
            mkinitcap((*slst)[j]);
        }

    // expand suggestions with dot(s)
    if (abbv && pAMgr && pAMgr->get_sugswithdots())
    {
        for (int j = 0; j < ns; j++)
        {
            (*slst)[j] = (char *) realloc((*slst)[j], strlen((*slst)[j]) + 1 + abbv);
            strcat((*slst)[j], word + strlen(word) - abbv);
        }
    }

    // suggest keepcase
    if (pAMgr->get_keepcase())
    {
        switch (captype)
        {
        case INITCAP:
        case ALLCAP:
        {
            int l = 0;
            for (int j=0; j < ns; j++)
            {
                if (!spell((*slst)[j]))
                {
                    char s[MAXSWUTF8L];
                    w_char w[MAXSWL];
                    int len;
                    if (utf8)
                    {
                        len = u8_u16(w, MAXSWL, (*slst)[j]);
                    }
                    else
                    {
                        strcpy(s, (*slst)[j]);
                        len = strlen(s);
                    }
                    mkallsmall2(s, w, len);
                    free((*slst)[j]);
                    if (spell(s))
                    {
                        (*slst)[l] = mystrdup(s);
                        l++;
                    }
                    else
                    {
                        mkinitcap2(s, w, len);
                        if (spell(s))
                        {
                            (*slst)[l] = mystrdup(s);
                            l++;
                        }
                    }
                }
                else
                {
                    (*slst)[l] = (*slst)[j];
                    l++;
                }
            }
            ns = l;
        }
        }
    }

    // remove duplications
    int l = 0;
    for (int j = 0; j < ns; j++)
    {
        (*slst)[l] = (*slst)[j];
        for (int k = 0; k < l; k++)
        {
            if (strcmp((*slst)[k], (*slst)[j]) == 0)
            {
                free((*slst)[j]);
                l--;
            }
        }
        l++;
    }
    return l;
}

// XXX need UTF-8 support
int Hunspell::suggest_auto(char*** slst, const char * word)
{
    char cw[MAXWORDUTF8LEN + 4];
    char wspace[MAXWORDUTF8LEN + 4];
    if (! pSMgr) return 0;
    int wl = strlen(word);
    if (utf8)
    {
        if (wl >= MAXWORDUTF8LEN) return 0;
    }
    else
    {
        if (wl >= MAXWORDLEN) return 0;
    }
    int captype = 0;
    int abbv = 0;
    wl = cleanword(cw, word, &captype, &abbv);
    if (wl == 0) return 0;
    int ns = 0;
    *slst = NULL; // HU, nsug in pSMgr->suggest

    switch(captype)
    {
    case NOCAP:
    {
        ns = pSMgr->suggest_auto(slst, cw, ns);
        if (ns>0) break;
        break;
    }

    case INITCAP:
    {
        memcpy(wspace,cw,(wl+1));
        mkallsmall(wspace);
        ns = pSMgr->suggest_auto(slst, wspace, ns);
        for (int j=0; j < ns; j++)
            mkinitcap((*slst)[j]);
        ns = pSMgr->suggest_auto(slst, cw, ns);
        break;

    }

    case HUHCAP:
    {
        ns = pSMgr->suggest_auto(slst, cw, ns);
        if (ns == 0)
        {
            memcpy(wspace,cw,(wl+1));
            mkallsmall(wspace);
            ns = pSMgr->suggest_auto(slst, wspace, ns);
        }
        break;
    }

    case ALLCAP:
    {
        memcpy(wspace,cw,(wl+1));
        mkallsmall(wspace);
        ns = pSMgr->suggest_auto(slst, wspace, ns);

        mkinitcap(wspace);
        ns = pSMgr->suggest_auto(slst, wspace, ns);

        for (int j=0; j < ns; j++)
            mkallcap((*slst)[j]);
        break;
    }
    }

    // word reversing wrapper for complex prefixes
    if (complexprefixes)
    {
        for (int j = 0; j < ns; j++)
        {
            if (utf8) reverseword_utf((*slst)[j]);
            else reverseword((*slst)[j]);
        }
    }

    // expand suggestions with dot(s)
    if (abbv && pAMgr && pAMgr->get_sugswithdots())
    {
        for (int j = 0; j < ns; j++)
        {
            (*slst)[j] = (char *) realloc((*slst)[j], strlen((*slst)[j]) + 1 + abbv);
            strcat((*slst)[j], word + strlen(word) - abbv);
        }
    }

    // replace '-' with ' '
    if (forbidden_compound == 2)
    {
        for (int j=0; j < ns; j++)
        {
            char * pos = strchr((*slst)[j],'-');
            if (pos) *pos = ' ';
        }
    }
    return ns;
}

// XXX need UTF-8 support
int Hunspell::stem(char*** slst, const char * word)
{
    char cw[MAXWORDUTF8LEN + 4];
    char wspace[MAXWORDUTF8LEN + 4];
    if (! pSMgr) return 0;
    int wl = strlen(word);
    if (utf8)
    {
        if (wl >= MAXWORDUTF8LEN) return 0;
    }
    else
    {
        if (wl >= MAXWORDLEN) return 0;
    }
    int captype = 0;
    int abbv = 0;
    wl = cleanword(cw, word, &captype, &abbv);
    if (wl == 0) return 0;

    int ns = 0;

    *slst = NULL; // HU, nsug in pSMgr->suggest

    switch(captype)
    {
    case HUHCAP:
    case NOCAP:
    {
        ns = pSMgr->suggest_stems(slst, cw, ns);

        if ((abbv) && (ns == 0))
        {
            memcpy(wspace,cw,wl);
            *(wspace+wl) = '.';
            *(wspace+wl+1) = '\0';
            ns = pSMgr->suggest_stems(slst, wspace, ns);
        }

        break;
    }

    case INITCAP:
    {

        ns = pSMgr->suggest_stems(slst, cw, ns);

        if (ns == 0)
        {
            memcpy(wspace,cw,(wl+1));
            mkallsmall(wspace);
            ns = pSMgr->suggest_stems(slst, wspace, ns);

        }

        if ((abbv) && (ns == 0))
        {
            memcpy(wspace,cw,wl);
            mkallsmall(wspace);
            *(wspace+wl) = '.';
            *(wspace+wl+1) = '\0';
            ns = pSMgr->suggest_stems(slst, wspace, ns);
        }

        break;

    }

    case ALLCAP:
    {
        ns = pSMgr->suggest_stems(slst, cw, ns);
        if (ns != 0) break;

        memcpy(wspace,cw,(wl+1));
        mkallsmall(wspace);
        ns = pSMgr->suggest_stems(slst, wspace, ns);

        if (ns == 0)
        {
            mkinitcap(wspace);
            ns = pSMgr->suggest_stems(slst, wspace, ns);
        }

        if ((abbv) && (ns == 0))
        {
            memcpy(wspace,cw,wl);
            mkallsmall(wspace);
            *(wspace+wl) = '.';
            *(wspace+wl+1) = '\0';
            ns = pSMgr->suggest_stems(slst, wspace, ns);
        }


        break;
    }
    }

    return ns;
}

int Hunspell::suggest_pos_stems(char*** slst, const char * word)
{
    char cw[MAXWORDUTF8LEN + 4];
    char wspace[MAXWORDUTF8LEN + 4];
    if (! pSMgr) return 0;
    int wl = strlen(word);
    if (utf8)
    {
        if (wl >= MAXWORDUTF8LEN) return 0;
    }
    else
    {
        if (wl >= MAXWORDLEN) return 0;
    }
    int captype = 0;
    int abbv = 0;
    wl = cleanword(cw, word, &captype, &abbv);
    if (wl == 0) return 0;

    int ns = 0; // ns=0 = normalized input

    *slst = NULL; // HU, nsug in pSMgr->suggest

    switch(captype)
    {
    case HUHCAP:
    case NOCAP:
    {
        ns = pSMgr->suggest_pos_stems(slst, cw, ns);

        if ((abbv) && (ns == 0))
        {
            memcpy(wspace,cw,wl);
            *(wspace+wl) = '.';
            *(wspace+wl+1) = '\0';
            ns = pSMgr->suggest_pos_stems(slst, wspace, ns);
        }

        break;
    }

    case INITCAP:
    {

        ns = pSMgr->suggest_pos_stems(slst, cw, ns);

        if (ns == 0 || ((*slst)[0][0] == '#'))
        {
            memcpy(wspace,cw,(wl+1));
            mkallsmall(wspace);
            ns = pSMgr->suggest_pos_stems(slst, wspace, ns);
        }

        break;

    }

    case ALLCAP:
    {
        ns = pSMgr->suggest_pos_stems(slst, cw, ns);
        if (ns != 0) break;

        memcpy(wspace,cw,(wl+1));
        mkallsmall(wspace);
        ns = pSMgr->suggest_pos_stems(slst, wspace, ns);

        if (ns == 0)
        {
            mkinitcap(wspace);
            ns = pSMgr->suggest_pos_stems(slst, wspace, ns);
        }
        break;
    }
    }

    return ns;
}

char * Hunspell::get_dic_encoding()
{
    return encoding;
}

const char * Hunspell::get_wordchars()
{
    return pAMgr->get_wordchars();
}

unsigned short * Hunspell::get_wordchars_utf16(int * len)
{
    return pAMgr->get_wordchars_utf16(len);
}

char * Hunspell::get_prevroot()
{
    return prevroot; // XXX not stateless, not for OOo
}

int Hunspell::get_prevcompound()
{
    return prevcompound; // XXX not stateless, not for OOo
}

int Hunspell::get_forbidden_compound()
{
    return forbidden_compound; // XXX not stateless, not for OOo
}

void Hunspell::mkinitcap(char * p)
{
    if (!utf8)
    {
        if (*p != '\0') *p = csconv[((unsigned char)*p)].cupper;
    }
    else
    {
        int len;
        w_char u[MAXWORDLEN];
        len = u8_u16(u, MAXWORDLEN, p);
        unsigned short i = utfconv[(u[0].h << 8) + u[0].l].cupper;
        u[0].h = (unsigned char) (i >> 8);
        u[0].l = (unsigned char) (i & 0x00FF);
        u16_u8(p, MAXWORDUTF8LEN, u, len);
    }
}

int Hunspell::mkinitcap2(char * p, w_char * u, int nc)
{
    if (!utf8)
    {
        if (*p != '\0') *p = csconv[((unsigned char)*p)].cupper;
    }
    else if (nc > 0)
    {
        unsigned short i = utfconv[(u[0].h << 8) + u[0].l].cupper;
        u[0].h = (unsigned char) (i >> 8);
        u[0].l = (unsigned char) (i & 0x00FF);
        u16_u8(p, MAXWORDUTF8LEN, u, nc);
        return strlen(p);
    }
    return nc;
}

int Hunspell::mkinitsmall2(char * p, w_char * u, int nc)
{
    if (!utf8)
    {
        if (*p != '\0') *p = csconv[((unsigned char)*p)].clower;
    }
    else if (nc > 0)
    {
        unsigned short i = utfconv[(u[0].h << 8) + u[0].l].clower;
        u[0].h = (unsigned char) (i >> 8);
        u[0].l = (unsigned char) (i & 0x00FF);
        u16_u8(p, MAXWORDUTF8LEN, u, nc);
        return strlen(p);
    }
    return nc;
}

struct cs_info * Hunspell::get_csconv()
{
    return csconv;
}

struct unicode_info2 * Hunspell::get_utf_conv()
{
    return utfconv;
}

int Hunspell::put_word(const char * word)
{
    if (pHMgr)
    {
        return pHMgr->put_word(word, strlen(word), NULL);
    }
    return 0;
}

int Hunspell::put_word_suffix(const char * word, const char * suffix)
{
    if (pHMgr)
    {
        return pHMgr->put_word(word, strlen(word), (char *) suffix);
    }
    return 0;
}

int Hunspell::put_word_pattern(const char * word, const char * pattern)
{
    if (pHMgr)
    {
        return pHMgr->put_word_pattern(word, strlen(word), pattern);
    }
    return 0;
}

const char * Hunspell::get_version()
{
    return pAMgr->get_version();
}

// XXX need UTF-8 support
char * Hunspell::morph(const char * word)
{
    char cw[MAXWORDUTF8LEN + 4];
    char wspace[MAXWORDUTF8LEN + 4];
    if (! pSMgr) return 0;
    int wl = strlen(word);
    if (utf8)
    {
        if (wl >= MAXWORDUTF8LEN) return 0;
    }
    else
    {
        if (wl >= MAXWORDLEN) return 0;
    }
    int captype = 0;
    int abbv = 0;
    wl = cleanword(cw, word, &captype, &abbv);
    if (wl == 0)
    {
        if (abbv)
        {
            for (wl = 0; wl < abbv; wl++) cw[wl] = '.';
            cw[wl] = '\0';
            abbv = 0;
        }
        else return 0;
    }

    char result[MAXLNLEN];
    char * st = NULL;

    *result = '\0';

    int n = 0;
    int n2 = 0;
    int n3 = 0;

    // test numbers
    // LANG_hu section: set dash information for suggestions
    if (langnum == LANG_hu)
    {
        while ((n < wl) &&
                (((cw[n] <= '9') && (cw[n] >= '0')) || (((cw[n] == '.') || (cw[n] == ',')) && (n > 0))))
        {
            n++;
            if ((cw[n] == '.') || (cw[n] == ','))
            {
                if (((n2 == 0) && (n > 3)) ||
                        ((n2 > 0) && ((cw[n-1] == '.') || (cw[n-1] == ',')))) break;
                n2++;
                n3 = n;
            }
        }

        if ((n == wl) && (n3 > 0) && (n - n3 > 3)) return NULL;
        if ((n == wl) || ((n>0) && ((cw[n]=='%') || (cw[n]=='�')) && check(cw+n)))
        {
            strcat(result, cw);
            result[n - 1] = '\0';
            if (n == wl)
            {
                st = pSMgr->suggest_morph(cw + n - 1);
                if (st)
                {
                    strcat(result, st);
                    free(st);
                }
            }
            else
            {
                char sign = cw[n];
                cw[n] = '\0';
                st = pSMgr->suggest_morph(cw + n - 1);
                if (st)
                {
                    strcat(result, st);
                    free(st);
                }
                strcat(result, "+"); // XXX SPEC. MORPHCODE
                cw[n] = sign;
                st = pSMgr->suggest_morph(cw + n);
                if (st)
                {
                    strcat(result, st);
                    free(st);
                }
            }
            return mystrdup(result);
        }
    }
    // END OF LANG_hu section

    switch(captype)
    {
    case NOCAP:
    {
        st = pSMgr->suggest_morph(cw);
        if (st)
        {
            strcat(result, st);
            free(st);
        }
        if (abbv)
        {
            memcpy(wspace,cw,wl);
            *(wspace+wl) = '.';
            *(wspace+wl+1) = '\0';
            st = pSMgr->suggest_morph(wspace);
            if (st)
            {
                if (*result) strcat(result, "\n");
                strcat(result, st);
                free(st);
            }
        }
        break;
    }
    case INITCAP:
    {
        memcpy(wspace,cw,(wl+1));
        mkallsmall(wspace);
        st = pSMgr->suggest_morph(wspace);
        if (st)
        {
            strcat(result, st);
            free(st);
        }
        st = pSMgr->suggest_morph(cw);
        if (st)
        {
            if (*result) strcat(result, "\n");
            strcat(result, st);
            free(st);
        }
        if (abbv)
        {
            memcpy(wspace,cw,wl);
            *(wspace+wl) = '.';
            *(wspace+wl+1) = '\0';
            mkallsmall(wspace);
            st = pSMgr->suggest_morph(wspace);
            if (st)
            {
                if (*result) strcat(result, "\n");
                strcat(result, st);
                free(st);
            }
            mkinitcap(wspace);
            st = pSMgr->suggest_morph(wspace);
            if (st)
            {
                if (*result) strcat(result, "\n");
                strcat(result, st);
                free(st);
            }
        }
        break;
    }
    case HUHCAP:
    {
        st = pSMgr->suggest_morph(cw);
        if (st)
        {
            strcat(result, st);
            free(st);
        }
#if 0
        memcpy(wspace,cw,(wl+1));
        mkallsmall(wspace);
        st = pSMgr->suggest_morph(wspace);
        if (st)
        {
            if (*result) strcat(result, "\n");
            strcat(result, st);
            free(st);
        }
#endif
        break;
    }
    case ALLCAP:
    {
        memcpy(wspace,cw,(wl+1));
        st = pSMgr->suggest_morph(wspace);
        if (st)
        {
            strcat(result, st);
            free(st);
        }
        mkallsmall(wspace);
        st = pSMgr->suggest_morph(wspace);
        if (st)
        {
            if (*result) strcat(result, "\n");
            strcat(result, st);
            free(st);
        }
        mkinitcap(wspace);
        st = pSMgr->suggest_morph(wspace);
        if (st)
        {
            if (*result) strcat(result, "\n");
            strcat(result, st);
            free(st);
        }
        if (abbv)
        {
            memcpy(wspace,cw,(wl+1));
            *(wspace+wl) = '.';
            *(wspace+wl+1) = '\0';
            if (*result) strcat(result, "\n");
            st = pSMgr->suggest_morph(wspace);
            if (st)
            {
                strcat(result, st);
                free(st);
            }
            mkallsmall(wspace);
            st = pSMgr->suggest_morph(wspace);
            if (st)
            {
                if (*result) strcat(result, "\n");
                strcat(result, st);
                free(st);
            }
            mkinitcap(wspace);
            st = pSMgr->suggest_morph(wspace);
            if (st)
            {
                if (*result) strcat(result, "\n");
                strcat(result, st);
                free(st);
            }
        }
        break;
    }
    }

    if (result && (*result))
    {
        // word reversing wrapper for complex prefixes
        if (complexprefixes)
        {
            if (utf8) reverseword_utf(result);
            else reverseword(result);
        }
        return mystrdup(result);
    }

    // compound word with dash (HU) I18n
    char * dash;
    int nresult = 0;
    // LANG_hu section: set dash information for suggestions
    if ((langnum == LANG_hu) && (dash=(char *) strchr(cw,'-')))
    {
        *dash='\0';
        // examine 2 sides of the dash
        if (dash[1] == '\0')   // base word ending with dash
        {
            if (spell(cw)) return pSMgr->suggest_morph(cw);
        }
        else if ((dash[1] == 'e') && (dash[2] == '\0'))     // XXX (HU) -e hat.
        {
            if (spell(cw) && (spell("-e")))
            {
                st = pSMgr->suggest_morph(cw);
                if (st)
                {
                    strcat(result, st);
                    free(st);
                }
                strcat(result,"+"); // XXX spec. separator in MORPHCODE
                st = pSMgr->suggest_morph("-e");
                if (st)
                {
                    strcat(result, st);
                    free(st);
                }
                return mystrdup(result);
            }
        }
        else
        {
            // first word ending with dash: word- XXX ???
            char r2 = *(dash + 1);
            dash[0]='-';
            dash[1]='\0';
            nresult = spell(cw);
            dash[1] = r2;
            dash[0]='\0';
            if (nresult && spell(dash+1) && ((strlen(dash+1) > 1) ||
                                             ((dash[1] > '0') && (dash[1] < '9'))))
            {
                st = morph(cw);
                if (st)
                {
                    strcat(result, st);
                    free(st);
                    strcat(result,"+"); // XXX spec. separator in MORPHCODE
                }
                st = morph(dash+1);
                if (st)
                {
                    strcat(result, st);
                    free(st);
                }
                return mystrdup(result);
            }
        }
        // affixed number in correct word
        if (nresult && (dash > cw) && (((*(dash-1)<='9') &&
                                        (*(dash-1)>='0')) || (*(dash-1)=='.')))
        {
            *dash='-';
            n = 1;
            if (*(dash - n) == '.') n++;
            // search first not a number character to left from dash
            while (((dash - n)>=cw) && ((*(dash - n)=='0') || (n < 3)) && (n < 6))
            {
                n++;
            }
            if ((dash - n) < cw) n--;
            // numbers: valami1000000-hoz
            // examine 100000-hoz, 10000-hoz 1000-hoz, 10-hoz,
            // 56-hoz, 6-hoz
            for(; n >= 1; n--)
            {
                if ((*(dash - n) >= '0') && (*(dash - n) <= '9') && check(dash - n))
                {
                    strcat(result, cw);
                    result[dash - cw - n] = '\0';
                    st = pSMgr->suggest_morph(dash - n);
                    if (st)
                    {
                        strcat(result, st);
                        free(st);
                    }
                    return mystrdup(result);
                }
            }
        }
    }
    return NULL;
}

// XXX need UTF-8 support
char * Hunspell::morph_with_correction(const char * word)
{
    char cw[MAXWORDUTF8LEN + 4];
    char wspace[MAXWORDUTF8LEN + 4];
    if (! pSMgr) return 0;
    int wl = strlen(word);
    if (utf8)
    {
        if (wl >= MAXWORDUTF8LEN) return 0;
    }
    else
    {
        if (wl >= MAXWORDLEN) return 0;
    }
    int captype = 0;
    int abbv = 0;
    wl = cleanword(cw, word, &captype, &abbv);
    if (wl == 0) return 0;

    char result[MAXLNLEN];
    char * st = NULL;

    *result = '\0';


    switch(captype)
    {
    case NOCAP:
    {
        st = pSMgr->suggest_morph_for_spelling_error(cw);
        if (st)
        {
            strcat(result, st);
            free(st);
        }
        if (abbv)
        {
            memcpy(wspace,cw,wl);
            *(wspace+wl) = '.';
            *(wspace+wl+1) = '\0';
            st = pSMgr->suggest_morph_for_spelling_error(wspace);
            if (st)
            {
                if (*result) strcat(result, "\n");
                strcat(result, st);
                free(st);
            }
        }
        break;
    }
    case INITCAP:
    {
        memcpy(wspace,cw,(wl+1));
        mkallsmall(wspace);
        st = pSMgr->suggest_morph_for_spelling_error(wspace);
        if (st)
        {
            strcat(result, st);
            free(st);
        }
        st = pSMgr->suggest_morph_for_spelling_error(cw);
        if (st)
        {
            if (*result) strcat(result, "\n");
            strcat(result, st);
            free(st);
        }
        if (abbv)
        {
            memcpy(wspace,cw,wl);
            *(wspace+wl) = '.';
            *(wspace+wl+1) = '\0';
            mkallsmall(wspace);
            st = pSMgr->suggest_morph_for_spelling_error(wspace);
            if (st)
            {
                if (*result) strcat(result, "\n");
                strcat(result, st);
                free(st);
            }
            mkinitcap(wspace);
            st = pSMgr->suggest_morph_for_spelling_error(wspace);
            if (st)
            {
                if (*result) strcat(result, "\n");
                strcat(result, st);
                free(st);
            }
        }
        break;
    }
    case HUHCAP:
    {
        st = pSMgr->suggest_morph_for_spelling_error(cw);
        if (st)
        {
            strcat(result, st);
            free(st);
        }
        memcpy(wspace,cw,(wl+1));
        mkallsmall(wspace);
        st = pSMgr->suggest_morph_for_spelling_error(wspace);
        if (st)
        {
            if (*result) strcat(result, "\n");
            strcat(result, st);
            free(st);
        }
        break;
    }
    case ALLCAP:
    {
        memcpy(wspace,cw,(wl+1));
        st = pSMgr->suggest_morph_for_spelling_error(wspace);
        if (st)
        {
            strcat(result, st);
            free(st);
        }
        mkallsmall(wspace);
        st = pSMgr->suggest_morph_for_spelling_error(wspace);
        if (st)
        {
            if (*result) strcat(result, "\n");
            strcat(result, st);
            free(st);
        }
        mkinitcap(wspace);
        st = pSMgr->suggest_morph_for_spelling_error(wspace);
        if (st)
        {
            if (*result) strcat(result, "\n");
            strcat(result, st);
            free(st);
        }
        if (abbv)
        {
            memcpy(wspace,cw,(wl+1));
            *(wspace+wl) = '.';
            *(wspace+wl+1) = '\0';
            if (*result) strcat(result, "\n");
            st = pSMgr->suggest_morph_for_spelling_error(wspace);
            if (st)
            {
                strcat(result, st);
                free(st);
            }
            mkallsmall(wspace);
            st = pSMgr->suggest_morph_for_spelling_error(wspace);
            if (st)
            {
                if (*result) strcat(result, "\n");
                strcat(result, st);
                free(st);
            }
            mkinitcap(wspace);
            st = pSMgr->suggest_morph_for_spelling_error(wspace);
            if (st)
            {
                if (*result) strcat(result, "\n");
                strcat(result, st);
                free(st);
            }
        }
        break;
    }
    }

    if (result) return mystrdup(result);
    return NULL;
}

/* analyze word
 * return line count
 * XXX need a better data structure for morphological analysis */
int Hunspell::analyze(char ***out, const char *word)
{
    int  n = 0;
    if (!word) return 0;
    char * m = morph(word);
    if(!m) return 0;
    if (!out) return line_tok(m, out);

    // without memory allocation
    /* BUG missing buffer size checking */
    int i, p;
    for(p = 0, i = 0; m[i]; i++)
    {
        if(m[i] == '\n' || !m[i+1])
        {
            n++;
            strncpy((*out)[n++], m + p, i - p + 1);
            if (m[i] == '\n') (*out)[n++][i - p] = '\0';
            if(!m[i+1]) break;
            p = i + 1;
        }
    }
    free(m);
    return n;
}

