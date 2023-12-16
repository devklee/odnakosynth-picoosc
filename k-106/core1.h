#ifndef _CORE1_H_
#define _CORE1_H_
#ifdef __cplusplus
extern "C" {
#endif

typedef struct queueCore1Item 
{
    uint8_t     op;
    uint8_t     flags;
    uint32_t    value;
} queueCore1Item;

void core1Init(void);
void core1Entry(void);
bool isCmdMode(void);
bool core1AddItem(queueCore1Item *item);
bool isProcessHalfVoltage(void);
bool isUseVoltageTable(void);
void cmdPrint(const char *restrict message, ...);

#ifdef __cplusplus
}
#endif
#endif /* _CORE1_H_ */