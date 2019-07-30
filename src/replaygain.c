#include "definitions.h"
#include <math.h>

/**
 * Current track gain.
 */
static double track_gain = 0.0;

/**
 * Current track peak.
 */
static double track_peak = 0.0;

/**
 * Track preamp (dB).  Not used currently, but can be.
 */
static double rg_preamp = 1.0;

/**
 * Changes the current track gain value.
 *
 * @param float gain The value as stored in the tags.
 */
static int rg_plugin_init(void)
{
	track_gain = 0.0;
	ices_log_debug("ReplayGain initialized.");
  return 0;
}

/**
 * Applies current track gain to samples.
 *
 * @param int ilen Number of input samples.
 * @param int16_t* il Samples for the left channel.
 * @param int16_t* ir Samples for the right channel.
 */
static int rg_plugin_process(int ilen, int16_t* il, int16_t* ir)
{
  return 0;
}

/**
 * Prepares to handle a new track.
 *
 * Does nothing case.
 */
static void rg_plugin_new_track(input_stream_t* source)
{
	ices_log_debug("ReplayGain got new track.");
}

/**
 * Plugin clean-up.  Does nothing.
 */
static void rg_plugin_shutdown(void)
{
}

static ices_plugin_t Plugin = {
	"replaygain",
	rg_plugin_init,
	rg_plugin_new_track,
	rg_plugin_process,
	rg_plugin_shutdown,

	NULL
};

/**
 * Returns plugin description.
 *
 * @return ices_plugin_t* A pointer to a static plugin description.
 */
ices_plugin_t* replaygain_plugin(void)
{
	return &Plugin;
}




/**
 * Returns the scaling factor.
 *
 * Current track gain/peak is used for computing.
 *
 * @return double The scaling factor.
 */
static double rg_get_scale(void)
{
  double scale;

  if (track_gain == 0.0)
    scale = 1.0;
  else {
    scale = pow(10.0, track_gain / 20.0);
    scale *= rg_preamp;
    if (scale > 15.0)
      scale = 15.0;
    if (track_peak && (scale * track_peak > 1.0))
        scale = 1.0 / track_peak;
  }

  return scale;
}
/**
 * Applies gain to a sample.
 *
 * @param double scale Sample scaling factor, computed from track gain and peak.
 * @param int16_t* sample The converted sample.
 */
static void rg_apply_sample(double scale, int16_t *sample)
{
  int32_t tmp = *sample * scale;
  if (tmp > 32767)
    tmp = 32767;
  else if (tmp < -32768)
    tmp = -32768;
  *sample = tmp;
}

/**
 * Applies current track gain to a sequence of samples.
 */
void rg_apply(int16_t* psamples, int nsamples)
{
  double scale = rg_get_scale();
  while (nsamples) {
    rg_apply_sample(scale, psamples);
    psamples++;
    nsamples--;
  }
}

/**
 * Sets the new track gain.
 *
 * @param double gain Track gain.
 */
void rg_set_track_gain(double gain)
{
    track_gain = gain;
    track_peak = 0.0;
    ices_log("Track gain set to %f.", gain);
}

/**
 * Get current track gain.
 *
 * @return double Current track gain.
 */
double rg_get_track_gain(void)
{
  return track_gain;
}
