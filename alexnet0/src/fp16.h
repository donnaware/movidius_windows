// Copied from Numpy

#ifdef __cplusplus
extern "C"
{
#endif

static unsigned half2float(unsigned short h);
unsigned short float2half(unsigned f);
void floattofp16(unsigned char *dst, float *src, unsigned nelem);
void fp16tofloat(float *dst, unsigned char *src, unsigned nelem);


#ifdef __cplusplus
}
#endif

