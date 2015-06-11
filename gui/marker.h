/*******************************************************************************
 *
 * This software was developed at the National Institute of Standards and
 * Technology (NIST) by employees of the Federal Government in the course
 * of their official duties. Pursuant to title 17 Section 105 of the
 * United States Code, this software is not subject to copyright protection
 * and is in the public domain. NIST assumes no responsibility whatsoever for
 * its use by other parties, and makes no guarantees, expressed or implied,
 * about its quality, reliability, or any other characteristic.
 *
 * Author(s):
 *      Martin Shetty (NIST)
 *
 * Description:
 *      Marker - for marking a channel or energt in a plot
 *
 ******************************************************************************/

#ifndef Q_MARKER_H
#define Q_MARKER_H

#include "detector.h"

struct Marker {
  double energy, channel;
  uint16_t bits;
  QMap<QString, QPen> themes;
  QPen default_pen;
  bool visible;
  bool chan_valid, energy_valid;

  Marker() : energy(0), channel(0), bits(0), visible(false), default_pen(Qt::gray, 1, Qt::SolidLine), chan_valid(false), energy_valid(false) {}

  void shift(uint16_t to_bits) {
    if (!to_bits || !bits)
      return;

    if (bits > to_bits)
      channel = channel / pow(2, bits - to_bits);
    if (bits < to_bits)
      channel = channel * pow(2, to_bits - bits);

    bits = to_bits;
  }

  void calibrate(Pixie::Detector det) {
    if (det.energy_calibrations_.has_a(Pixie::Calibration(bits))) {
      energy = det.energy_calibrations_.get(Pixie::Calibration(bits)).transform(channel);
      energy_valid = true;
    }
  }

};

#endif