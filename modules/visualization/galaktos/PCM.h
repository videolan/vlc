
void initPCM(int maxsamples);
void addPCM(int16_t [2][512]);
void getPCM(double *data, int samples, int channel, int freq, double smoothing, int derive);
void freePCM();
int getPCMnew(double *PCMdata, int channel, int freq, double smoothing, int derive,int reset);
