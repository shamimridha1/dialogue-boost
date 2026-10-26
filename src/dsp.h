// Dialogue Boost DSP: HPF + dialogue EQ (scaled by Clarity) -> compressor -> boost gain -> lookahead limiter
#pragma once
#include <cmath>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#include <vector>
#include <algorithm>

namespace dlb {

static inline double dbToLin (double db) { return std::pow (10.0, db / 20.0); }
static inline double linToDb (double l) { return 20.0 * std::log10 (std::max (l, 1e-12)); }

struct Biquad
{
	double b0 {1}, b1 {0}, b2 {0}, a1 {0}, a2 {0};
	double z1[8] {}, z2[8] {};

	void reset () { std::fill (z1, z1 + 8, 0.0); std::fill (z2, z2 + 8, 0.0); }

	inline double process (double x, int ch)
	{
		double y = b0 * x + z1[ch];
		z1[ch] = b1 * x - a1 * y + z2[ch];
		z2[ch] = b2 * x - a2 * y;
		return y;
	}

	void setNorm (double B0, double B1, double B2, double A0, double A1, double A2)
	{
		b0 = B0 / A0; b1 = B1 / A0; b2 = B2 / A0; a1 = A1 / A0; a2 = A2 / A0;
	}

	// RBJ cookbook
	void highpass (double fs, double f, double q)
	{
		double w = 2 * M_PI * f / fs, c = std::cos (w), s = std::sin (w), al = s / (2 * q);
		setNorm ((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + al, -2 * c, 1 - al);
	}
	void peak (double fs, double f, double q, double db)
	{
		double A = std::pow (10.0, db / 40.0), w = 2 * M_PI * f / fs, c = std::cos (w),
		       s = std::sin (w), al = s / (2 * q);
		setNorm (1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A);
	}
	void lowShelf (double fs, double f, double db)
	{
		double A = std::pow (10.0, db / 40.0), w = 2 * M_PI * f / fs, c = std::cos (w),
		       s = std::sin (w), al = s / 2 * std::sqrt (2.0), sa = 2 * std::sqrt (A) * al;
		setNorm (A * ((A + 1) - (A - 1) * c + sa), 2 * A * ((A - 1) - (A + 1) * c),
		         A * ((A + 1) - (A - 1) * c - sa), (A + 1) + (A - 1) * c + sa,
		         -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - sa);
	}
	void highShelf (double fs, double f, double db)
	{
		double A = std::pow (10.0, db / 40.0), w = 2 * M_PI * f / fs, c = std::cos (w),
		       s = std::sin (w), al = s / 2 * std::sqrt (2.0), sa = 2 * std::sqrt (A) * al;
		setNorm (A * ((A + 1) + (A - 1) * c + sa), -2 * A * ((A - 1) + (A + 1) * c),
		         A * ((A + 1) + (A - 1) * c - sa), (A + 1) - (A - 1) * c + sa,
		         2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - sa);
	}
};

class Processor
{
public:
	// user parameters
	double boostDb {6.0};    // 0..+12
	double clarity {1.0};    // 0..2 (EQ amount)
	double ceilingDb {-1.5}; // -6..0

	double grDb {0.0}; // last block max total gain reduction (for display)

	void prepare (double sampleRate, int channels)
	{
		fs = sampleRate;
		nch = std::min (std::max (channels, 1), 8);
		lookahead = std::max (1, (int)std::lround (0.0015 * fs)); // 1.5 ms
		win = lookahead + 1;
		delay.assign (nch, std::vector<double> (win, 0.0));
		holdBuf.assign (win, 1.0);
		boxBuf.assign (win, 1.0);
		boxSum = win;
		pos = 0;
		relEnv = 1.0;
		compEnvDb = -120.0;
		compAtt = std::exp (-1.0 / (0.010 * fs));
		compRel = std::exp (-1.0 / (0.150 * fs));
		limRel = 1.0 - std::exp (-1.0 / (0.100 * fs));
		lastClarity = -1;
		updateFilters ();
		reset ();
	}

	int latency () const { return lookahead; }

	void reset ()
	{
		hpf.reset (); ls.reset (); mud.reset (); pres.reset (); air.reset ();
		for (auto& d : delay) std::fill (d.begin (), d.end (), 0.0);
		std::fill (holdBuf.begin (), holdBuf.end (), 1.0);
		std::fill (boxBuf.begin (), boxBuf.end (), 1.0);
		boxSum = win;
		relEnv = 1.0;
		compEnvDb = -120.0;
		pos = 0;
	}

	void updateFilters ()
	{
		if (clarity == lastClarity) return;
		lastClarity = clarity;
		hpf.highpass (fs, 80.0, 0.7071);
		ls.lowShelf (fs, 118.0, -2.0 * clarity);
		mud.peak (fs, 250.0, 1.1, -2.5 * clarity);
		pres.peak (fs, 3000.0, 1.1, 4.0 * clarity);
		air.highShelf (fs, 6000.0, 2.0 * clarity);
	}

	// in/out: arrays of channel pointers (float). in may equal out.
	template <typename T>
	void process (T** in, T** out, int channels, int frames)
	{
		updateFilters ();
		const int C = std::min (channels, nch);
		const double boost = dbToLin (boostDb);
		const double ceil = dbToLin (ceilingDb);
		double maxGr = 0.0;
		double x[8];

		for (int i = 0; i < frames; ++i)
		{
			// 1) EQ
			double peakIn = 0.0;
			for (int c = 0; c < C; ++c)
			{
				double s = in[c][i];
				s = hpf.process (s, c);
				s = ls.process (s, c);
				s = mud.process (s, c);
				s = pres.process (s, c);
				s = air.process (s, c);
				x[c] = s;
				peakIn = std::max (peakIn, std::fabs (s));
			}

			// 2) compressor (linked, peak detector, -18 dB, 2:1, 6 dB knee)
			double lvl = linToDb (peakIn);
			double coef = lvl > compEnvDb ? compAtt : compRel;
			compEnvDb = lvl + coef * (compEnvDb - lvl);
			const double thr = -18.0, ratio = 2.0, knee = 6.0;
			double over = compEnvDb - thr, grComp = 0.0;
			if (over > knee / 2)
				grComp = over * (1.0 - 1.0 / ratio);
			else if (over > -knee / 2)
				grComp = (1.0 - 1.0 / ratio) * (over + knee / 2) * (over + knee / 2) / (2 * knee);
			double g = dbToLin (-grComp) * boost;

			// 3) required limiter gain
			double peak = 0.0;
			for (int c = 0; c < C; ++c)
			{
				x[c] *= g;
				peak = std::max (peak, std::fabs (x[c]));
			}
			double req = peak > ceil ? ceil / peak : 1.0;

			// sliding min over window (O(win), window is ~72 samples)
			holdBuf[pos] = req;
			double hold = 1.0;
			for (int k = 0; k < win; ++k) hold = std::min (hold, holdBuf[k]);

			// release envelope that never exceeds hold
			if (hold < relEnv) relEnv = hold;
			else relEnv += (hold - relEnv) * limRel;

			// box filter -> smooth gain that is <= required gain at the delayed sample
			boxSum += relEnv - boxBuf[pos];
			boxBuf[pos] = relEnv;
			double lg = boxSum / win;

			// delay audio by lookahead and apply
			int rd = (pos + 1) % win; // oldest sample
			for (int c = 0; c < C; ++c)
			{
				double delayed = delay[c][rd];
				delay[c][pos] = x[c];
				double y = delayed * lg;
				if (y > ceil) y = ceil; // numeric safety
				if (y < -ceil) y = -ceil;
				out[c][i] = (T)y;
			}
			for (int c = C; c < channels; ++c) out[c][i] = 0;

			double totalGr = grComp - linToDb (lg);
			maxGr = std::max (maxGr, totalGr);
			pos = rd;
		}
		grDb = maxGr;
	}

private:
	double fs {48000};
	int nch {2};
	int lookahead {72}, win {73}, pos {0};
	std::vector<std::vector<double>> delay;
	std::vector<double> holdBuf, boxBuf;
	double boxSum {0}, relEnv {1}, limRel {0};
	double compEnvDb {0}, compAtt {0}, compRel {0};
	double lastClarity {-1};
	Biquad hpf, ls, mud, pres, air;
};

} // namespace dlb
