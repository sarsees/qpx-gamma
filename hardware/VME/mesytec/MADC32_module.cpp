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
 *      MADC32
 *
 ******************************************************************************/

#include "MADC32_module.h"
#include "custom_logger.h"
#include "vmecontroller.h"

#define MADC32_Firmware                   0x0203

#define Const(name) static const int name =

static const int eventBuffer  = 0x0000;
static const int ReadoutReset = 0x6034;

namespace Qpx {

static DeviceRegistrar<MADC32> registrar("VME/MADC32");

MADC32::MADC32() {
  m_controller = nullptr;
  m_baseAddress = 0;
  status_ = DeviceStatus::loaded | DeviceStatus::can_boot;
  module_firmware_code_ = MADC32_Firmware;
}


MADC32::~MADC32() {
  this->die();
}


bool MADC32::daq_init() {

  //for scaler
  Gamma::Setting reset_counters("VME/MADC32/reset_ctr_ab");
  reset_counters.enrich(setting_definitions_, true);
  m_controller->write16(m_baseAddress + reset_counters.metadata.address, AddressModifier::A32_UserData, (uint16_t)2);

  //the rest
  Gamma::Setting reset("VME/MADC32/readout_reset");
  reset.enrich(setting_definitions_, true);
  m_controller->write16(m_baseAddress + reset.metadata.address, AddressModifier::A32_UserData, (uint16_t)1);

  Gamma::Setting freset("VME/MADC32/FIFO_reset");
  freset.enrich(setting_definitions_, true);
  m_controller->write16(m_baseAddress + freset.metadata.address, AddressModifier::A32_UserData, (uint16_t)0);

  Gamma::Setting st("VME/MADC32/start_acq");
  st.enrich(setting_definitions_, true);
  m_controller->write16(m_baseAddress + st.metadata.address, AddressModifier::A32_UserData, (uint16_t)1);

}

void MADC32::rebuild_structure(Gamma::Setting &set) {
  for (auto &k : set.branches.my_data_) {
    if ((k.metadata.setting_type == Gamma::SettingType::stem) && (k.id_ == "VME/MADC32/ChannelThresholds")) {

      Gamma::Setting temp("VME/MADC32/Threshold");
      temp.enrich(setting_definitions_, true);

      while (k.branches.size() < 32)
        k.branches.my_data_.push_back(temp);
      while (k.branches.size() > 32)
        k.branches.my_data_.pop_back();

      uint32_t offset = k.metadata.address;

      std::set<int32_t> indices;
      int address = 0;
      for (auto &p : k.branches.my_data_) {
        if (p.id_ != temp.id_) {
          temp.indices = p.indices;
          p = temp;
        }

        p.metadata.address = offset + address * 2;
        p.metadata.name = "Threshold " + std::to_string(address);

        for (auto &i : p.indices)
          indices.insert(i);

        ++address;
      }
    }
  }
}



void MADC32::addReadout(VmeStack& stack, int style = 0)
{
  if (style == 0) {
    //single event

    //  Add instructions to read out the ADC for a event. Since we're only working in
    //  single even tmode, we'll just read 'too many' words and let the
    //  BERR terminate for us.  This ensures that we'll have that 0xfff at the end of
    //  the data.
    //  stack  - The VMUSB readout stack that's being built for this stack.

    stack.addFifoRead32(m_baseAddress + eventBuffer, AddressModifier::A32_UserBlock, (size_t)45);
    stack.addWrite16(m_baseAddress + ReadoutReset, AddressModifier::A32_UserData, (uint16_t)1);
    stack.addDelay(5);

  } else if (style == 1) {
    //scaler

    Gamma::Setting busy_time_lo("VME/MADC32/adc_busy_time_lo");
    busy_time_lo.enrich(setting_definitions_, true);

    Gamma::Setting busy_time_hi("VME/MADC32/adc_busy_time_hi");
    busy_time_hi.enrich(setting_definitions_, true);

    Gamma::Setting time_0("VME/MADC32/time_0");
    time_0.enrich(setting_definitions_, true);

    Gamma::Setting time_1("VME/MADC32/time_1");
    time_1.enrich(setting_definitions_, true);

    stack.addRead16((uint32_t)(m_baseAddress + busy_time_lo.metadata.address), AddressModifier::A32_UserData);
    stack.addRead16((uint32_t)(m_baseAddress + busy_time_hi.metadata.address), AddressModifier::A32_UserData);

    stack.addRead16((uint32_t)(m_baseAddress + time_0.metadata.address), AddressModifier::A32_UserData);
    stack.addRead16((uint32_t)(m_baseAddress + time_1.metadata.address), AddressModifier::A32_UserData);

    // reset the time for incremental scalers....

    Gamma::Setting reset_counters("VME/MADC32/reset_ctr_ab");
    reset_counters.enrich(setting_definitions_, true);

    stack.addWrite16((uint32_t)(m_baseAddress + reset_counters.metadata.address), AddressModifier::A32_UserData, (uint16_t)2);
  } else if (style == 2) {
    /*!
      Create the readout list for the Mesytec chain  This will be a fifo block read at the
      cblt address with a count that is the largest event size * maxwords * num_modules + 1
      That should be sufficient to ensure there's a terminating 0xffffffff
      @param rdolist - CVMUSBReadoutList we are appending our instructions to.
    */

//    uint32_t location = m_pConfig->getUnsignedParameter("-cbltaddress");
//    uint32_t mcast    = m_pConfig->getUnsignedParameter("-mcastaddress");

//    size_t   size     = m_pConfig->getIntegerParameter("-maxwordspermodule");
//    list<string>  modnames;
//    modnames = getModules();
//    size              = size * 36 * (modnames.size() + 1);

//    rdolist.addFifoRead32(location, cbltamod, size);

//    // Broadcast readout_reswet:


//    rdolist.addWrite16(mcast + ReadoutReset, initamod, (uint16_t)1);

  }
}

std::list<Hit> MADC32::parse(std::list<uint32_t> data, uint64_t &evts, std::string &madc_pattern)
{
  std::list<Hit> hits;

  uint64_t events = 0;

  uint64_t headers = 0;
  uint64_t footers = 0;
  uint64_t junk = 0;

  uint32_t header_m   = 0xff008000; // Header Mask
  uint32_t header_c   = 0x40000000; // Header Compare

  uint32_t footer_m      = 0xc0000000; // Footer Mask
  uint32_t footer_c      = 0xc0000000; // Footer Compare
  uint32_t footer_time_m = 0x3fffffff; // Mask for timestamp in footer

  uint32_t evt_mask = 0xffe04000; // event header mask
  uint32_t evt_c    = 0x04000000; // event compare

  uint32_t det_mask = 0x001f0000; // Detector mask
  uint32_t nrg_mask = 0x00001fff; // Energy mask
  uint32_t ovrfl_c  = 0x00004000; // Overflow compare

  uint32_t junk_c  = 0xffffffff; // Overflow compare

  int upshift = 0;

  madc_pattern.clear();

  for (auto &word : data) {
    if (word == junk_c) {
      madc_pattern += "J";
//      junk++;
    } else if ((word & header_m) == header_c) {
      uint32_t module = ((word & 0x00ff0000) >> 16);
      uint32_t resolution = ((word & 0x00007000) >> 12);
      uint32_t words_f = (word & 0x00000fff);
      if ((resolution == 4) || (resolution == 3))
        upshift = 3;
      else if ((resolution == 1) || (resolution == 2))
        upshift = 4;
      else if (resolution == 0)
        upshift = 5;
//                  PL_DBG << "  MADC header module=" << module << "  resolution=" << resolution
//                         << "  words=" << words_f << "  upshift=" << upshift;
      headers++;
      madc_pattern += "H";

    } else if ((word & footer_m) == footer_c) {
      madc_pattern += "F";
      uint32_t timestamp = word & footer_time_m;
      for (auto &h : hits)
        h.timestamp.time_native = timestamp * 5;


      footers++;
//      PL_DBG << "  MADC timestamp: " << itobin32(timestamp);
    } else if ((word & evt_mask) == evt_c) {
      int chan_nr = (word & det_mask) >> 16;
      uint16_t nrg = (word & nrg_mask);
      bool overflow = ((word & ovrfl_c) != 0);
//                  PL_DBG << "  MADC hit detector=" << chan_nr << "  energy=" << nrg << "  overflow=" << overflow;


      Hit one_hit;
      one_hit.channel   = chan_nr;
      one_hit.energy    = nrg << upshift;
      one_hit.timestamp.time_native = (evts + events) * 5;
      one_hit.timestamp.timebase_divider = 75;
      one_hit.timestamp.timebase_multiplier = 1000;

      hits.push_back(one_hit);
      events++;
      madc_pattern += "E";

    } else {
      madc_pattern += "?";

//      PL_DBG << "Unrecognized data in buffer";
    }
  }

//  PL_DBG << "<MADC32> parsed " << madc_pattern;

  if ((headers != 1) || (headers != footers))
    hits.clear();
  else
    evts += events;

  return hits;
}

}
