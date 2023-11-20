# odnakosynth-picoosc
Raspberry Pico Wave Oscillator Projects

### PWM oscillator [project](https://github.com/devklee/odnakosynth-picoosc/tree/main/pwmosc)

Sawtooth and square wave are generated with PWM on different pins. The circuit is also included.

### PWM with 4 bit R-2R DAC oscillator [project](https://github.com/devklee/odnakosynth-picoosc/tree/main/pwmr2rosc)

Sawtooth saw is generated with 4 bit R-2R DAC. Signal is smoothed with RC filter. Callback function is called via pin IRQ. Pin IRQ is controlled by pin PWD. The circuit is also included.


### 4-chanel SPI 12-bit DAC oscillator oscillator [project](https://github.com/devklee/odnakosynth-picoosc/tree/main/spiosc)

2x MCP4822 with 4x 12-bit DAC chanel. The circuit is also included.

### Frequency Counter and more [project](https://github.com/devklee/odnakosynth-picoosc/tree/main/freq-counter)

Frequency Counter, MIDI USB and serial, VCO. The circuit is also included.

## References

https://blog.thea.codes/the-design-of-the-juno-dco/

https://electricdruid.net/roland-juno-dcos/

https://github.com/polykit/pico-dco

https://www.electronics-tutorials.ws/combination/r-2r-dac.html
