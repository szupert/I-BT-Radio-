#ifndef dsp_full_loc
#define dsp_full_loc
#include <pgmspace.h>
/*************************************************************************************
    HOWTO:
    Copy this file to I-BT-Radio/locale/displayL10n_custom.h
    and modify it
*************************************************************************************/
const char mon[] PROGMEM = "H";
const char tue[] PROGMEM = "K";
const char wed[] PROGMEM = "Sze";
const char thu[] PROGMEM = "Cs";
const char fri[] PROGMEM = "P";
const char sat[] PROGMEM = "Szo";
const char sun[] PROGMEM = "V";

const char monf[] PROGMEM = "hetfo";
const char tuef[] PROGMEM = "kedd";
const char wedf[] PROGMEM = "szerda";
const char thuf[] PROGMEM = "csutortok";
const char frif[] PROGMEM = "pentek";
const char satf[] PROGMEM = "szombat";
const char sunf[] PROGMEM = "vasarnap";

const char jan[] PROGMEM = "januar";
const char feb[] PROGMEM = "februar";
const char mar[] PROGMEM = "marcius";
const char apr[] PROGMEM = "aprilis";
const char may[] PROGMEM = "majus";
const char jun[] PROGMEM = "junius";
const char jul[] PROGMEM = "julius";
const char aug[] PROGMEM = "augusztus";
const char sep[] PROGMEM = "szeptember";
const char octt[] PROGMEM = "oktober";
const char nov[] PROGMEM = "november";
const char decc[] PROGMEM = "december";

const char wn_N[]      PROGMEM = "ESZAK";
const char wn_NNE[]    PROGMEM = "EEK";
const char wn_NE[]     PROGMEM = "EK";
const char wn_ENE[]    PROGMEM = "KEK";
const char wn_E[]      PROGMEM = "KELET";
const char wn_ESE[]    PROGMEM = "KDK";
const char wn_SE[]     PROGMEM = "DK";
const char wn_SSE[]    PROGMEM = "DDK";
const char wn_S[]      PROGMEM = "DEL";
const char wn_SSW[]    PROGMEM = "DDNy";
const char wn_SW[]     PROGMEM = "DNy";
const char wn_WSW[]    PROGMEM = "NyDNy";
const char wn_W[]      PROGMEM = "NYUGAT";
const char wn_WNW[]    PROGMEM = "NyENy";
const char wn_NW[]     PROGMEM = "ENy";
const char wn_NNW[]    PROGMEM = "EENy";

const char* const dow[]     PROGMEM = { sun, mon, tue, wed, thu, fri, sat };
const char* const dowf[]    PROGMEM = { sunf, monf, tuef, wedf, thuf, frif, satf };
const char* const mnths[]   PROGMEM = { jan, feb, mar, apr, may, jun, jul, aug, sep, octt, nov, decc };
const char* const wind[]    PROGMEM = { wn_N, wn_NNE, wn_NE, wn_ENE, wn_E, wn_ESE, wn_SE, wn_SSE, wn_S, wn_SSW, wn_SW, wn_WSW, wn_W, wn_WNW, wn_NW, wn_NNW, wn_N };

const char    const_PlReady[]    PROGMEM = "[kesz]";
const char  const_PlStopped[]    PROGMEM = "[leallitva]";
const char  const_PlConnect[]    PROGMEM = "[csatlakozas]";
const char  const_DlgVolume[]    PROGMEM = "HANGERO";
const char    const_DlgLost[]    PROGMEM = "* NINCS KAPCSOLAT *";
const char  const_DlgUpdate[]    PROGMEM = "* FRISSITES *";
const char const_DlgNextion[]    PROGMEM = "* NEXTION *";
const char const_getWeather[]    PROGMEM = "";
const char  const_waitForSD[]    PROGMEM = "SD BEOLVASAS";

const char        apNameTxt[]    PROGMEM = "HALOZAT NEVE";
const char        apPassTxt[]    PROGMEM = "JELSZO";
const char       bootstrFmt[]    PROGMEM = "Csatlakozas: %s";
const char        apSettFmt[]    PROGMEM = "BEALLITAS: HTTP://%s/";
#if EXT_WEATHER
const char       weatherFmt[]    PROGMEM = "%s, %.1f\011C \007 hoerzet: %.1f\011C \007 legnyomas: %d мм \007 paratartalom: %d%% \007 szel: %.1f m/s [%s]";
#else
const char       weatherFmt[]    PROGMEM = "%s, %.1f\011C \007 legnyomas: %d mm \007 paratartalom: %d%%";
#endif
const char     weatherUnits[]    PROGMEM = "metric";   /* standard, metric, imperial */
const char      weatherLang[]    PROGMEM = "hu";       /* https://openweathermap.org/current#multi */

#endif
