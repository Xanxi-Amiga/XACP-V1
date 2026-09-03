
long amp_input_callback(struct Amp *itsAmp, void *buffer, long DataToRead);
void amp_output_callback(struct Amp *itsAmp, unsigned char Layer);
void amp_equalize_callback(struct Amp *itsAmp, unsigned char Layer);

