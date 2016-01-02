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
 *      Qpx::Spectrum::Spectrum generic spectrum type.
 *                       All public methods are thread-safe.
 *                       When deriving override protected methods.
 *
 ******************************************************************************/

#include <fstream>
#include <boost/algorithm/string.hpp>
#include "spectrum.h"
#include "custom_logger.h"
#include "xmlable.h"

namespace Qpx {
namespace Spectrum {

Template Spectrum::get_template() {
  Template new_temp;

  Gamma::Setting ignore_zero;
  ignore_zero.id_ = "cutoff_logic";
  ignore_zero.metadata.setting_type = Gamma::SettingType::integer;
  ignore_zero.metadata.description = "Hits rejected below minimum energy (affects coincidence logic and binning)";
  ignore_zero.metadata.writable = true;
  ignore_zero.metadata.minimum = 0;
  ignore_zero.metadata.step = 1;
  ignore_zero.metadata.maximum = 1000000;
  new_temp.generic_attributes.add(ignore_zero);

  Gamma::Setting coinc_window;
  coinc_window.id_ = "coinc_window";
  coinc_window.metadata.setting_type = Gamma::SettingType::floating;
  coinc_window.metadata.minimum = 0;
  coinc_window.metadata.step = 1;
  coinc_window.metadata.maximum = 1000000;
  coinc_window.metadata.unit = "ticks";
  coinc_window.metadata.description = "Coincidence window";
  coinc_window.metadata.writable = true;
  coinc_window.value_dbl = 3.0;
  new_temp.generic_attributes.add(coinc_window);

  return new_temp;
}

bool Spectrum::initialize() {
  cutoff_logic_ = get_attr("cutoff_logic").value_int;

  coinc_window_ = get_attr("coinc_window").value_dbl;

  if (coinc_window_ < 0)
    coinc_window_ = 0;

  return false;
}


PreciseFloat Spectrum::get_count(std::initializer_list<uint16_t> list ) const {
  boost::shared_lock<boost::shared_mutex> lock(mutex_);
  if (list.size() != this->metadata_.dimensions)
    return 0;
  return this->_get_count(list);
}

std::unique_ptr<std::list<Entry>> Spectrum::get_spectrum(std::initializer_list<Pair> list) {
  boost::shared_lock<boost::shared_mutex> lock(mutex_);
  if (list.size() != this->metadata_.dimensions)
    return 0; //wtf???
  else {
    //    std::vector<Pair> ranges(list.begin(), list.end());
    return this->_get_spectrum(list);
  }
}

void Spectrum::add_bulk(const Entry& e) {
  boost::shared_lock<boost::shared_mutex> lock(mutex_);
  if (metadata_.dimensions < 1)
    return;
  else
    this->_add_bulk(e);
}

bool Spectrum::from_template(const Template& newtemplate) {
  boost::unique_lock<boost::mutex> uniqueLock(u_mutex_, boost::defer_lock);
  while (!uniqueLock.try_lock())
    boost::this_thread::sleep_for(boost::chrono::seconds{1});
  
  metadata_.bits = newtemplate.bits;
  shift_by_ = 16 - metadata_.bits;
  metadata_.resolution = pow(2,metadata_.bits);
  metadata_.name = newtemplate.name_;
  metadata_.appearance = newtemplate.appearance;
  metadata_.visible = newtemplate.visible;
  metadata_.match_pattern = newtemplate.match_pattern;
  metadata_.add_pattern = newtemplate.add_pattern;
  metadata_.attributes = newtemplate.generic_attributes;

  return (this->initialize());
}

void Spectrum::addSpill(const Spill& one_spill) {
  boost::unique_lock<boost::mutex> uniqueLock(u_mutex_, boost::defer_lock);
  while (!uniqueLock.try_lock())
    boost::this_thread::sleep_for(boost::chrono::seconds{1});

  for (auto &q : one_spill.hits)
    this->pushHit(q);

  for (auto &q : one_spill.stats) {
    if (q.stats_type == StatsType::stop) {
//      PL_DBG << "<" << metadata_.name << "> final RunInfo received, dumping backlog of events " << backlog.size();
      while (!backlog.empty()) {
        recent_count_++;
        metadata_.total_count++;
        this->addEvent(backlog.front());
        backlog.pop_front();
      }
    }
    this->addStats(q);
  }

  if (one_spill.run != RunInfo())
    this->addRun(one_spill.run);

//  PL_DBG << "<" << metadata_.name << "> left in backlog " << backlog.size();
}

void Spectrum::pushHit(const Hit& newhit)
{
  if (newhit.energy < cutoff_logic_)
    return;
//  PL_DBG << "Processing " << newhit.to_string();

  bool appended = false;
  bool pileup = false;
  if (backlog.empty()) {
    backlog.push_back(Event(newhit, coinc_window_));
//    PL_DBG << "add to empty";
  } else if (!backlog.back().in_window(newhit.timestamp)) {
    backlog.push_back(Event(newhit, coinc_window_));
//    PL_DBG << "add out of window";
  } else {
    for (auto &q : backlog) {
      if (q.in_window(newhit.timestamp)) {
        if ((q.hit.count(newhit.channel) == 0) && !appended) {
//          PL_DBG << "coinc 1";
          q.addHit(newhit);
          appended = true;
        } else if ((q.hit.count(newhit.channel) == 0) && appended) {
          q.addHit(newhit);
          PL_DBG << "<" << metadata_.name << "> hit " << newhit.to_string() << " coincident with more than one other hit (counted >=2 times)";
        } else {
          PL_DBG << "<" << metadata_.name << "> pileup hit " << newhit.to_string() << " with " << q.to_string() << " already has " << q.hit[newhit.channel].to_string();
          //PL_DBG << "Logical pileup detected. Event may be discarded";
          pileup = true;
        }
      } /*else
        break;*/
    }

    if (!appended && !pileup) {
      backlog.push_back(Event(newhit, coinc_window_));
//      PL_DBG << "append fresh";
    }
  }

  if (!backlog.empty()) {
    Event evt = backlog.front();
    while ((newhit.timestamp - evt.upper_time) > (coinc_window_*2)) {
//      PL_DBG << "in while loop";

      if (validateEvent(evt)) {
//        PL_DBG << "validated " << evt.to_string();
        recent_count_++;
        metadata_.total_count++;
        this->addEvent(evt);
      }
      backlog.pop_front();
      if (!backlog.empty())
        evt = backlog.front();
      else
        break;
    }
  }

}

void Spectrum::closeAcquisition() {
  boost::unique_lock<boost::mutex> uniqueLock(u_mutex_, boost::defer_lock);
  while (!uniqueLock.try_lock())
    boost::this_thread::sleep_for(boost::chrono::seconds{1});
  _closeAcquisition();
}


bool Spectrum::validateEvent(const Event& newEvent) const {
  bool addit = true;
  for (int i = 0; i < metadata_.match_pattern.size(); i++)
    if (((metadata_.match_pattern[i] == 1) && (newEvent.hit.count(i) == 0)) ||
        ((metadata_.match_pattern[i] == -1) && (newEvent.hit.count(i) > 0)))
      addit = false;
  return addit;
}

std::map<int, std::list<StatsUpdate>> Spectrum::get_stats() {
  return stats_list_;
}


void Spectrum::addStats(const StatsUpdate& newBlock) {
  //private; no lock required

  if ((newBlock.channel >= 0)
      && (newBlock.channel < metadata_.add_pattern.size())
      && (newBlock.channel < metadata_.match_pattern.size())
      && (metadata_.add_pattern[newBlock.channel] || metadata_.match_pattern[newBlock.channel])) {
    //PL_DBG << "Spectrum " << metadata_.name << " received update for chan " << newBlock.channel;
    bool chan_new = (stats_list_.count(newBlock.channel) == 0);
    bool new_start = (newBlock.stats_type == StatsType::start);
    if (!chan_new && new_start && (stats_list_[newBlock.channel].back().stats_type == StatsType::running))
      stats_list_[newBlock.channel].back().stats_type = StatsType::stop;

    if (metadata_.recent_end.lab_time.is_not_a_date_time())
      metadata_.recent_start = newBlock;
    else
      metadata_.recent_start = metadata_.recent_end;

    metadata_.recent_end = newBlock;
    metadata_.recent_count = recent_count_;
//    PL_DBG << "<Spectrum> \"" << metadata_.name << "\" recent count_ = " << recent_count_;
    recent_count_ = 0;

    if (!chan_new && (stats_list_[newBlock.channel].back().stats_type == StatsType::running))
      stats_list_[newBlock.channel].pop_back();

    stats_list_[newBlock.channel].push_back(newBlock);

    if (!chan_new) {
      StatsUpdate start = stats_list_[newBlock.channel].front();

      boost::posix_time::time_duration rt ,lt;
      boost::posix_time::time_duration real ,live;

      for (auto &q : stats_list_[newBlock.channel]) {
        if (q.stats_type == StatsType::start) {
          real += rt;
          live += lt;
          start = q;
//          PL_DBG << "<Spectrum> \"" << metadata_.name << "\" RT + "
//                 << rt << " = " << real << "   LT + " << lt << " = " << live;
//          PL_DBG << "<Spectrum> \"" << metadata_.name << "\" FROM "
//                 << boost::posix_time::to_iso_extended_string(start.lab_time);
        } else {
          lt = rt = q.lab_time - start.lab_time;
          StatsUpdate diff = q - start;
//          PL_DBG << "<Spectrum> \"" << metadata_.name << "\" TOTAL_TIME " << q.total_time << "-" << start.total_time
//                 << " = " << diff.total_time;
          if (diff.total_time > 0) {
            double scale_factor = rt.total_microseconds() / diff.total_time;
            double this_time_unscaled = diff.live_time - diff.sfdt;
            lt = boost::posix_time::microseconds(static_cast<long>(this_time_unscaled * scale_factor));
//            PL_DBG << "<Spectrum> \"" << metadata_.name << "\" TO "
//                   << boost::posix_time::to_iso_extended_string(q.lab_time)
//                   << "  RT = " << rt << "   LT = " << lt;
          }
        }
      }

      if (stats_list_[newBlock.channel].back().stats_type != StatsType::start) {
        real += rt;
        live += lt;
//        PL_DBG << "<Spectrum> \"" << metadata_.name << "\" RT + "
//               << rt << " = " << real << "   LT + " << lt << " = " << live;
      }
      real_times_[newBlock.channel] = real;
      live_times_[newBlock.channel] = live;

      metadata_.live_time = metadata_.real_time = real;
      for (auto &q : real_times_)
        if (q.second.total_milliseconds() < metadata_.real_time.total_milliseconds())
          metadata_.real_time = q.second;

      for (auto &q : live_times_)
        if (q.second.total_milliseconds() < metadata_.live_time.total_milliseconds())
          metadata_.live_time = q.second;

//      PL_DBG << "<Spectrum> \"" << metadata_.name << "\"  ********* "
//             << "RT = " << to_simple_string(metadata_.real_time)
//             << "   LT = " << to_simple_string(metadata_.live_time) ;

    }
  }
}


void Spectrum::addRun(const RunInfo& run_info) {
  //private; no lock required

  if (metadata_.start_time.is_not_a_date_time() && (!run_info.time_start.is_not_a_date_time())) {
    metadata_.start_time = run_info.time_start;
  }

 if (run_info.detectors.size() > 0)
    this->_set_detectors(run_info.detectors);

}

std::vector<double> Spectrum::energies(uint8_t chan) const {
  boost::shared_lock<boost::shared_mutex> lock(mutex_);
  
  if (chan < metadata_.dimensions)
    return energies_[chan];
  else
    return std::vector<double>();
}
  
void Spectrum::set_detectors(const std::vector<Gamma::Detector>& dets) {
  boost::unique_lock<boost::mutex> uniqueLock(u_mutex_, boost::defer_lock);
  while (!uniqueLock.try_lock())
    boost::this_thread::sleep_for(boost::chrono::seconds{1});
  
  this->_set_detectors(dets);
}

void Spectrum::_set_detectors(const std::vector<Gamma::Detector>& dets) {
  //private; no lock required
//  PL_DBG << "<Spectrum> _set_detectors";

  metadata_.detectors.clear();

  //metadata_.detectors.resize(metadata_.dimensions, Gamma::Detector());
  recalc_energies();
}

void Spectrum::recalc_energies() {
  //private; no lock required

  energies_.resize(metadata_.dimensions);
  if (energies_.size() != metadata_.detectors.size())
    return;

  for (int i=0; i < metadata_.detectors.size(); ++i) {
    energies_[i].resize(metadata_.resolution, 0.0);
     Gamma::Calibration this_calib;
    if (metadata_.detectors[i].energy_calibrations_.has_a(Gamma::Calibration("Energy", metadata_.bits)))
      this_calib = metadata_.detectors[i].energy_calibrations_.get(Gamma::Calibration("Energy", metadata_.bits));
    else
      this_calib = metadata_.detectors[i].highest_res_calib();
    for (uint32_t j=0; j<metadata_.resolution; j++)
      energies_[i][j] = this_calib.transform(j, metadata_.bits);
  }
}

Gamma::Setting Spectrum::get_attr(std::string setting) const {
  return metadata_.attributes.get(Gamma::Setting(setting));
}



bool Spectrum::write_file(std::string dir, std::string format) const {
  boost::shared_lock<boost::shared_mutex> lock(mutex_);
  return _write_file(dir, format);
}

bool Spectrum::read_file(std::string name, std::string format) {
  boost::unique_lock<boost::mutex> uniqueLock(u_mutex_, boost::defer_lock);
  while (!uniqueLock.try_lock())
    boost::this_thread::sleep_for(boost::chrono::seconds{1});
  metadata_.attributes = this->default_settings();
  return _read_file(name, format);
}

std::string Spectrum::channels_to_xml() const {
  boost::shared_lock<boost::shared_mutex> lock(mutex_);
  return _channels_to_xml();
}

uint16_t Spectrum::channels_from_xml(const std::string& str) {
  boost::unique_lock<boost::mutex> uniqueLock(u_mutex_, boost::defer_lock);
  while (!uniqueLock.try_lock())
    boost::this_thread::sleep_for(boost::chrono::seconds{1});
  return _channels_from_xml(str);
}


//accessors for various properties
Metadata Spectrum::metadata() const {
  boost::shared_lock<boost::shared_mutex> lock(mutex_);
  return metadata_;  
}

std::string Spectrum::type() const {
  boost::shared_lock<boost::shared_mutex> lock(mutex_);
  return my_type();
}

uint16_t Spectrum::dimensions() const {
  boost::shared_lock<boost::shared_mutex> lock(mutex_);
  return metadata_.dimensions;
}

uint32_t Spectrum::resolution() const {
  boost::shared_lock<boost::shared_mutex> lock(mutex_);
  return metadata_.resolution;
}

std::string Spectrum::name() const {
  boost::shared_lock<boost::shared_mutex> lock(mutex_);
  return metadata_.name;
}

uint16_t Spectrum::bits() const {
  boost::shared_lock<boost::shared_mutex> lock(mutex_);
  return metadata_.bits;
}


//change stuff

void Spectrum::set_visible(bool vis) {
  boost::unique_lock<boost::mutex> uniqueLock(u_mutex_, boost::defer_lock);
  while (!uniqueLock.try_lock())
    boost::this_thread::sleep_for(boost::chrono::seconds{1});
  metadata_.visible = vis;
}

void Spectrum::set_appearance(uint32_t newapp) {
  boost::unique_lock<boost::mutex> uniqueLock(u_mutex_, boost::defer_lock);
  while (!uniqueLock.try_lock())
    boost::this_thread::sleep_for(boost::chrono::seconds{1});
  metadata_.appearance = newapp;
}

void Spectrum::set_start_time(boost::posix_time::ptime newtime) {
  boost::unique_lock<boost::mutex> uniqueLock(u_mutex_, boost::defer_lock);
  while (!uniqueLock.try_lock())
    boost::this_thread::sleep_for(boost::chrono::seconds{1});
  metadata_.start_time = newtime;
}

void Spectrum::set_real_time(boost::posix_time::time_duration tm) {
  boost::unique_lock<boost::mutex> uniqueLock(u_mutex_, boost::defer_lock);
  while (!uniqueLock.try_lock())
    boost::this_thread::sleep_for(boost::chrono::seconds{1});
  metadata_.real_time = tm;
}

void Spectrum::set_live_time(boost::posix_time::time_duration tm) {
  boost::unique_lock<boost::mutex> uniqueLock(u_mutex_, boost::defer_lock);
  while (!uniqueLock.try_lock())
    boost::this_thread::sleep_for(boost::chrono::seconds{1});
  metadata_.live_time = tm;
}

void Spectrum::set_description(std::string newdesc) {
  boost::unique_lock<boost::mutex> uniqueLock(u_mutex_, boost::defer_lock);
  while (!uniqueLock.try_lock())
    boost::this_thread::sleep_for(boost::chrono::seconds{1});
  metadata_.description = newdesc;
}

void Spectrum::set_generic_attr(Gamma::Setting setting) {
  boost::unique_lock<boost::mutex> uniqueLock(u_mutex_, boost::defer_lock);
  while (!uniqueLock.try_lock())
    boost::this_thread::sleep_for(boost::chrono::seconds{1});
  for (auto &q : metadata_.attributes.my_data_) {
    if ((q.id_ == setting.id_) && (q.metadata.writable))
      q = setting;
  }
}

void Spectrum::set_rescale_factor(PreciseFloat newfactor) {
  boost::unique_lock<boost::mutex> uniqueLock(u_mutex_, boost::defer_lock);
  while (!uniqueLock.try_lock())
    boost::this_thread::sleep_for(boost::chrono::seconds{1});
  metadata_.rescale_factor = newfactor;
}





/////////////////////
//Save and load//////
/////////////////////

void Spectrum::to_xml(pugi::xml_node &root) const {
  boost::shared_lock<boost::shared_mutex> lock(mutex_);

  pugi::xml_node node = root.append_child("Spectrum");

  node.append_attribute("type").set_value(this->my_type().c_str());

  node.append_child("Name").append_child(pugi::node_pcdata).set_value(metadata_.name.c_str());
  node.append_child("TotalEvents").append_child(pugi::node_pcdata).set_value(metadata_.total_count.str().c_str());
  node.append_child("Appearance").append_child(pugi::node_pcdata).set_value(std::to_string(metadata_.appearance).c_str());
  node.append_child("Visible").append_child(pugi::node_pcdata).set_value(std::to_string(metadata_.visible).c_str());
  node.append_child("Resolution").append_child(pugi::node_pcdata).set_value(std::to_string(metadata_.bits).c_str());

  std::stringstream patterndata;
  for (auto &q: metadata_.match_pattern)
    patterndata << static_cast<short>(q) << " ";
  node.append_child("MatchPattern").append_child(pugi::node_pcdata).set_value(boost::algorithm::trim_copy(patterndata.str()).c_str());

  patterndata.str(std::string()); //clear it
  for (auto &q: metadata_.add_pattern)
    patterndata << static_cast<short>(q) << " ";
  node.append_child("AddPattern").append_child(pugi::node_pcdata).set_value(boost::algorithm::trim_copy(patterndata.str()).c_str());

  node.append_child("RealTime").append_child(pugi::node_pcdata).set_value(to_simple_string(metadata_.real_time).c_str());
  node.append_child("LiveTime").append_child(pugi::node_pcdata).set_value(to_simple_string(metadata_.live_time).c_str());

  if (metadata_.attributes.size())
    metadata_.attributes.to_xml(node);

  if (metadata_.detectors.size()) {
    pugi::xml_node child = node.append_child("Detectors");
    for (auto q : metadata_.detectors) {
      q.settings_ = Gamma::Setting();
      q.to_xml(child);
    }
  }

  if ((metadata_.resolution > 0) && (metadata_.total_count > 0))
    node.append_child("ChannelData").append_child(pugi::node_pcdata).set_value(this->_channels_to_xml().c_str());
}


bool Spectrum::from_xml(const pugi::xml_node &node) {
  if (std::string(node.name()) != "Spectrum")
    return false;

  boost::unique_lock<boost::mutex> uniqueLock(u_mutex_, boost::defer_lock);
  while (!uniqueLock.try_lock())
    boost::this_thread::sleep_for(boost::chrono::seconds{1});

  std::string numero;

  if (!node.child("Name"))
    return false;
  metadata_.name = std::string(node.child_value("Name"));

  metadata_.total_count = PreciseFloat(node.child_value("TotalEvents"));
  metadata_.appearance = boost::lexical_cast<unsigned int>(std::string(node.child_value("Appearance")));
  metadata_.visible = boost::lexical_cast<bool>(std::string(node.child_value("Visible")));
  metadata_.bits = boost::lexical_cast<short>(std::string(node.child_value("Resolution")));

  if ((!metadata_.bits) || (metadata_.total_count == 0))
    return false;

  shift_by_ = 16 - metadata_.bits;
  metadata_.resolution = pow(2, metadata_.bits);
  metadata_.match_pattern.clear();
  metadata_.add_pattern.clear();

  std::stringstream pattern_match(node.child_value("MatchPattern"));
  while (pattern_match.rdbuf()->in_avail()) {
    pattern_match >> numero;
    metadata_.match_pattern.push_back(boost::lexical_cast<short>(boost::algorithm::trim_copy(numero)));
  }

  std::stringstream pattern_add(node.child_value("AddPattern"));
  while (pattern_add.rdbuf()->in_avail()) {
    pattern_add >> numero;
    metadata_.add_pattern.push_back(boost::lexical_cast<short>(boost::algorithm::trim_copy(numero)));
  }

  if (node.child("RealTime"))
    metadata_.real_time = boost::posix_time::duration_from_string(node.child_value("RealTime"));
  if (node.child("LiveTime"))
    metadata_.live_time = boost::posix_time::duration_from_string(node.child_value("LiveTime"));

  metadata_.attributes.from_xml(node.child(metadata_.attributes.xml_element_name().c_str()));

  if (node.child("Detectors")) {
    metadata_.detectors.clear();
    for (auto &q : node.child("Detectors").children()) {
      Gamma::Detector det;
      det.from_xml(q);
      metadata_.detectors.push_back(det);
    }
  }

  std::string this_data(node.child_value("ChannelData"));
  boost::algorithm::trim(this_data);
  this->_channels_from_xml(this_data);

  bool ret = this->initialize();

  if (ret)
   this->recalc_energies();

  return ret;
}


}}
