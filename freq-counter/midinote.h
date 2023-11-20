#ifndef _MIDINOTE_H_
#define _MIDINOTE_H_
#ifdef __cplusplus
extern "C" {
#endif

void midiNoteReset(void);
void midiNoteSendMidiNote(uint16_t note);
void midiNoteSendTuneNote(int amount);
void midiNoteSaveMidiNote(void);
void midiNoteSetHalf(bool half);
void midiNoteGetData(uint16_t num, uint16_t *voltage);
void midiNoteSetData(uint16_t num, uint16_t *voltage);
void midiNoteSetNoteVoltage(uint32_t freq, uint16_t voltage);
#ifdef __cplusplus
}
#endif
#endif /* _MIDINOTE_H_ */