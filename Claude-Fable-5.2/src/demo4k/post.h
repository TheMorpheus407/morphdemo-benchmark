/* Audio post-processing of the 4k demo: C reference of what the assembly does.
 * The GPU renders 4 floats per frame: dry L/R and echo-send L/R.  A single
 * in-place pass runs a 3-tap feedback echo network over the send channels,
 * then dry + wet are folded down to an interleaved stereo buffer.            */
#define SONG_SECONDS 60
#define SND_W 4096
#define SND_H 704
static void post_process(float *b, int nframes) {
  /* delays in frames: dotted 8th (16875) with channel swap, plus two short diffusing taps */
  for (int i = 16875 * 4; i < nframes * 4; i += 4) {
    float l = b[i + 2], r = b[i + 3];
    l += 0.38f * b[i - 16875 * 4 + 3] + 0.25f * b[i - 4409 * 4 + 2] + 0.2f * b[i - 2903 * 4 + 3];
    r += 0.38f * b[i - 16875 * 4 + 2] + 0.25f * b[i - 4409 * 4 + 3] + 0.2f * b[i - 2903 * 4 + 2];
    b[i + 2] = l; b[i + 3] = r;
  }
  for (int i = 0; i < nframes; i++) { b[2 * i] = b[4 * i] + b[4 * i + 2]; b[2 * i + 1] = b[4 * i + 1] + b[4 * i + 3]; }
}
