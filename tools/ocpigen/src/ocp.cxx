/*
 * This file is protected by Copyright. Please refer to the COPYRIGHT file
 * distributed with this source distribution.
 *
 * This file is part of OpenCPI <http://www.opencpi.org>
 *
 * OpenCPI is free software: you can redistribute it and/or modify it under the
 * terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option) any
 * later version.
 *
 * OpenCPI is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more
 * details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <assert.h>
#include "OcpiUtilMisc.h"
#include "ocp.h"
#include "hdl.h"
#include "assembly.h"

namespace OE=OCPI::Util::EzXml;
namespace OU=OCPI::Util;

#undef OCP_SIGNAL_MT
#define OCP_SIGNAL_MT(n, w) {#n, true, true, w, true, false, OCP_##n},
#define OCP_SIGNAL_MTR(n, w) {#n, true, true, w, true, true, OCP_##n},
#undef OCP_SIGNAL_ST
#define OCP_SIGNAL_ST(n, w) {#n, true, false, w, true, false, OCP_##n},
#define OCP_SIGNAL_MS(n) {#n, false, true, 0, false, false, OCP_##n},
#define OCP_SIGNAL_MV(n, w) {#n, true, true, w, false, false, OCP_##n},
#define OCP_SIGNAL_MSR(n) {#n, false, true, 0, false, true, OCP_##n},
#define OCP_SIGNAL_MVR(n, w) {#n, true, true, w, false, true, OCP_##n},
#define OCP_SIGNAL_SS(n) {#n, false, false, 0, false, false, OCP_##n},
#define OCP_SIGNAL_SV(n, w) {#n, true, false, w, false, false, OCP_##n},
OcpSignalDesc ocpSignals [N_OCP_SIGNALS+1] = {
OCP_SIGNALS
{0,0,0,0,0,0,N_OCP_SIGNALS}
};

#undef OCP_SIGNAL_MS
#undef OCP_SIGNAL_MV
#undef OCP_SIGNAL_SS
#undef OCP_SIGNAL_SV

#if 0
static unsigned myfls(uint64_t n) {
  for (int i = sizeof(n)*8; i > 0; i--)
    if (n & ((uint64_t)1 << (i - 1)))
      return i;
  return 0;
}

size_t ceilLog2(uint64_t n) {
  return OCPI_UTRUNCATE(size_t, n ? myfls(n - 1) : 0);
}
size_t floorLog2(uint64_t n) {
  //  ocpiInfo("Floor log2 of %u is %u", n, myfls(n)-1);
  return OCPI_UTRUNCATE(size_t, myfls(n) - 1);
}
#endif

OcpPort::
OcpPort(Worker &w, ezxml_t x, Port *sp, int ordinal, WIPType type, const char *defName,
	const char *&err) 
  : Port(w, x, sp, ordinal, type, defName, err),
    m_values(NULL), m_nAlloc(0), m_impreciseBurst(false), m_preciseBurst(false), m_dataWidth(8),
    m_dataWidthFound(false), m_byteWidth(0), m_bwFound(false), m_continuous(false) {
  memset(&ocp, 0, sizeof(ocp));
  if (err)
    return;
  if (sp) {
    // WHAT CAN YOU SPECIFY HERE IN A SPEC PORT?  We assume nothing
  }
  const char *clockName;
  if ((err = OE::getBoolean(x, "MyClock", &myClock)) ||
      (err = OE::getBoolean(x, "ImpreciseBurst", &m_impreciseBurst)) ||
      (err = OE::getBoolean(x, "Continuous", &m_continuous)) ||
      (err = OE::getExprNumber(x, "DataWidth", m_dataWidth,
			       &m_dataWidthFound, m_dataWidthExpr, &w)) ||
      (err = OE::getNumber(x, "ByteWidth", &m_byteWidth, 0, m_dataWidth)) ||
      (err = OE::getBoolean(x, "PreciseBurst", &m_preciseBurst)))
    return;
  if (myClock && (clockName = ezxml_cattr(x, "clock")))
    err = OU::esprintf("port \"%s\" refers to clock \"%s\", and also has MyClock=true, "
		       "which is invalid", pname(), clockName);
  // We can't create clocks at this point based on myclock, since
  // it might depend on what happens with other ports.
}

// Our special copy constructor
OcpPort::
OcpPort(const OcpPort &other, Worker &w , std::string &name, size_t count, const char *&err)
  : Port(other, w, name, count, err) {
  if (err)
    return;
  if (other.m_values) {
    m_values = new uint8_t[other.m_nAlloc];
    memcpy(m_values, other.m_values, other.m_nAlloc);
  } else
    m_values = NULL;
  m_nAlloc = other.m_nAlloc;
  m_impreciseBurst = other.m_impreciseBurst;
  m_preciseBurst = other.m_preciseBurst;
  m_dataWidth = other.m_dataWidth;
  m_byteWidth = other.m_byteWidth;
  m_continuous = other.m_continuous;
  ocp = other.ocp;
}

void OcpPort::
emitPortDescription(FILE *f, Language lang) const {
  Port::emitPortDescription(f, lang);
  fprintf(f, "  %s WIP attributes for this %s interface are:\n", hdlComment(lang), typeName());
}

void OcpPort::
emitRecordSignal(FILE *f, std::string &last, const char */*prefix*/, bool /*useRecord*/,
		 bool /*inPackage*/, bool inWorker,
		 const char *defaultIn, const char *defaultOut) {
  if (last.size())
    fprintf(f, last.c_str(), ";");
  std::string temp;
  OU::format(temp,
	     "    -- Signals for %s %s port named \"%s\".  See record types above.\n"
	     "    %-*s : in  %s%s_t%s%s",
	     typeName(), masterIn() ? "input" : "output", pname(),
	     (int)m_worker->m_maxPortTypeName, typeNameIn.c_str(), inWorker ? "worker_" : "",
	     typeNameIn.c_str(), defaultIn ? " := " : "", defaultIn ? defaultIn : "");
  if (inWorker ? haveWorkerOutputs() : haveOutputs())
    OU::format(last,
	       "%s;\n"
	       "    %-*s : out %s%s_t%s%s%%s",
	       temp.c_str(),
	       (int)m_worker->m_maxPortTypeName, typeNameOut.c_str(), inWorker ? "worker_" : "",
	       typeNameOut.c_str(), defaultOut ? " := " : "", defaultOut ? defaultOut : "");
  else
    OU::format(last, "%s%%s", temp.c_str());
}

void OcpPort::
emitRecordInterfaceConstants(FILE *f) {
  // Before emitting the record, define the constants for the data path width.
  fprintf(f,
	  "\n"
	  "  -- Constant declarations for parameterized signal widths for port \"%s\"\n", 
	  pname());
  if (ocp.MAddr.value)
    fprintf(f, "  constant ocpi_port_%s_MAddr_width : natural;\n", pname());
  if (ocp.MData.value)
    fprintf(f, "  constant ocpi_port_%s_MData_width : natural;\n", pname());
  if (ocp.MByteEn.value)
    fprintf(f, "  constant ocpi_port_%s_MByteEn_width : natural;\n", pname());
  if (ocp.MDataInfo.value)
    fprintf(f, "  constant ocpi_port_%s_MDataInfo_width : natural;\n", pname());
}

// Determine the expression for the width of an OCP vector signal in the data path
void OcpPort::
vectorWidth(const OcpSignalDesc *osd, std::string &out, Language /*lang*/, bool /*convert*/,
	    bool /*value*/) {
  switch (osd->number) {
  case OCP_MAddr:
    OU::format(out, "ocpi_port_%s_MAddr_width", pname()); break;
  case OCP_MData:
    OU::format(out, "ocpi_port_%s_MData_width", pname()); break;
  case OCP_MByteEn:
    OU::format(out, "ocpi_port_%s_MByteEn_width", pname()); break;
  case OCP_MDataInfo:
    OU::format(out, "ocpi_port_%s_MDataInfo_width", pname()); break;
  default:
    OU::format(out, "%zu", ocp.signals[osd->number].width);
  }
}

void OcpPort::
emitInterfaceConstants(FILE *f, Language lang) {
  if (ocp.MAddr.value)
    emitConstant(f, "ocpi_port_%s_MAddr_width", lang, ocp.MAddr.width);
  if (ocp.MData.value)
    emitConstant(f, "ocpi_port_%s_MData_width", lang, ocp.MData.width);
  if (ocp.MByteEn.value)
    emitConstant(f, "ocpi_port_%s_MByteEn_width", lang, ocp.MByteEn.width);
  if (ocp.MDataInfo.value)
    emitConstant(f, "ocpi_port_%s_MDataInfo_width", lang, ocp.MDataInfo.width);
}

void OcpPort::
emitSignals(FILE *f, Language lang, std::string &last, bool /*inPackage*/, bool /*inWorker*/,
	    bool convert) {
  const char *comment = hdlComment(lang);
  bool mIn = masterIn();
  OcpSignalDesc *osd;
  for (unsigned n = 0; n < m_count; n++) {
    if (!myClock)
      fprintf(f,
	      "  %s No Clk signal here. The \"%s\" interface uses \"%s\" as clock\n",
	      comment, pname(), clock->signal());
    osd = ocpSignals;
    std::string ws;
    for (OcpSignal *os = ocp.signals; osd->name; os++, osd++)
      if (os->master == mIn && /* strcmp(osd->name, "Clk") && */ os->value) {
	vectorWidth(osd, ws, lang, convert);
	emitSignal(os->signal, f, lang, Signal::IN,
		   last, osd->vector ? (int)os->width : -1, n,
		   "", NULL, NULL, osd->vector ? ws.c_str() : NULL);
      }
    osd = ocpSignals;
    for (OcpSignal *os = ocp.signals; osd->name; os++, osd++)
      if (os->master != mIn && /* strcmp(osd->name, "Clk") && */ os->value) {
	vectorWidth(osd, ws, lang, convert);
	emitSignal(os->signal, f, lang, Signal::OUT,
		   last, osd->vector ? (int)os->width : -1, n,
		   "", NULL, NULL, osd->vector ? ws.c_str() : NULL);
      }
  }
}

void OcpPort::
emitVector(FILE *f, const OcpSignalDesc *osd) {
  fprintf(f, "std_logic_vector(");
  std::string wstr;
  vectorWidth(osd, wstr, VHDL, false);
  fprintf(f, "%s-1 downto 0)", wstr.c_str());
}

void OcpPort::
emitDirection(FILE *f,  const char *implName, bool mIn, std::string &dir) {
  fprintf(f,
	  "\n  -- Record for the %s input (OCP %s) signals for port \"%s\" of worker \"%s\"\n",
	  typeName(), mIn ? "master" : "slave", pname(), implName);
  fprintf(f, "  type %s_t is record\n", dir.c_str());
  OcpSignalDesc *osd = ocpSignals;
  for (OcpSignal *os = ocp.signals; osd->name; os++, osd++)
    //      if ((osd->master == mIn && strcmp(osd->name, "Clk")) && os->value) {
    if (os->master == mIn && os->value) {
      fprintf(f, "    %-20s: ", osd->name);
      if (osd->type)
	fprintf(f, "ocpi.ocp.%s_t", osd->name);
      else if (osd->vector)
	emitVector(f, osd);
      else
	fprintf(f, "std_logic");
      fprintf(f, ";\n");
    }
  fprintf(f, "  end record %s_t;\n", dir.c_str());
}

void OcpPort::
emitRecordInterface(FILE *f, const char *implName) {
  bool mIn = masterIn();
  //      emitPortDescription(p, f);
  fprintf(f, "\n"
	  "  -- These 2 records correspond to the input and output sides of the OCP bundle\n"
	  "  -- for the \"%s\" worker's \"%s\" profile interface named \"%s\"\n",
	  implName, typeName(), pname());
  std::string in, out;
  OU::format(in, typeNameIn.c_str(), "");
  OU::format(out, typeNameOut.c_str(), "");
  emitDirection(f, implName, mIn, in);
  emitDirection(f, implName, !mIn, out);
  if (m_count > 1)
    emitRecordArray(f);
}

bool OcpPort::
needsControlClock() const {
  return true;
}

// Post processing for deriveOCP
void OcpPort::
fixOCP() {
  OcpSignal *o = ocp.signals;
  OcpSignalDesc *osd = ocpSignals;
  for (unsigned i = 0; i < N_OCP_SIGNALS; i++, o++, osd++)
    if (o->value || o->width) {
      if (osd->vector) {
	if (osd->width)
	  o->width = osd->width;
      } else
	o->width = 1;
      m_nAlloc += o->width;
    }
  // FIXME:  is m_values actually used for anything anymore???
  m_values = (uint8_t*)calloc(m_nAlloc, 1);
  uint8_t *v = m_values;
  o = ocp.signals;
  osd = ocpSignals;
  for (unsigned i = 0; i < N_OCP_SIGNALS; i++, o++, osd++)
    if (o->value || o->width) {
      o->value = v;
      v += o->width;
    }
}

const char *OcpPort::
deriveOCP() {
  OcpSignal *o = ocp.signals;
  OcpSignalDesc *osd = ocpSignals;
  for (unsigned i = 0; i < N_OCP_SIGNALS; i++, o++, osd++)
    o->master = osd->master;
  static uint8_t s[1]; // a non-zero string pointer
  if (myClock)
    ocp.Clk.value = s;
  return NULL;
}
const char *OcpPort::
doPatterns(unsigned nWip, size_t &maxPortTypeName) {
  const char *err;
  if ((err = Port::doPatterns(nWip, maxPortTypeName)))
    return err;
  if (clock && clock->port == this && clock->m_signal.empty()) {
    std::string sin;
    // ordinal == -2 means suppress ordinal
    if ((err = doPattern(m_count > 1 ? 0 : -2, nWip, true, !masterIn(), sin)))
      return err;
    ocpiCheck(asprintf(&ocp.Clk.signal, "%s%s", sin.c_str(), "Clk") > 0);
    clock->m_signal = ocp.Clk.signal;
  }
  OcpSignalDesc *osd = ocpSignals;
  for (OcpSignal *os = ocp.signals; osd->name; os++, osd++)
    if (os->master == masterIn() && os->value)
      ocpiCheck(asprintf(&os->signal, "%s%s", fullNameIn.c_str(), osd->name) > 0);
  osd = ocpSignals;
  for (OcpSignal *os = ocp.signals; osd->name; os++, osd++)
    if (os->master != masterIn() && os->value)
      ocpiCheck(asprintf(&os->signal, "%s%s", fullNameOut.c_str(), osd->name) > 0);
  return NULL;
}

void OcpPort::
emitVerilogSignals(FILE *f) {
  for (unsigned n = 0; n < m_count; n++) {
    OcpSignalDesc *osd = ocpSignals;
    std::string num;
    OU::format(num, "%u", n);
    std::string name, width;
    for (OcpSignal *os = ocp.signals; osd->name; os++, osd++)
      if (os->master == masterIn() && /* strcmp(osd->name, "Clk")) && */ os->value) {
	OU::format(name, os->signal, num.c_str());
	if (osd->vector) {
	  vectorWidth(osd, width, Verilog);
	  fprintf(f, "  input  [%3s-1:0] %s;\n", width.c_str(), name.c_str());
	} else
	  fprintf(f, "  input            %s;\n", name.c_str());
      }
    osd = ocpSignals;
    for (OcpSignal *os = ocp.signals; osd->name; os++, osd++)
      if (os->master != masterIn() && /* strcmp(osd->name, "Clk")) && */ os->value) {
	OU::format(name, os->signal, num.c_str());
	if (osd->vector) {
	  vectorWidth(osd, width, Verilog);
	  fprintf(f, "  output [%3s-1:0] %s;\n", width.c_str(), name.c_str());
	} else
	  fprintf(f, "  output           %s;\n", name.c_str());
      }
  }
}
void OcpPort::
emitVHDLSignalWrapperPortMap(FILE *f, std::string &last) {
  std::string in, out;
  OU::format(in, typeNameIn.c_str(), "");
  OU::format(out, typeNameOut.c_str(), "");
  OcpSignalDesc *osd = ocpSignals;
  for (OcpSignal *os = ocp.signals; osd->name; os++, osd++)
    if (os->value && os->master) {
      fprintf(f, "%s      %s.%s => %s", last.c_str(),
	      os->master == masterIn() ? in.c_str() : out.c_str(),
	      osd->name, os->signal);
      last = ",\n";
    }
  osd = ocpSignals;
  for (OcpSignal *os = ocp.signals; osd->name; os++, osd++)
    if (os->value && !os->master) {
      fprintf(f, "%s      %s.%s => %s", last.c_str(),
	      os->master == masterIn() ? in.c_str() : out.c_str(),
	      osd->name, os->signal);
      last = ",\n";
    }
}

void OcpPort::
emitVHDLRecordWrapperSignals(FILE *f) {
  for (unsigned n = 0; n < m_count; n++) {
    std::string num;
    OU::format(num, "%u", n);
    std::string in, out;
    OU::format(in, typeNameIn.c_str(), "");
    OU::format(out, typeNameOut.c_str(), "");
    OcpSignalDesc *osd = ocpSignals;
    for (OcpSignal *os = ocp.signals; osd->name; os++, osd++)
      if (os->value) {
	std::string name;
	OU::format(name, os->signal, num.c_str());
	if (os->value) {
	  fprintf(f, "      signal %s : ", name.c_str());
	  if (osd->vector)
	    emitVector(f, osd);
	  else
	    fprintf(f, "std_logic");
	  fprintf(f, ";\n");
	}
      }
  }

}
void OcpPort::
emitVHDLRecordWrapperAssignments(FILE *f) {
  for (unsigned n = 0; n < m_count; n++) {
    std::string num;
    OU::format(num, "%u", n);
    std::string in, out;
    OU::format(in, typeNameIn.c_str(), "");
    OU::format(out, typeNameOut.c_str(), "");
    OcpSignalDesc *osd = ocpSignals;
    for (OcpSignal *os = ocp.signals; osd->name; os++, osd++)
      if (os->value) {
	std::string name;
	OU::format(name, os->signal, num.c_str());
	if (os->value) {
	  std::string rec;
	  if (m_count > 1)
	    OU::format(rec, "%s(%u).%s", os->master == masterIn() ? in.c_str() : out.c_str(),
		       n, osd->name);
	  else
	    OU::format(rec, "%s.%s", os->master == masterIn() ? in.c_str() : out.c_str(),
		       osd->name);
	  fprintf(f, "  %s <= %s;\n",
		  os->master == masterIn() ? name.c_str() : rec.c_str(),
		  os->master == masterIn() ? rec.c_str() : name.c_str());
	}
      }
  }
}
void OcpPort::
emitVHDLRecordWrapperPortMap(FILE *f, std::string &last) {
  for (unsigned n = 0; n < m_count; n++) {
    std::string num;
    OU::format(num, "%u", n);
    std::string in, out;
    OU::format(in, typeNameIn.c_str(), "");
    OU::format(out, typeNameOut.c_str(), "");
    OcpSignalDesc *osd = ocpSignals;
    for (OcpSignal *os = ocp.signals; osd->name; os++, osd++)
      if (os->value) {
	std::string name;
	OU::format(name, os->signal, num.c_str());
	if (os->value) {
	  fprintf(f, "%s      %s => %s", last.c_str(),
		  name.c_str(), name.c_str());
	  last = ",\n";
	}
      }
  }
}
void OcpPort::
emitConnectionSignal(FILE *f, bool output, Language lang, std::string &signal) {
  if (lang == Verilog) {
    // Generate signals when both sides has the signal configured.
    OcpSignalDesc *osd;
    OcpSignal *os;
    bool wantMaster = (m_master && output) || (!m_master && !output);
    for (osd = ocpSignals, os = ocp.signals; osd->name; os++, osd++)
      if (os->master == wantMaster && os->value) {
	fprintf(f, "wire ");
	if (osd->vector)
	  fprintf(f, "[%3zu:0] ", os->width - 1);
	else
	  fprintf(f, "        ");
	fprintf(f, "%s%s;\n", signal.c_str(), osd->name);
      }
  } else {
    std::string tname;
    OU::format(tname, output ? typeNameOut.c_str() : typeNameIn.c_str(), "");
    std::string stype;
    Worker &w = *m_worker;
    // WCI ports on assemblies are always generic generic
    if (m_type == WCIPort && (m_master || w.m_assembly))
      OU::format(stype, "wci.wci_%s_%st", output ? "m2s" : "s2m",
		 m_count > 1 ? "array_" : "");
    else {
      std::string lib(w.m_library);
      w.addParamConfigSuffix(lib);
      OU::format(stype, "%s.%s_defs.%s%s_t", lib.c_str(), w.m_implName, tname.c_str(),
		 m_count > 1 ? "_array" : "");
    }
    if (m_count > 1)
      OU::formatAdd(stype, "(0 to %zu)", m_count - 1);
    // Make master the canonical type?
    fprintf(f,
	    "  signal %s : %s;\n", signal.c_str(), stype.c_str());
  }	       
}

// Emit for one direction, into a string
void OcpPort::
emitPortSignalsDir(FILE *f, bool output, Language lang, const char *indent,
		   bool &any, std::string &comment, std::string &last,
		   OcpAdapt *adapt, Attachments &atts) {
  std::string name;
  OU::format(name, output ? typeNameOut.c_str() : typeNameIn.c_str(), "");
  // only do WCI with individual signals if it is a slave that isn't an assembly
  OcpSignalDesc *osd;
  OcpSignal *os;
  OcpAdapt *oa;
  if (lang == VHDL && atts.size() == 0 && output) {
    doPrev(f, last, comment, hdlComment(lang));
    if (any)
      fputs(indent, f);
    any = true;
    fprintf(f, "%s => open", name.c_str());
    return;
  }
  for (osd = ocpSignals, os = ocp.signals, oa = adapt; osd->name; os++, osd++, oa++) {
    ocpiDebug("Perhaps connecting OCP signal %s value %p master %u os->master %u atts %zu",
	      osd->name, os->value, m_master, os->master, atts.size());
    // If the signal is in the interface
    if (os->value && (output ? os->master == m_master : os->master != m_master)) {
      std::string signal, thisComment;
      connectOcpSignal(*osd, *os, *oa, signal, thisComment, lang, atts);
      /* if (signal.length()) */ {
	// We have a new one, so can close the previous one
	doPrev(f, last, comment, hdlComment(lang));
	if (lang == VHDL) {
	  if (any)
	    fputs(indent, f);
	  fprintf(f, "%s.%s => %s", name.c_str(),
		  //		  p.master == os->master ? out.c_str() : in.c_str(),
		  osd->name, signal.c_str());
	} else {
	  fprintf(f, "  .%s(%s",
		  os->signal, signal.c_str());
	  fprintf(f, ")");
	}
	comment = oa->comment ? oa->comment : thisComment.c_str();
	any = true;
      }
    }
  }
}

void OcpPort::
emitExprAssignments(std::string &out, std::string &signalIn, OcpAdapt *adapt, Attachments &atts) {
  OU::formatAdd(out,
		"\n"
		"  -- Temporary input signal assignments for port \"%s\",\n"
		"  -- since it has some signals with expression values that are not \"globally static\" in VHDL terms.\n",
		pname());
  OcpSignalDesc *osd;
  OcpSignal *os;
  OcpAdapt *oa;
  for (osd = ocpSignals, os = ocp.signals, oa = adapt; osd->name; os++, osd++, oa++)
    if (os->value && os->master != m_master) {
      std::string signal, thisComment;
      connectOcpSignal(*osd, *os, *oa, signal, thisComment, VHDL, atts);
      const char *comment = oa->comment ? oa->comment : thisComment.c_str();
      OU::formatAdd(out, "  %s.%s <= %s;%s%s\n", signalIn.c_str(), osd->name,
		    signal.c_str(), comment[0] ? "-- " : "", comment);
    }
}
void OcpPort::
emitPortSignals(FILE *f, Attachments &atts, Language lang,
		const char *indent, bool &any, std::string &comment, std::string &last,
		const char */*myComment*/, OcpAdapt *adapt, std::string *signalIn,
		std::string &exprs) {
  if (signalIn) {
    std::string signal;
    // ocpSignalPrefix(signal, !master, lang, atts);
    doPrev(f, last, comment, hdlComment(lang));
    if (any)
      fputs(indent, f);
    any = true;
    fprintf(f, "%s => %s", typeNameIn.c_str(), signalIn->c_str());
    emitExprAssignments(exprs, *signalIn, adapt, atts);
  } else
    emitPortSignalsDir(f, false, lang, indent, any, comment, last, adapt, atts);
  emitPortSignalsDir(f, true, lang, indent, any, comment, last, adapt, atts);
}

InstancePort  &OcpPort::
ocpSignalPrefix(std::string &signal, bool master, Language lang, Attachments &atts) {
  // Find the other end of the connection
  assert(atts.size() == 1); // OCP connections are always point-to-point
  Connection &c = atts.front()->m_connection;
  assert(c.m_attachments.size() == 2);
  InstancePort *otherIp = NULL;
  Attachment *at;
  for (AttachmentsIter ai = c.m_attachments.begin(); ai != c.m_attachments.end(); ai++)
    if ((*ai)->m_instPort.m_port != this) {
      at = *ai;
      otherIp = &at->m_instPort;
      break;
    }
  assert(otherIp);
  // Decide on our indexing.  We need an index if our attachment is a subset of
  // what we are connecting to, which is either another internal port or an external one.
  // In either case
  std::string &cName = master ? c.m_masterName : c.m_slaveName;
  size_t index, top, count = 0; // count for indexing purpose
  if (otherIp->m_port->m_count > c.m_count) {
    // We're connecting to something bigger: indexing is needed in this port binding
    count = c.m_count;
    index = at->m_index;
    top = index + count - 1;
  }
  if (count) {
    std::string num, temp1;
    if (lang == Verilog)
      OU::format(num, "%zu", index);
    OU::format(temp1, cName.c_str(), num.c_str());
    if (count > 1) {
      assert(lang == VHDL);
      OU::format(signal, "%s(%zu to %zu)", temp1.c_str(), index, top);
    } else {
      if (lang == VHDL)
	OU::format(signal, "%s(%zu)", temp1.c_str(), index);
      else
	signal = temp1;
    }
  } else
    OU::format(signal, cName.c_str(), "");
  return *otherIp;
}

void OcpPort::
connectOcpSignal(OcpSignalDesc &osd, OcpSignal &os, OcpAdapt &oa,
		 std::string &signal, std::string &thisComment, Language lang,
		 Attachments &atts) {
  if (atts.empty()) {
    // A truly unconnected port.  All we want is a tieoff if it is an input
    // We can always use zero since that will assert reset
    if (os.master != m_master)
      if (lang == VHDL)
	signal = osd.vector ? "(others => '0')" : "'0'";
      else
	OU::format(signal, "%zu'b0", os.width);
    else if (lang == VHDL)
      signal = "open";
    return;
  }
  std::string temp;
  InstancePort &otherIp = ocpSignalPrefix(temp, os.master, lang, atts);
  if (lang == VHDL)
    temp += '.';
  if (oa.expr) {
    std::string other;
    if (oa.other != N_OCP_SIGNALS)
      temp += ocpSignals[oa.other].name;
    if (!strcmp(oa.expr, "open")) {
      static size_t unused; // can this really be processed scoped?
      if (os.width > 1)
	OU::formatAdd(signal, "unused(%zu to %zu)", unused, unused + os.width - 1);
      else
	OU::formatAdd(signal, "unused(%zu)", unused);
      unused += os.width;
    } else
      OU::formatAdd(signal, oa.expr, temp.c_str());
  } else {
    signal = temp + osd.name;
    OcpPort &other = *static_cast<OcpPort*>(otherIp.m_port);
    if (osd.vector && os.width != other.ocp.signals[osd.number].width) {
      ocpiDebug("Narrowing of assignment to port %s of worker %s from %zu to %zu",
		pname(), m_worker->m_implName, other.ocp.signals[osd.number].width, os.width);
      OU::formatAdd(signal, lang == Verilog ? "[%zu-1:0]" : "(%zu-1 downto 0)",
		    os.width);
      thisComment = "worker is narrower than external, which is OK";
    }
  }
}

const char *OcpPort::
resolveExpressions(OCPI::Util::IdentResolver &ir) {
  if (m_dataWidthExpr.length()) {
    const char *err = parseExprNumber(m_dataWidthExpr.c_str(), m_dataWidth, NULL, &ir);
    if (err)
      return err;
    m_dataWidthExpr.clear();
  }
  return Port::resolveExpressions(ir);
}

