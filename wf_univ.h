void drawWF(int id, double *data, int cnt, int wpix, int hpix, int leftqrg, int rightqrg, int res, int tunedQRG, char *fn);
void init_wf_univ();

enum _WATERFALL_IDs_ {
    WFID_BIG = 0,
    WFID_SMALL,
    WFID_MAX
};
