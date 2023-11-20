#ifndef _FREQUENCYCOUNTER_H_
#define _FREQUENCYCOUNTER_H_
#ifdef __cplusplus
extern "C" {
#endif

void frequencyCounterInit(void);
bool frequencyCounterGetPpsFlag(void);
void frequencyCounterResetPpsFlag(void);
uint32_t frequencyCounterGetFrequency(void);

#ifdef __cplusplus
}
#endif
#endif /* _FREQUENCYCOUNTER_H_ */