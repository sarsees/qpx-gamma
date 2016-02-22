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
 *      Qpx::SorterEVT
 *
 ******************************************************************************/

#ifndef SORTER_EVT
#define SORTER_EVT

#include "daq_device.h"
#include "detector.h"
#include <boost/thread.hpp>
#include <boost/atomic.hpp>

#include "CFileDataSource.h"

namespace Qpx {

class SorterEVT : public DaqDevice {
  
public:

  SorterEVT();
//  SorterEVT(std::string file);
  ~SorterEVT();

  static std::string plugin_name() {return "SorterEVT";}
  std::string device_name() const override {return plugin_name();}

  bool write_settings_bulk(Gamma::Setting &set) override;
  bool read_settings_bulk(Gamma::Setting &set) const override;
  void get_all_settings() override;
  bool boot() override;
  bool die() override;

  bool daq_start(SynchronizedQueue<Spill*>* out_queue) override;
  bool daq_stop() override;
  bool daq_running() override;

private:
  //no copying
  void operator=(SorterEVT const&);
  SorterEVT(const SorterEVT&);

  //Acquisition threads, use as static functors
  static void worker_run(SorterEVT* callback, SynchronizedQueue<Spill*>* spill_queue);

protected:

  std::string setting_definitions_file_;

  boost::atomic<int> run_status_;
  boost::thread *runner_;

  bool loop_data_;
  bool override_pause_;
  bool override_timestamps_;
  bool bad_buffers_rep_;
  bool bad_buffers_dbg_;
  int  pause_ms_;

  std::string source_dir_;
  std::list<std::string> files_;
  uint64_t expected_rbuf_items_;

  static std::string buffer_to_string(const std::list<uint32_t>&);

  Spill get_spill();

  static CFileDataSource *open_EVT_file(std::string);
  static uint64_t num_of_evts(CFileDataSource *);

};

}

#endif
