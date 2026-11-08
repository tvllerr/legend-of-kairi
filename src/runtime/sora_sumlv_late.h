// (s148 sumlv) the two hooks of sora_sumlv.h that need later headers (sora_summon_menu.h owned rule, sora_dbgmenu.h rows). Include after sora_dbgmenu.h.
#pragma once
// charms owned for the level cap: every SM_DEF row's unlock; Stitch (row 1) is parked out of the Summon list (SMN.own_st 0) but his charm
// counts by its real rule (Vah Ruta, or a fake 4 beasts) - s148 call, else LV7 could never be reached
static int su_charms_(void) {
    int n = 0;
    for (int d = 0; d < SM_NDEF; d++) {
        if (d == 1 && SMN.own_st == 0) {
            const SmDef *D = &SM_DEF[d];
            bool own = D->unlock >= SMU_RUTA && D->unlock <= SMU_NABORIS ? (UL.have && UL.st.beast[D->unlock - SMU_RUTA]) || UL.beasts_fake >= 4 : smn_owned_def(d);
            n += own;
        } else n += smn_owned_def(d);
    }
    return n;
}
// a debug unlock that can raise the cap / own summons is ON: Debug Menu Unlock Summons / Unlock Drive Forms (fake beasts), or debugall
static bool su_debug_(void) { return DM.smn || DM.forms || g_dbgall; }
