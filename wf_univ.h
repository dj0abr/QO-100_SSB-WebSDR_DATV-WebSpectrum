void drawWF(int id, double *fdata, int cnt, int wpix, int hpix, unsigned int _realqrg, int _rightqrg, int res, int _tunedQRG, char *_fn);
void init_wf_univ();

enum _WATERFALL_IDs_ {
    WFID_BIG = 0,
    WFID_SMALL,
    WFID_MAX
};
