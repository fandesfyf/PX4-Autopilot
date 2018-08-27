/****************************************************************************
 *
 *   Copyright (C) 2013 PX4 Development Team. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name PX4 nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/

/**
 * Driver for the PX4 audio alarm port, /dev/tone_alarm.
 *
 * The tone_alarm driver supports a set of predefined "alarm"
 * tunes and one user-supplied tune.
 *
 * The TONE_SET_ALARM ioctl can be used to select a predefined
 * alarm tune, from 1 - <TBD>.  Selecting tune zero silences
 * the alarm.
 *
 * Tunes follow the syntax of the Microsoft GWBasic/QBasic PLAY
 * statement, with some exceptions and extensions.
 *
 * From Wikibooks:
 *
 * PLAY "[string expression]"
 *
 * Used to play notes and a score ... The tones are indicated by letters A through G.
 * Accidentals are indicated with a "+" or "#" (for sharp) or "-" (for flat)
 * immediately after the note letter. See this example:
 *
 *   PLAY "C C# C C#"
 *
 * Whitespaces are ignored inside the string expression. There are also codes that
 * set the duration, octave and tempo. They are all case-insensitive. PLAY executes
 * the commands or notes the order in which they appear in the string. Any indicators
 * that change the properties are effective for the notes following that indicator.
 *
 * Ln     Sets the duration (length) of the notes. The variable n does not indicate an actual duration
 *        amount but rather a note type; L1 - whole note, L2 - half note, L4 - quarter note, etc.
 *        (L8, L16, L32, L64, ...). By default, n = 4.
 *        For triplets and quintets, use L3, L6, L12, ... and L5, L10, L20, ... series respectively.
 *        The shorthand notation of length is also provided for a note. For example, "L4 CDE L8 FG L4 AB"
 *        can be shortened to "L4 CDE F8G8 AB". F and G play as eighth notes while others play as quarter notes.
 * On     Sets the current octave. Valid values for n are 0 through 6. An octave begins with C and ends with B.
 *        Remember that C- is equivalent to B.
 * < >    Changes the current octave respectively down or up one level.
 * Nn     Plays a specified note in the seven-octave range. Valid values are from 0 to 84. (0 is a pause.)
 *        Cannot use with sharp and flat. Cannot use with the shorthand notation neither.
 * MN     Stand for Music Normal. Note duration is 7/8ths of the length indicated by Ln. It is the default mode.
 * ML     Stand for Music Legato. Note duration is full length of that indicated by Ln.
 * MS     Stand for Music Staccato. Note duration is 3/4ths of the length indicated by Ln.
 * Pn     Causes a silence (pause) for the length of note indicated (same as Ln).
 * Tn     Sets the number of "L4"s per minute (tempo). Valid values are from 32 to 255. The default value is T120.
 * .      When placed after a note, it causes the duration of the note to be 3/2 of the set duration.
 *        This is how to get "dotted" notes. "L4 C#." would play C sharp as a dotted quarter note.
 *        It can be used for a pause as well.
 *
 * Extensions/variations:
 *
 * MB MF  The MF command causes the tune to play once and then stop. The MB command causes the
 *        tune to repeat when it ends.
 *
 */

#include <px4_config.h>
#include <debug.h>

#include <drivers/device/device.h>
#include <drivers/drv_tone_alarm.h>

#include <sys/types.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <math.h>
#include <ctype.h>

#include <board_config.h>
#include <drivers/drv_hrt.h>

#include <chip.h>
#include <up_internal.h>
#include <up_arch.h>

#include <chip/sam_pinmap.h>
#include <sam_gpio.h>
#include <sam_tc.h>

#include <systemlib/err.h>
#include <circuit_breaker/circuit_breaker.h>

/* Check that tone alarm and HRT timers are different */
#if defined(TONE_ALARM_CHANNEL)  && defined(HRT_TIMER_CHANNEL)
# if TONE_ALARM_CHANNEL == HRT_TIMER_CHANNEL
#   error TONE_ALARM_CHANNEL and HRT_TIMER_CHANNEL must use different timers.
# endif
#endif

/* Tone alarm configuration */
/**********************************************************/
/**********************************************************/
/**********************************************************/
/**********************************************************/
/**********************************************************/
/**********************************************************/
/********************* TONE_ALARM_NOT_DONE ****************/
/******           This code is not finished 	***********/
/**********************************************************/
/**********************************************************/
/**********************************************************/
/**********************************************************/
/**********************************************************/
/**********************************************************/
#ifdef TONE_ALARM_CHANNEL

#if defined(TONE_ALARM_TIMER)
# error "TONE_ALARM_TIMER should not be defined, instead define TONE_ALARM_CHANNEL from 0-11"
#endif

#if TONE_ALARM_CHANNEL == 0 || TONE_ALARM_CHANNEL == 1 || TONE_ALARM_CHANNEL == 2
# define TONE_ALARM_TIMER 	0
#endif
#if TONE_ALARM_CHANNEL == 3 || TONE_ALARM_CHANNEL == 4 || TONE_ALARM_CHANNEL == 5
# define TONE_ALARM_TIMER 	1
#endif
#if TONE_ALARM_CHANNEL == 6 || TONE_ALARM_CHANNEL == 7 || TONE_ALARM_CHANNEL == 8
# define TONE_ALARM_TIMER 	2
#endif
#if TONE_ALARM_CHANNEL == 9 || TONE_ALARM_CHANNEL == 10 || TONE_ALARM_CHANNEL == 11
# define TONE_ALARM_TIMER 	3
#endif

/* HRT configuration */
#if   TONE_ALARM_TIMER == 0
# define HRT_TIMER_BASE			SAM_TC012_BASE
# if !defined(CONFIG_SAMV7_TC0)
#  error "HRT_TIMER_CHANNEL 0-2 Require CONFIG_SAMV7_TC0=y"
# endif
#elif TONE_ALARM_TIMER == 1
# define HRT_TIMER_BASE			SAM_TC345_BASE
# if !defined(CONFIG_SAMV7_TC1)
#  error "HRT_TIMER_CHANNEL 3-5 Require CONFIG_SAMV7_TC1=y"
# endif
#elif TONE_ALARM_TIMER == 2
# define HRT_TIMER_BASE			SAM_TC678_BASE
# if !defined(CONFIG_SAMV7_TC2)
#  error "HRT_TIMER_CHANNEL 6-8 Require CONFIG_SAMV7_TC2=y"
# endif
#elif TONE_ALARM_TIMER == 3
# define HRT_TIMER_BASE			SAM_TC901_BASE
# if !defined(CONFIG_SAMV7_TC3)
#  error "HRT_TIMER_CHANNEL 9-11 Require CONFIG_SAMV7_TC3=y"
# endif
#else
# error "HRT_TIMER_CHANNEL should be defined valid value are from 0-11"
# endif

#define TONE_ALARM_CLOCK (BOARD_MCK_FREQUENCY/128)


/*
 * Timer register accessors
 */
#define REG(_reg)	(*(volatile uint32_t *)(SAM_TC0_BASE + SAM_TC_CHAN_OFFSET(HRT_TIMER_CHANNEL) + _reg))

#define rCCR	REG(SAM_TC_CCR_OFFSET)
#define rCMR 	REG(SAM_TC_CMR_OFFSET)
#define rSMMR 	REG(SAM_TC_SMMR_OFFSET)
#define rRAB 	REG(SAM_TC_RAB_OFFSET)
#define rCV 	REG(SAM_TC_CV_OFFSET)
#define rRA 	REG(SAM_TC_RA_OFFSET)
#define rRB 	REG(SAM_TC_RB_OFFSET)
#define rRC 	REG(SAM_TC_RC_OFFSET)
#define rSR 	REG(SAM_TC_SR_OFFSET)
#define rIER 	REG(SAM_TC_IER_OFFSET)
#define rIDR 	REG(SAM_TC_IDR_OFFSET)
#define rIMR 	REG(SAM_TC_IMR_OFFSET)
#define rEMR 	REG(SAM_TC_EMR_OFFSET)

#define CBRK_BUZZER_KEY 782097

class ToneAlarm : public cdev::CDev
{
public:
	ToneAlarm();
	~ToneAlarm() = default;

	virtual int		init();

	virtual int		ioctl(file *filp, int cmd, unsigned long arg);
	virtual ssize_t		write(file *filp, const char *buffer, size_t len);
	inline const char	*name(int tune)
	{
		return _tune_names[tune];
	}

	enum {
		CBRK_OFF = 0,
		CBRK_ON,
		CBRK_UNINIT
	};

private:
	static const unsigned	_tune_max = 1024 * 8; // be reasonable about user tunes
	const char		 *_default_tunes[TONE_NUMBER_OF_TUNES];
	const char		 *_tune_names[TONE_NUMBER_OF_TUNES];
	static const uint8_t	_note_tab[];

	unsigned		_default_tune_number; // number of currently playing default tune (0 for none)

	const char		*_user_tune;

	const char		*_tune;		// current tune string
	const char		*_next;		// next note in the string

	unsigned		_tempo;
	unsigned		_note_length;
	enum { MODE_NORMAL, MODE_LEGATO, MODE_STACCATO} _note_mode;
	unsigned		_octave;
	unsigned		_silence_length; // if nonzero, silence before next note
	bool			_repeat;	// if true, tune restarts at end
	int				_cbrk;	//if true, no audio output

	hrt_call		_note_call;	// HRT callout for note completion

	// Convert a note value in the range C1 to B7 into a divisor for
	// the configured timer's clock.
	//
	unsigned		note_to_divisor(unsigned note);

	// Calculate the duration in microseconds of play and silence for a
	// note given the current tempo, length and mode and the number of
	// dots following in the play string.
	//
	unsigned		note_duration(unsigned &silence, unsigned note_length, unsigned dots);

	// Calculate the duration in microseconds of a rest corresponding to
	// a given note length.
	//
	unsigned		rest_duration(unsigned rest_length, unsigned dots);

	// Start playing the note
	//
	void			start_note(unsigned note);

	// Stop playing the current note and make the player 'safe'
	//
	void			stop_note();

	// Start playing the tune
	//
	void			start_tune(const char *tune);

	// Parse the next note out of the string and play it
	//
	void			next_note();

	// Find the next character in the string, discard any whitespace and
	// return the canonical (uppercase) version.
	//
	int			next_char();

	// Extract a number from the string, consuming all the digit characters.
	//
	unsigned		next_number();

	// Consume dot characters from the string, returning the number consumed.
	//
	unsigned		next_dots();

	// hrt_call trampoline for next_note
	//
	static void		next_trampoline(void *arg);

};

// semitone offsets from C for the characters 'A'-'G'
const uint8_t ToneAlarm::_note_tab[] = {9, 11, 0, 2, 4, 5, 7};

/*
 * Driver 'main' command.
 */
extern "C" __EXPORT int tone_alarm_main(int argc, char *argv[]);


ToneAlarm::ToneAlarm() :
	CDev(TONEALARM0_DEVICE_PATH),
	_default_tune_number(0),
	_user_tune(nullptr),
	_tune(nullptr),
	_next(nullptr),
	_cbrk(CBRK_UNINIT)
{
	// enable debug() calls
	//_debug_enabled = true;
	_default_tunes[TONE_STARTUP_TUNE] = "MFT240L8 O4aO5dc O4aO5dc O4aO5dc L16dcdcdcdc";		// startup tune
	_default_tunes[TONE_ERROR_TUNE] = "MBT200a8a8a8PaaaP";						// ERROR tone
	_default_tunes[TONE_NOTIFY_POSITIVE_TUNE] = "MFT200e8a8a";					// Notify Positive tone
	_default_tunes[TONE_NOTIFY_NEUTRAL_TUNE] = "MFT200e8e";						// Notify Neutral tone
	_default_tunes[TONE_NOTIFY_NEGATIVE_TUNE] = "MFT200e8c8e8c8e8c8";				// Notify Negative tone
	_default_tunes[TONE_ARMING_WARNING_TUNE] = "MNT75L1O2G";					//arming warning
	_default_tunes[TONE_BATTERY_WARNING_SLOW_TUNE] = "MBNT100a8";					//battery warning slow
	_default_tunes[TONE_BATTERY_WARNING_FAST_TUNE] = "MBNT255a8a8a8a8a8a8a8a8a8a8a8a8a8a8a8a8";	//battery warning fast
	_default_tunes[TONE_GPS_WARNING_TUNE] = "MFT255L4AAAL1F#";					//gps warning slow
	_default_tunes[TONE_ARMING_FAILURE_TUNE] = "MFT255L4<<<BAP";
	_default_tunes[TONE_PARACHUTE_RELEASE_TUNE] = "MFT255L16agagagag";			// parachute release
	_default_tunes[TONE_EKF_WARNING_TUNE] = "MFT255L8ddd#d#eeff";				// ekf warning
	_default_tunes[TONE_BARO_WARNING_TUNE] = "MFT255L4gf#fed#d";				// baro warning
	_default_tunes[TONE_SINGLE_BEEP_TUNE] = "MFT100a8";                             // single beep
	_default_tunes[TONE_HOME_SET] = "MFT100L4>G#6A#6B#4";

	_tune_names[TONE_STARTUP_TUNE] = "startup";			// startup tune
	_tune_names[TONE_ERROR_TUNE] = "error";				// ERROR tone
	_tune_names[TONE_NOTIFY_POSITIVE_TUNE] = "positive";		// Notify Positive tone
	_tune_names[TONE_NOTIFY_NEUTRAL_TUNE] = "neutral";		// Notify Neutral tone
	_tune_names[TONE_NOTIFY_NEGATIVE_TUNE] = "negative";		// Notify Negative tone
	_tune_names[TONE_ARMING_WARNING_TUNE] = "arming";		// arming warning
	_tune_names[TONE_BATTERY_WARNING_SLOW_TUNE] = "slow_bat";	// battery warning slow
	_tune_names[TONE_BATTERY_WARNING_FAST_TUNE] = "fast_bat";	// battery warning fast
	_tune_names[TONE_GPS_WARNING_TUNE] = "gps_warning";	            // gps warning
	_tune_names[TONE_ARMING_FAILURE_TUNE] = "arming_failure";            //fail to arm
	_tune_names[TONE_PARACHUTE_RELEASE_TUNE] = "parachute_release";	// parachute release
	_tune_names[TONE_EKF_WARNING_TUNE] = "ekf_warning";				// ekf warning
	_tune_names[TONE_BARO_WARNING_TUNE] = "baro_warning";			// baro warning
	_tune_names[TONE_SINGLE_BEEP_TUNE] = "beep";                    // single beep
	_tune_names[TONE_HOME_SET] = "home_set";
}

int
ToneAlarm::init()
{
	int ret;

	ret = CDev::init();

	if (ret != OK) {
		return ret;
	}

	/* configure the GPIO to the idle state */
	sam_configgpio(GPIO_TONE_ALARM_IDLE);

#if TONE_ALARM_NOT_DONE	/* initialise the timer */
	rCR1 = 0;
	rCR2 = 0;
	rSMCR = 0;
	rDIER = 0;
	rCCER &= TONE_CCER;		/* unlock CCMR* registers */
	rCCMR1 = TONE_CCMR1;
	rCCMR2 = TONE_CCMR2;
	rCCER = TONE_CCER;
	rDCR = 0;

#ifdef rBDTR // If using an advanced timer, you need to activate the output
	rBDTR = ATIM_BDTR_MOE; // enable the main output of the advanced timer
#endif

	/* toggle the CC output each time the count passes 1 */
	TONE_rCCR = 1;

	/* default the timer to a prescale value of 1; playing notes will change this */
	rPSC = 0;

	/* make sure the timer is running */
	rCR1 = GTIM_CR1_CEN;
#endif

	return OK;
}

unsigned
ToneAlarm::note_to_divisor(unsigned note)
{
	// compute the frequency first (Hz)
	float freq = 880.0f * expf(logf(2.0f) * ((int)note - 46) / 12.0f);

	float period = 0.5f / freq;

	// and the divisor, rounded to the nearest integer
	unsigned divisor = (period * TONE_ALARM_CLOCK) + 0.5f;

	return divisor;
}

unsigned
ToneAlarm::note_duration(unsigned &silence, unsigned note_length, unsigned dots)
{
	unsigned whole_note_period = (60 * 1000000 * 4) / _tempo;

	if (note_length == 0) {
		note_length = 1;
	}

	unsigned note_period = whole_note_period / note_length;

	switch (_note_mode) {
	case MODE_NORMAL:
		silence = note_period / 8;
		break;

	case MODE_STACCATO:
		silence = note_period / 4;
		break;

	default:
	case MODE_LEGATO:
		silence = 0;
		break;
	}

	note_period -= silence;

	unsigned dot_extension = note_period / 2;

	while (dots--) {
		note_period += dot_extension;
		dot_extension /= 2;
	}

	return note_period;
}

unsigned
ToneAlarm::rest_duration(unsigned rest_length, unsigned dots)
{
	unsigned whole_note_period = (60 * 1000000 * 4) / _tempo;

	if (rest_length == 0) {
		rest_length = 1;
	}

	unsigned rest_period = whole_note_period / rest_length;

	unsigned dot_extension = rest_period / 2;

	while (dots--) {
		rest_period += dot_extension;
		dot_extension /= 2;
	}

	return rest_period;
}

void
ToneAlarm::start_note(unsigned note)
{
	// check if circuit breaker is enabled
	if (_cbrk == CBRK_UNINIT) {
		_cbrk = circuit_breaker_enabled("CBRK_BUZZER", CBRK_BUZZER_KEY);
	}

	if (_cbrk != CBRK_OFF) { return; }

	// compute the divisor
	unsigned divisor = note_to_divisor(note);

	// pick the lowest prescaler value that we can use
	// (note that the effective prescale value is 1 greater)
	unsigned prescale = divisor / 65536;

	// calculate the timer period for the selected prescaler value
	unsigned period = (divisor / (prescale + 1)) - 1;
#if TONE_ALARM_NOT_DONE
	rPSC = prescale;	// load new prescaler
	rARR = period;		// load new toggle period
	rEGR = GTIM_EGR_UG;	// force a reload of the period
	rCCER |= TONE_CCER;	// enable the output
#else
	prescale++;
	period++;
#endif
	// configure the GPIO to enable timer output
	sam_configgpio(GPIO_TONE_ALARM);
}

void
ToneAlarm::stop_note()
{
	/* stop the current note */
#if TONE_ALARM_NOT_DONE
	rCCER &= ~TONE_CCER;
#endif
	/*
	 * Make sure the GPIO is not driving the speaker.
	 */
	sam_configgpio(GPIO_TONE_ALARM_IDLE);
}

void
ToneAlarm::start_tune(const char *tune)
{
	// kill any current playback
	hrt_cancel(&_note_call);

	// record the tune
	_tune = tune;
	_next = tune;

	// initialise player state
	_tempo = 120;
	_note_length = 4;
	_note_mode = MODE_NORMAL;
	_octave = 4;
	_silence_length = 0;
	_repeat = false;		// otherwise command-line tunes repeat forever...

	// schedule a callback to start playing
	hrt_call_after(&_note_call, 0, (hrt_callout)next_trampoline, this);
}

void
ToneAlarm::next_note()
{
	// do we have an inter-note gap to wait for?
	if (_silence_length > 0) {
		stop_note();
		hrt_call_after(&_note_call, (hrt_abstime)_silence_length, (hrt_callout)next_trampoline, this);
		_silence_length = 0;
		return;
	}

	// make sure we still have a tune - may be removed by the write / ioctl handler
	if ((_next == nullptr) || (_tune == nullptr)) {
		stop_note();
		return;
	}

	// parse characters out of the string until we have resolved a note
	unsigned note = 0;
	unsigned note_length = _note_length;
	unsigned duration;

	while (note == 0) {
		// we always need at least one character from the string
		int c = next_char();

		if (c == 0) {
			goto tune_end;
		}

		_next++;

		switch (c) {
		case 'L':	// select note length
			_note_length = next_number();

			if (_note_length < 1) {
				goto tune_error;
			}

			break;

		case 'O':	// select octave
			_octave = next_number();

			if (_octave > 6) {
				_octave = 6;
			}

			break;

		case '<':	// decrease octave
			if (_octave > 0) {
				_octave--;
			}

			break;

		case '>':	// increase octave
			if (_octave < 6) {
				_octave++;
			}

			break;

		case 'M':	// select inter-note gap
			c = next_char();

			if (c == 0) {
				goto tune_error;
			}

			_next++;

			switch (c) {
			case 'N':
				_note_mode = MODE_NORMAL;
				break;

			case 'L':
				_note_mode = MODE_LEGATO;
				break;

			case 'S':
				_note_mode = MODE_STACCATO;
				break;

			case 'F':
				_repeat = false;
				break;

			case 'B':
				_repeat = true;
				break;

			default:
				goto tune_error;
			}

			break;

		case 'P':	// pause for a note length
			stop_note();
			hrt_call_after(&_note_call,
				       (hrt_abstime)rest_duration(next_number(), next_dots()),
				       (hrt_callout)next_trampoline,
				       this);
			return;

		case 'T': {	// change tempo
				unsigned nt = next_number();

				if ((nt >= 32) && (nt <= 255)) {
					_tempo = nt;

				} else {
					goto tune_error;
				}

				break;
			}

		case 'N':	// play an arbitrary note
			note = next_number();

			if (note > 84) {
				goto tune_error;
			}

			if (note == 0) {
				// this is a rest - pause for the current note length
				hrt_call_after(&_note_call,
					       (hrt_abstime)rest_duration(_note_length, next_dots()),
					       (hrt_callout)next_trampoline,
					       this);
				return;
			}

			break;

		case 'A'...'G':	// play a note in the current octave
			note = _note_tab[c - 'A'] + (_octave * 12) + 1;
			c = next_char();

			switch (c) {
			case '#':	// up a semitone
			case '+':
				if (note < 84) {
					note++;
				}

				_next++;
				break;

			case '-':	// down a semitone
				if (note > 1) {
					note--;
				}

				_next++;
				break;

			default:
				// 0 / no next char here is OK
				break;
			}

			// shorthand length notation
			note_length = next_number();

			if (note_length == 0) {
				note_length = _note_length;
			}

			break;

		default:
			goto tune_error;
		}
	}

	// compute the duration of the note and the following silence (if any)
	duration = note_duration(_silence_length, note_length, next_dots());

	// start playing the note
	start_note(note);

	// and arrange a callback when the note should stop
	hrt_call_after(&_note_call, (hrt_abstime)duration, (hrt_callout)next_trampoline, this);
	return;

	// tune looks bad (unexpected EOF, bad character, etc.)
tune_error:
	syslog(LOG_ERR, "tune error\n");
	_repeat = false;		// don't loop on error

	// stop (and potentially restart) the tune
tune_end:
	stop_note();

	if (_repeat) {
		start_tune(_tune);

	} else {
		_tune = nullptr;
		_default_tune_number = 0;
	}

	return;
}

int
ToneAlarm::next_char()
{
	while (isspace(*_next)) {
		_next++;
	}

	return toupper(*_next);
}

unsigned
ToneAlarm::next_number()
{
	unsigned number = 0;
	int c;

	for (;;) {
		c = next_char();

		if (!isdigit(c)) {
			return number;
		}

		_next++;
		number = (number * 10) + (c - '0');
	}
}

unsigned
ToneAlarm::next_dots()
{
	unsigned dots = 0;

	while (next_char() == '.') {
		_next++;
		dots++;
	}

	return dots;
}

void
ToneAlarm::next_trampoline(void *arg)
{
	ToneAlarm *ta = (ToneAlarm *)arg;

	ta->next_note();
}


int
ToneAlarm::ioctl(file *filp, int cmd, unsigned long arg)
{
	int result = OK;

	PX4_DEBUG("ioctl %i %u", cmd, arg);

//	irqstate_t flags = enter_critical_section();

	/* decide whether to increase the alarm level to cmd or leave it alone */
	switch (cmd) {
	case TONE_SET_ALARM:
		PX4_DEBUG("TONE_SET_ALARM %u", arg);

		if (arg < TONE_NUMBER_OF_TUNES) {
			if (arg == TONE_STOP_TUNE) {
				// stop the tune
				_tune = nullptr;
				_next = nullptr;
				_repeat = false;
				_default_tune_number = 0;

			} else {
				/* always interrupt alarms, unless they are repeating and already playing */
				if (!(_repeat && _default_tune_number == arg)) {
					/* play the selected tune */
					_default_tune_number = arg;
					start_tune(_default_tunes[arg]);
				}
			}

		} else {
			result = -EINVAL;
		}

		break;

	default:
		result = -ENOTTY;
		break;
	}

//	leave_critical_section(flags);

	/* give it to the superclass if we didn't like it */
	if (result == -ENOTTY) {
		result = CDev::ioctl(filp, cmd, arg);
	}

	return result;
}

int
ToneAlarm::write(file *filp, const char *buffer, size_t len)
{
	// sanity-check the buffer for length and nul-termination
	if (len > _tune_max) {
		return -EFBIG;
	}

	// if we have an existing user tune, free it
	if (_user_tune != nullptr) {

		// if we are playing the user tune, stop
		if (_tune == _user_tune) {
			_tune = nullptr;
			_next = nullptr;
		}

		// free the old user tune
		free((void *)_user_tune);
		_user_tune = nullptr;
	}

	// if the new tune is empty, we're done
	if (buffer[0] == '\0') {
		return OK;
	}

	// allocate a copy of the new tune
	_user_tune = strndup(buffer, len);

	if (_user_tune == nullptr) {
		return -ENOMEM;
	}

	// and play it
	start_tune(_user_tune);

	return len;
}

/**
 * Local functions in support of the shell command.
 */
namespace
{

ToneAlarm	*g_dev;

int
play_tune(unsigned tune)
{
	exit(0);

	int	fd, ret;

	fd = open(TONEALARM0_DEVICE_PATH, 0);

	if (fd < 0) {
		err(1, TONEALARM0_DEVICE_PATH);
	}

	ret = ioctl(fd, TONE_SET_ALARM, tune);
	close(fd);

	if (ret != 0) {
		err(1, "TONE_SET_ALARM");
	}

	exit(0);
}

int
play_string(const char *str, bool free_buffer)
{
	int	fd, ret;

	fd = open(TONEALARM0_DEVICE_PATH, O_WRONLY);

	if (fd < 0) {
		err(1, TONEALARM0_DEVICE_PATH);
	}

	ret = write(fd, str, strlen(str) + 1);
	close(fd);

	if (free_buffer) {
		free((void *)str);
	}

	if (ret < 0) {
		err(1, "play tune");
	}

	exit(0);
}

} // namespace

int
tone_alarm_main(int argc, char *argv[])
{
	unsigned tune = 0;

	/* start the driver lazily */
	if (g_dev == nullptr) {
		g_dev = new ToneAlarm;

		if (g_dev == nullptr) {
			errx(1, "couldn't allocate the ToneAlarm driver");
		}

		if (g_dev->init() != OK) {
			delete g_dev;
			errx(1, "ToneAlarm init failed");
		}
	}

	if (argc > 1) {
		const char *argv1 = argv[1];

		if (!strcmp(argv1, "start")) {
			play_tune(TONE_STOP_TUNE);
		}

		if (!strcmp(argv1, "stop")) {
			play_tune(TONE_STOP_TUNE);
		}

		if ((tune = strtol(argv1, nullptr, 10)) != 0) {
			play_tune(tune);
		}

		/* It might be a tune name */
		for (tune = 1; tune < TONE_NUMBER_OF_TUNES; tune++)
			if (!strcmp(g_dev->name(tune), argv1)) {
				play_tune(tune);
			}

		/* If it is a file name then load and play it as a string */
		if (*argv1 == '/') {
			FILE *fd = fopen(argv1, "r");
			int sz;
			char *buffer;

			if (fd == nullptr) {
				errx(1, "couldn't open '%s'", argv1);
			}

			fseek(fd, 0, SEEK_END);
			sz = ftell(fd);
			fseek(fd, 0, SEEK_SET);
			buffer = (char *)malloc(sz + 1);

			if (buffer == nullptr) {
				errx(1, "not enough memory memory");
			}

			fread(buffer, sz, 1, fd);
			/* terminate the string */
			buffer[sz] = 0;
			play_string(buffer, true);
		}

		/* if it looks like a PLAY string... */
		if (strlen(argv1) > 2) {
			if (*argv1 == 'M') {
				play_string(argv1, false);
			}
		}

	}

	errx(1, "unrecognized command, try 'start', 'stop', an alarm number or name, or a file name starting with a '/'");
}
#endif /* TONE_ALARM_CHANNEL */
