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
 *      Qpx::SinkRaw to output custom list mode to file
 *
 ******************************************************************************/


#include <fstream>
#include <algorithm>
#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include "spectrum_raw.h"
#include "daq_sink_factory.h"

namespace Qpx {

static SinkRegistrar<SpectrumRaw> registrar("Raw");

SpectrumRaw::SpectrumRaw()
  : open_bin_(false)
  , open_xml_(false)
  , events_this_spill_(0)
{
  Setting base_options = metadata_.attributes;
  metadata_ = Metadata("Raw", "Custom gated list mode to file. Please provide path and name for valid and accessible file", 0,
                    {}, {});
  metadata_.attributes = base_options;

  Qpx::Setting file_setting;
  file_setting.id_ = "file_dir";
  file_setting.metadata.setting_type = Qpx::SettingType::dir_path;
  file_setting.metadata.writable = true;
  file_setting.metadata.flags.insert("preset");
  file_setting.metadata.description = "path to temp output directory";
  metadata_.attributes.branches.add(file_setting);
}

SpectrumRaw::~SpectrumRaw() {
  _closeAcquisition();
}

bool SpectrumRaw::initialize() {
  Spectrum::initialize();

  file_dir_ = get_attr("file_dir").value_text;
  with_hit_pattern_ = get_attr("with_pattern").value_int;
  PL_DBG << "trying to create " << file_dir_;
  if (file_dir_.empty())
    return false;
  PL_DBG << "raw: file dir not empty";

  return init_bin();
}

bool SpectrumRaw::init_text() {
  file_name_txt_ = file_dir_ + "/qpx_out.txt";
  xml_root_ = xml_doc_.append_child("QpxListData");

  if (metadata_.attributes.branches.size())
    metadata_.attributes.to_xml(xml_root_);

  open_xml_ = true;
  return true;
}

bool SpectrumRaw::init_bin() {
  file_name_bin_ = file_dir_ + "/qpx_out.bin";

  file_bin_.open(file_name_bin_, std::ofstream::out | std::ofstream::trunc | std::ofstream::binary);

  if (!file_bin_.is_open())
    return false;

  PL_DBG << "successfully opened binary";

  if (!file_bin_.good() || !init_text()) {
    file_bin_.close();
    return false;
  }

  PL_DBG << "binary is good";

  open_bin_ = true;
  return true;

  /*  std::string header("qpx_list/");
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

void SpectrumRaw::addEvent(const Event& newEvent) {
  hit_bin(newEvent);
}

void SpectrumRaw::addStats(const StatsUpdate& stats) {
  //override, because coincident events have been split (one for each channel)
  StatsUpdate stats_override = stats;
  stats_override.events_in_spill = events_this_spill_;
  total_events_ += events_this_spill_;
  events_this_spill_ = 0;
  stats_text(stats_override);
}

void SpectrumRaw::_closeAcquisition() {
  if (open_xml_) {
    PL_DBG << "<SpectrumRaw> writing " << file_name_txt_ << " for \"" << metadata_.name << "\"";
    if (!xml_doc_.save_file(file_name_txt_.c_str()))
      PL_ERR << "<SpectrumRaw> Failed to save " << file_name_txt_;
    open_xml_ = false;
  }
  if (open_bin_) {
    PL_DBG << "<SpectrumRaw> closing " << file_name_bin_ << " for \"" << metadata_.name << "\"";
    file_bin_.close();
    open_bin_ = false;
  }
}


void SpectrumRaw::hit_bin(const Event &newEvent) {
  if (!open_bin_)
    return;

  std::multiset<Hit> all_hits;
  for (auto &q : newEvent.hits)
    all_hits.insert(q.second);

  for (auto &q : all_hits) {
    q.write_bin(file_bin_);
    events_this_spill_++;
  }
}

void SpectrumRaw::stats_text(const StatsUpdate& stats) {
  if (!open_xml_)
    return;
  stats.to_xml(xml_root_);
}

}
