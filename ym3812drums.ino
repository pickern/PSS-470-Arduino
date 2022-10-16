// Test program for arduino controlling a ym3812 synthesis chip
// inside of a yamaha pss-470 keyboard

//define pins
byte AO = 11;
byte WR = 10;
byte RD = 9;
byte CS = 8;

// virtual registers
byte registers[0xF6];
// PORTD is the data bus

int notes[12] = {0x16B, 0x181, 0x198, 0x1B0, 0x1CA, 0x1E5, 0x202, 0x220, 0x241, 0x263, 0x287, 0x2AE};
// 16B - C#
  // 181 - D
  // 198 - D#
  // 1B0 - E
  // 1CA - F
  // 1E5 - F#
  // 202 - G
  // 220 - G#
  // 241 - A
  // 263 - A#
  // 287 - B
  // 2AE - C

int progression[16] = {7, 3, 3, 3, 5, 1, 1, 1, 7, 3, 3, 3, 1, 1, 2, 2}; 
int currentProg = 0;


// write data to register at address
void write(byte address, byte data) {
  //assign virtual register, for bookkeeping
  registers[address] = data;

  //write address
  digitalWrite(CS, HIGH);
  digitalWrite(RD, HIGH);
  digitalWrite(WR, LOW);
  digitalWrite(AO, LOW);
  PORTD = address;
  digitalWrite(CS, LOW);
  delayMicroseconds(1);// write pulse, should be 100ns?
  digitalWrite(CS, HIGH);
  digitalWrite(RD, HIGH);
  digitalWrite(WR, LOW);
  // address write delay
  delayMicroseconds(4);

  //write data
  digitalWrite(AO, HIGH);
  PORTD = data;
  digitalWrite(CS, LOW);
  // write pulse, should be 100ns?
  delayMicroseconds(1);
  digitalWrite(CS, HIGH);
  digitalWrite(RD, LOW);
  digitalWrite(WR, HIGH);
  digitalWrite(A0, HIGH);
  // data write delay
  delayMicroseconds(23);
}

// set all registers to 0
void reset() {
  byte i;
  for(i = 0x01; i<0xF6; i++){
    registers[i] = 0x00;
    write(i, 0x00);
  }
}

// GLOBAL PARAMETERS
// todo: timer stuff, CSM, key split, rhythm
void setWaveformControl(unsigned char in){
  // in:
  // 0: all waves are sine waves
  // 1: waves can be modified according to E0-F5 bits 0-1
  write(0x01, !!in<<5);
}

void setAMVibratoDepth(unsigned char am, unsigned char vib){
  // am:
  // 0: AM depth is 1dB
  // 1: AM depth is 4.8 dB
  // vibrato:
  // 0: vibrato depth is 7 cents
  // 1: vibrato depth is 14 cents
  write(0xBD, (!!am<<7)|(!!vib<<6));
}
// END GLOBAL PARAMETERS

// CHANNEL PARAMETERS
// available channels are 0x00 thru 0x08
void setFeedbackAlgorithm(unsigned char channel, unsigned char fb, unsigned char alg){
  // feedback:
  // 000: no feedback
  // 111: max feedback
  // algorithm:
  // 0: op1 modulates op2
  // 1: both ops produce sound directly
  write(0xC0|channel, fb<<1|alg);
}

void keyOn(unsigned char channel, unsigned int fnum, unsigned char octave){
  // chromatic scale fnums (octave 4):
  // 16B - C#
  // 181 - D
  // 198 - D#
  // 1B0 - E
  // 1CA - F
  // 1E5 - F#
  // 202 - G
  // 220 - G#
  // 241 - A
  // 263 - A#
  // 287 - B
  // 2AE - C
  // octave:
  // 0x0 - octave 0
  // 0x7 - octave 7 

  //write fnum lsb
  write(0xA0|channel, 0xFF & fnum);
  //write keyon, octave, fnum msb
  write(0xB0|channel, 0x20|(octave<<2)|(fnum>>8));
}

void keyOff(unsigned char channel){
  // turns key off, leaves octave and fnum msb intact
  unsigned char target = 0xB0|channel;
  write(target, registers[target]&~(1<<5));
}
// END CHANNEL PARAMETERS

// OPERATOR PARAMETERS
// operator can be 0x00 or 0x01
void setAMVibEGKSRMult(int channel, int op, unsigned char am, unsigned char vib, unsigned char eg, unsigned char ksr, unsigned char mult){
  // am: bit to enable amplitude modulation
  // vib: bit to enable vibrato
  // eg:
  // 0 - sound decays upon hitting sustain phase
  // 1 - sustain level maintained until key released
  // ksr: bit to make envelope shorter as pitch rises
  // mult - modulation frequency multiple
  // 4 bits - sets frequency of operator in relation to f number according to the harmonic series
  unsigned char target = (unsigned char)(0x20 + channel + 5*(channel/3) + 3*op);
  unsigned char data = (am<<7)|(vib<<6)|(eg<<5)|(ksr<<4)|(mult);
  write(target, data);
}

void setKSLOutput(int channel, int op, unsigned char ksl, unsigned char outlvl){
  // ksl - causes output level to decrease as frequency rises
  // 0x0 - no change
  // 0x3 - 6dB/8ve
  // outlvl 
  // 0x00 - loudest
  // 0x1F - softest
  unsigned char target = (unsigned char)(0x40 + channel + 5*(channel/3) + 3*op);
  unsigned char data = (ksl<<6)|(outlvl);
  write(target, data);
}

void setAttackDecay(int channel, int op, unsigned char attack, unsigned char decay){
  // attack & decay each 4 bits
  // 0x0 slowest
  // 0xF fastest
  unsigned char target = (unsigned char)(0x60 + channel + 5*(channel/3) + 3*op);
  unsigned char data = (attack<<4)|(decay);
  write(target, data);
}

void setSustainRelease(int channel, int op, unsigned char sustain, unsigned char rel){
  // sustain:
  // 0x0 loudest
  // 0xF is softest
  // release:
  // 0x0 slowest
  // 0xF fastest
  unsigned char target = (unsigned char)(0x80 + channel + 5*(channel/3) + 3*op);
  unsigned char data = (sustain<<4)|(rel);
  write(target, data);
}

void setWaveform(int channel, int op, unsigned char waveform){
  // waveform:
  // 0x0 - sine
  // 0x1 - half sine
  // 0x2 - abs(sine) 
  // 0x3 - sine saw
  unsigned char target = (unsigned char)(0xE0 + channel + 5*(channel/3) + 3*op);
  write(target, waveform);
}
// END OPERATOR PARAMETERS

void setup() {
  //init pins
  pinMode(AO, OUTPUT);
  pinMode(WR, OUTPUT);
  pinMode(RD, OUTPUT);
  pinMode(CS, OUTPUT);

  digitalWrite(CS, HIGH);
  digitalWrite(WR, LOW);
  digitalWrite(RD, LOW);
  digitalWrite(A0, LOW);

  //init PORTD
  DDRD = B11111111; //all pins output
  PORTD = B00000000; //all pins 0

  // reset
  reset();

  makeDrums();
}

void loop() {
  int currNote = notes[progression[currentProg]];
  //kick
  keyOff(2);
  keyOff(3);
  keyOn(2, 0x198, 3);
  keyOn(3, 0x198, 3);
  keyOff(5);
  keyOn(5, notes[progression[currentProg]], 1);
  
  
  keyOff(4);
  //keyOn(4, 0x198, 4);
  scramble(random(6,9), currNote);
  keyOff(random(6,9));
  delay(150);
  keyOff(2);
  keyOff(3);
  keyOn(2, 0x198, 3);
  keyOn(3, 0x198, 3);
  keyOff(5);
  keyOn(5, notes[progression[currentProg]], 1);
  
  keyOff(4);
  keyOn(4, 0x198, 4);
  scramble(random(6,9), currNote);
  keyOff(random(6,9));
  delay(150);
  keyOff(4);
  keyOn(4, 0x198, 4);
  scramble(random(6,9), currNote);
  keyOff(random(6,9));
  keyOff(5);
  keyOn(5, notes[progression[currentProg]], 1);
  delay(150);
  keyOff(4);
  keyOn(4, 0x198, 4);
  scramble(random(6,9), currNote);
  keyOff(random(6,9));
  keyOff(5);
  keyOn(5, notes[progression[currentProg]], 1);
  delay(150);
  
  //snare
  keyOff(1);
  keyOff(0);
  keyOn(0, 0x198, 2);
  keyOn(1, 0x198, 2);
  
  keyOff(4);
  //keyOn(4, 0x198, 4);
  scramble(random(6,9), currNote);
  keyOff(random(6,9));
  keyOff(5);
  keyOn(5, notes[progression[currentProg]], 1);
  delay(150);
  
  keyOff(2);
  keyOff(3); 
  keyOn(2, 0x198, 3);
  keyOn(3, 0x198, 3);
  keyOff(4);
  keyOn(4, 0x198, 4);
  scramble(random(6,9), currNote);
  keyOff(random(6,9));
  keyOff(5);
  keyOn(5, notes[progression[currentProg]], 1);
  delay(150);
  keyOff(4);
  keyOn(4, 0x198, 4);
  scramble(random(6,9), currNote);
  keyOff(random(6,9));
  keyOff(5);
  keyOn(5, notes[progression[currentProg]], 1);
  delay(150);
  keyOff(4);
  keyOn(4, 0x198, 4);
  scramble(random(6,9), currNote);
  keyOff(random(6,9));
  keyOff(5);
  keyOn(5, notes[progression[currentProg]], 1);
  delay(150);

  currentProg = (currentProg+1)%16;
}

//tests
void makeDrums(){
  setWaveformControl(1);
  //channel 3 and 4 are kick
  kick();
  //channels 1 and 2 are snare
  snare();
  //channel 5
  hat();
  //channel 6
  bass();
}

void bass(){
  setAMVibEGKSRMult(5, 0, 0, 0, 0, 0, 0);
  setAMVibEGKSRMult(5, 1, 0, 0, 0, 0, 2);
  setAttackDecay(5,0,0xF,0x1);
  setAttackDecay(5,1,0xD,0x6);
  setSustainRelease(5,0,0x8,0x3);
  setSustainRelease(5,1,0x4,0xA);
  setWaveform(5, 0, 3);
  setWaveform(5, 1, 3);
}

void kick(){
  // algorithm: (thud) [0,0]->[0,1] + (noise) [1,0]->[1,1]
  //set multipliers
  setAMVibEGKSRMult(2, 0, 0, 0, 0, 0, 1);
  setAMVibEGKSRMult(2, 1, 0, 0, 0, 0, 0);
  setAMVibEGKSRMult(3, 0, 0, 0, 0, 0, 7);
  setAMVibEGKSRMult(3, 1, 0, 0, 0, 0, 0);
  //set attack/decays
  setAttackDecay(2,0,0xE,0xA);
  setAttackDecay(2,1,0xD,0x7);
  setAttackDecay(3,0,0xC,0x8);
  setAttackDecay(3,1,0xC,0x8);
  //set sustain/releases
  setSustainRelease(2,0,0xF,0xF);
  setSustainRelease(2,1,0xF,0xF);
  setSustainRelease(3,0,0xF,0xF);
  setSustainRelease(3,1,0xF,0xF);
  //set feedback
  setFeedbackAlgorithm(3, 0x7, 0);
}

void snare(){
  // algorithm: (thud) [0,0]->[0,1] + (noise) [1,0]->[1,1]
  //set multipliers
  setAMVibEGKSRMult(0, 0, 0, 0, 0, 0, 1);
  setAMVibEGKSRMult(0, 1, 0, 0, 0, 0, 0);
  setAMVibEGKSRMult(1, 0, 0, 0, 0, 0, 7);
  setAMVibEGKSRMult(1, 1, 0, 0, 0, 0, 1);
  //set attack/decays
  setAttackDecay(0,0,0xF,0xE);
  setAttackDecay(0,1,0xF,0xE);
  setAttackDecay(1,0,0xF,0x0);
  setAttackDecay(1,1,0xF,0x7);
  //set sustain/releases
  setSustainRelease(0,0,0xF,0xF);
  setSustainRelease(0,1,0xF,0xF);
  setSustainRelease(1,0,0xF,0xF);
  setSustainRelease(1,1,0xF,0xF);
  //set feedback
  setFeedbackAlgorithm(1, 0x7, 0);
}

void hat(){
  setAMVibEGKSRMult(4, 0, 0, 0, 0, 0, 7);
  setAMVibEGKSRMult(4, 1, 0, 0, 0, 0, 0);
  // set attack decay
  setAttackDecay(4,0,0xF,0x0);
  setAttackDecay(4,1,0xF,0x8);
  // set sustain release
  setSustainRelease(4,0,0xF,0xF);
  setSustainRelease(4,1,0xF,0xF);
  //set feedback
  setFeedbackAlgorithm(4, 0x7, 0);
}

void scramble(char channel, int note){
  // test parameters
  // enable multiple waveforms
  setWaveformControl(1);
  // set mod multiplier to 1
  setAMVibEGKSRMult(channel, 0, random(2), random(2), 0, 0, random(8));
  // set mod level to 40db
  setKSLOutput(channel, 0, 0, 0x10);
  // mod attack quick, decay long
  setAttackDecay(channel, 0, random(8), random(8));
  // mod sustain medium, release medium
  setSustainRelease(channel, 0, random(8), random(8));
  // set mod waveform
  setWaveform(channel, 0, random(4));
  // set car multiplier to 1
  setAMVibEGKSRMult(channel, 1, 0, 0, 0, 0, random(8));
  // set car to max volume
  setKSLOutput(channel, 1, 0, 0x04);
  // car attack quick, decay long
  setAttackDecay(channel, 1, random(8), random(8));
  // car sustain medium, release medium
  setSustainRelease(channel, 1, random(8), random(8));
  // set car waveform
  setWaveform(channel, 1, random(4));
  // keyOn
  keyOn(channel, note, random(8));
}
