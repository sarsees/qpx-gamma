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
 *      Polynomial - 
 *
 ******************************************************************************/

#include "polynomial.h"

#include <sstream>
#include <iomanip>
#include <numeric>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
//#include "custom_logger.h"
#include "qpx_util.h"

std::string Polynomial::to_string() const
{
  std::string ret = type() + " = ";
  std::string vars;
  int i = 0;
  for (auto &c : coeffs_) {
    if (i > 0)
      ret += " + ";
    ret += c.second.name();
    if (c.first > 0)
      ret += "*(x-xoffset)";
    if (c.first > 1)
      ret += "^" + std::to_string(c.first);
    i++;
    vars += "     " + c.second.to_string() + "\n";
  }
  vars += "     " + xoffset_.to_string();

  ret += "   rsq=" + boost::lexical_cast<std::string>(chi2_) + "    where:\n" +  vars;

  return ret;
}

std::string Polynomial::to_UTF8(int precision, bool with_rsq) const
{
  std::string calib_eqn;
  int i = 0;
  for (auto &c : coeffs_) {
    if (i > 0)
      calib_eqn += " + ";
    calib_eqn += to_str_precision(c.second.value().value(), precision);
    if (c.first > 0) {
      if (xoffset_.value().value())
        calib_eqn += "(x-" + to_str_precision(xoffset_.value().value(), precision) + ")";
      else
        calib_eqn += "x";
    }
    if (c.first > 1)
      calib_eqn += UTF_superscript(c.first);
    i++;
  }

  if (with_rsq)
    calib_eqn += std::string("   r")
        + UTF_superscript(2)
        + std::string("=")
        + to_str_precision(chi2_, precision);

  return calib_eqn;
}

std::string Polynomial::to_markup(int precision, bool with_rsq) const
{
  std::string calib_eqn;

  int i = 0;
  for (auto &c : coeffs_) {
    if (i > 0)
      calib_eqn += " + ";
    calib_eqn += to_str_precision(c.second.value().value(), precision);
    if (c.first > 0) {
      if (xoffset_.value().value())
        calib_eqn += "(x-" + to_str_precision(xoffset_.value().value(), precision) + ")";
      else
        calib_eqn += "x";
    }
    if (c.first > 1)
      calib_eqn +=  "<sup>" + UTF_superscript(c.first) + "</sup>";
    i++;
  }

    if (with_rsq)
    calib_eqn += "   r<sup>2</sup>"
        + std::string("=")
        + to_str_precision(chi2_, precision);

  return calib_eqn;
}


double Polynomial::eval(double x) const
{
  double x_adjusted = x - xoffset_.value().value();
  double result = 0.0;
  for (auto &c : coeffs_)
    result += c.second.value().value() * pow(x_adjusted, c.first);
  return result;
}


double Polynomial::derivative(double x) const
{
  Polynomial new_poly;  // derivative not true if offset != 0
  new_poly.xoffset_ = xoffset_;

  for (auto &c : coeffs_) {
    if (c.first != 0) {
      new_poly.add_coeff(c.first - 1,
                         c.second.lower() * c.first,
                         c.second.upper() * c.first,
                         c.second.value().value() * c.first);
    }
  }
  return new_poly.eval(x);
}
