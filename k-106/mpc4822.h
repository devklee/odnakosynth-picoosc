#ifndef _MPC4822_H_
#define _MPC4822_H_
#ifdef __cplusplus
extern "C" {
#endif

void dacInit(void);
void dacWrite(uint8_t flags, uint value);
void dacWriteRaw(uint16_t value);

#ifdef __cplusplus
}
#endif
#endif /* _MPC4822_H_ */