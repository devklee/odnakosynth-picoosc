#ifndef _MIDINOTE_H_
#define _MIDINOTE_H_
#ifdef __cplusplus
extern "C" {
#endif

void midiNoteInit(void);
void midiNoteTask(void);
void midiNoteReset(void);
void midiNoteReset2(void);
void midiNoteSendTuneNote(int amount);
void midiNoteSaveMidiNote(void);
void midiNoteSetHalf(bool half);
void midiNoteGetData(uint16_t num, uint16_t *voltage);
void midiNoteSetData(uint16_t num, uint16_t *voltage);
void midiNoteCmd(uint cmd);
void midiNotePrintVoltageTable(void);
void midiNoteNoteOn(uint note, uint velocity);
void midiNoteNoteOff(uint note);
void midiNoteFreqAdjusting(uint flags);
void midiNoteControl(uint note, uint velocity);


#ifdef __cplusplus
}
#endif
#endif /* _MIDINOTE_H_ */