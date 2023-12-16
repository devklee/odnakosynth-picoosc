#ifndef _MIDI_H_
#define _MIDI_H_
#ifdef __cplusplus
extern "C" {
#endif

void midiInit(bool logMidiEvent, bool serialTask, bool usbTask);
void midiProcessTask(void);
void midiSendPing(void);
void midiSendFreq(uint counter, uint32_t value);
void midiSendNoteTable(uint num);

#ifdef __cplusplus
}
#endif
#endif /* _MIDI_H_ */