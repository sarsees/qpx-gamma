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
 * This software can be redistributed and/or modified freely provided that
 * any derivative works bear some notice that they are derived from it, and
 * any modified versions bear some notice that they have been modified.
 *
 * Author(s):
 *      Martin Shetty (NIST)
 *
 * Description:
 *      Pixie::Spectrum::SpectrumRaw to output custom list mode to file
 *
 ******************************************************************************/


#include <fstream>
#include <algorithm>
#include <boost/algorithm/string.hpp>
#include <boost/date_time/local_time/local_time.hpp>
#include <boost/filesystem.hpp>
#include "spectrum_raw.h"
#include <utility>

namespace Pixie {
namespace Spectrum {

static Registrar<SpectrumRaw> registrar("Raw");

SpectrumRaw::~SpectrumRaw() {
  _closeAcquisition();
}

bool SpectrumRaw::initialize() {
  format_ = get_attr("format").value_int;
  file_dir_ = get_attr("file_dir").value_text;
  with_hit_pattern_ = get_attr("with_pattern").value_int;
  if (file_dir_.empty())
    return false;

  metadata_.type = my_type();

  if (format_ == 0)
    return init_bin();
  if (format_ == 1)
    return init_text();
}

bool SpectrumRaw::init_text() {
  file_name_txt_ = file_dir_ + "/qpx_out.txt";

  file_xml_ = fopen(file_name_txt_.c_str(), "w");
  if (file_xml_ == nullptr)
    return false;

  xml_printer_ = new tinyxml2::XMLPrinter(file_xml_);
  xml_printer_->PushHeader(true, true);

  xml_printer_->OpenElement("QpxListData");

  std::stringstream ss;
  xml_printer_->OpenElement("MatchPattern");
  for (int i = 0; i < kNumChans; i++)
    ss << metadata_.match_pattern[i] << " ";
  xml_printer_->PushText(boost::algorithm::trim_copy(ss.str()).c_str());
  xml_printer_->CloseElement();

  open_xml_ = true;
  return true;
}

bool SpectrumRaw::init_bin() {
  file_name_bin_ = file_dir_ + "/qpx_out.bin";

  file_bin_.open(file_name_bin_, std::ofstream::out | std::ofstream::trunc | std::ofstream::binary);

  if (!file_bin_.is_open())
    return false;

  if (!file_bin_.good() || !init_text()) {
    file_bin_.close();
    return false;
  }

  xml_printer_->OpenElement("BinaryOut");

  xml_printer_->OpenElement("FileName");
  xml_printer_->PushText(file_name_bin_.c_str());
  xml_printer_->CloseElement();

  xml_printer_->OpenElement("FileFormat");
  xml_printer_->PushText("CHANNEL_NUMBER  TIME_HI  TIME_MID  TIME_LOW  ENERGY");
  xml_printer_->CloseElement();

  xml_printer_->CloseElement();

  open_bin_ = true;
  return true;

  /*  std::string header("qpx_list/Pixie-4/");
  out_file_.write(header.data(), header.size());
  if (with_hit_pattern_) {
    std::string hp("with_pattern/");
    out_file_.write(hp.data(), hp.size());
  } */
}

std::unique_ptr<std::list<Entry>> SpectrumRaw::_get_spectrum(std::initializer_list<Pair> list) {
  std::unique_ptr<std::list<Entry>> result(new std::list<Entry>);
  return result;
}

void SpectrumRaw::addHit(const Hit& newHit) {
  if (format_ == 0)
    hit_bin(newHit);
  else if (format_ == 1)
    hit_text(newHit);
}

void SpectrumRaw::addStats(const StatsUpdate& stats) {
  if (format_ == 0) {
    //override, because coincident events have been split (one for each channel)
    StatsUpdate stats_override = stats;
    stats_override.events_in_spill = events_this_spill_;
    total_events_ += events_this_spill_;
    events_this_spill_ = 0;
    stats_text(stats_override);
  } else if (format_ == 1)
    stats_text(stats);
}

void SpectrumRaw::addRun(const RunInfo& run) {
  if (format_ == 0) {
    //override, because coincident events have been split (one for each channel)
    RunInfo run_override = run;
    run_override.total_events = total_events_;
    total_events_ = 0;
    run_text(run_override);
  } else if (format_ == 1)
    run_text(run);
}

void SpectrumRaw::_closeAcquisition() {
  if (open_xml_) {
    PL_DBG << "<SpectrumRaw> closing " << file_name_txt_ << " for " << metadata_.name;
    xml_printer_->CloseElement(); //QpxListData
    fclose(file_xml_);
    open_xml_ = false;
  }
  if (open_bin_) {
    PL_DBG << "<SpectrumRaw> closing " << file_name_bin_ << " for " << metadata_.name;
    file_bin_.close();
    open_bin_ = false;
  }
}


void SpectrumRaw::hit_text(const Hit &newHit) {
  if (!open_xml_)
    return;
  newHit.to_xml(*xml_printer_);
}

void SpectrumRaw::hit_bin(const Hit &newHit) {
  if (!open_bin_)
    return;

  std::vector<uint16_t> energy(kNumChans);
  std::vector<uint16_t> lo(kNumChans);
  std::vector<uint16_t> mi(kNumChans);
  std::vector<uint16_t> hi(kNumChans);
  std::vector<uint64_t> time(kNumChans);
  std::multimap<uint64_t, uint16_t> order;

  for (uint16_t i = 0; i < kNumChans; i++) {
    if (newHit.pattern[i]) {
    energy[i] = newHit.energy[i];
    hi[i] = newHit.buf_time_hi;
    mi[i] = newHit.evt_time_hi;
    lo[i] = newHit.evt_time_lo;
    if (newHit.chan_trig_time[i] > newHit.evt_time_lo)
      mi[i]--;
    if (newHit.evt_time_hi < newHit.buf_time_mi)
      hi[i]++;
    uint16_t task_b = (newHit.run_type & 0x000F);
    if ((task_b == 0x0000) || (task_b == 0x0001))
      hi[i] = newHit.chan_real_time[i];
    lo[i] = newHit.chan_trig_time[i];
    time[i] = (((uint64_t)hi[i]) << 32) + (((uint64_t)mi[i]) << 16) + ((uint64_t)lo[i]);
    order.insert(std::pair<uint64_t,uint16_t>(time[i], i));
    }
  }

  for (auto &q : order) {
    file_bin_.write((char*)&q.second, sizeof(q.second));
    file_bin_.write((char*)&hi[q.second], sizeof(hi[q.second]));
    file_bin_.write((char*)&mi[q.second], sizeof(mi[q.second]));
    file_bin_.write((char*)&lo[q.second], sizeof(lo[q.second]));
    file_bin_.write((char*)&energy[q.second], sizeof(energy[q.second]));
    events_this_spill_++;
  }
}

void SpectrumRaw::stats_text(const StatsUpdate& stats) {
  if (!open_xml_)
    return;
  stats.to_xml(*xml_printer_);
}


void SpectrumRaw::run_text(const RunInfo& run) {
  if (!open_xml_)
    return;
  run.to_xml(*xml_printer_);
}



}}
