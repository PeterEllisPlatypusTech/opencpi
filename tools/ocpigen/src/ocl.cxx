/*
 *  Copyright (c) Mercury Federal Systems, Inc., Arlington VA., 2009-2011
 *
 *    Mercury Federal Systems, Incorporated
 *    1901 South Bell Street
 *    Suite 402
 *    Arlington, Virginia 22202
 *    United States of America
 *    Telephone 703-413-0781
 *    FAX 703-413-0784
 *
 *  This file is part of OpenCPI (www.opencpi.org).
 *     ____                   __________   ____
 *    / __ \____  ___  ____  / ____/ __ \ /  _/ ____  _________ _
 *   / / / / __ \/ _ \/ __ \/ /   / /_/ / / /  / __ \/ ___/ __ `/
 *  / /_/ / /_/ /  __/ / / / /___/ ____/_/ / _/ /_/ / /  / /_/ /
 *  \____/ .___/\___/_/ /_/\____/_/    /___/(_)____/_/   \__, /
 *      /_/                                             /____/
 *
 *  OpenCPI is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published
 *  by the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  OpenCPI is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with OpenCPI.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>
#include <assert.h>
#include <ctype.h>
#include <sys/time.h>
#include "OCL_Worker.h"
#include "OcpiUtilCppMacros.h"
#include "OcpiUtilWorker.h"
#include "OcpiUtilMisc.h"
#include "wip.h"
#include "assembly.h"
#include "rcc.h"

namespace OU = OCPI::Util;
// Generate the readonly implementation file.
// What implementations must explicitly (verilog) or implicitly (VHDL) include.
#define HEADER ".h"
#define OCLIMPL "-worker"
#define SOURCE ".cl"
#define OCLENTRYPOINT "_entry_point"

class OclPort : public RccPort {
public:
  OclPort(Worker &w, ezxml_t x, DataPort *sp, int ordinal, const char *&err);
};

static const char *
upperdup(const char *s) {
  char *upper = (char *)malloc(strlen(s) + 1);
  for (char *u = upper; (*u++ = (char)toupper(*s)); s++)
    ;
  return upper;
}

static const char *oclTypes[] = {"none",
  "OCLBoolean", "OCLChar", "OCLDouble", "OCLFloat", "int16_t", "int32_t", "uint8_t",
  "uint32_t", "uint16_t", "int64_t", "uint64_t", "OCLChar" };

#if 0
static void
printMember(FILE *f, OU::Member *t, const char *prefix, size_t &offset, unsigned &pad)
{
  size_t rem = offset & (t->m_align - 1);
  if (rem)
    fprintf(f, "%s  char         pad%u_[%zu];\n",
	    prefix, pad++, t->m_align - rem);
  offset = OU::roundUp(offset, t->m_align);
  if (t->m_isSequence) {
    fprintf(f, "%s  uint32_t     %s_length;\n", prefix, t->m_name.c_str());
    if (t->m_align > sizeof(uint32_t))
      fprintf(f, "%s  char         pad%u_[%zu];\n",
	      prefix, pad++, t->m_align - (unsigned)sizeof(uint32_t));
    offset += t->m_align;
  }
  fprintf(f, "%s  %-12s %s", prefix, oclTypes[t->m_baseType], t->m_name.c_str());
  if (t->m_baseType == OA::OCPI_String)
    fprintf(f, "[%zu]", OU::roundUp(t->m_stringLength + 1, 4));
  if (t->m_isSequence || t->m_arrayRank)
    fprintf(f, "[%zu]", t->m_sequenceLength);
  fprintf(f, "; // offset %zu, 0x%zx\n", offset, offset);
  offset += t->m_nBytes;
}

static const char *
methodName(Worker *w, const char *method, const char *&mName) {
  const char *pat = w->m_pattern ? w->m_pattern : w->m_staticPattern;
  if (!pat) {
    mName = method;
    return 0;
  }
  size_t length =
    strlen(w->m_implName) + strlen(method) + strlen(pat) + 1;
  char c, *s = (char *)malloc(length);
  mName = s;
  while ((c = *pat++)) {
    if (c != '%')
      *s++ = c;
    else if (!*pat)
      *s++ = '%';
    else switch (*pat++) {
      case '%':
	*s++ = '%';
	break;
      case 'm':
	strcpy(s, method);
	while (*s)
	  s++;
	break;
      case 'M':
	*s++ = (char)toupper(method[0]);
	strcpy(s, method + 1);
	while (*s)
	  s++;
	break;
      case 'w':
	strcpy(s, w->m_implName);
	while (*s)
	  s++;
	break;
      case 'W':
	*s++ = (char)toupper(w->m_implName[0]);
	strcpy(s, w->m_implName + 1);
	while (*s)
	  s++;
	break;
      default:
	return OU::esprintf("Invalid pattern rule: %s", w->m_pattern);
      }
  }
  *s++ = 0;
  return 0;
}
#endif

/*
  FIXME
  1. add support for local memory.
     a. Get local memory sizes from metadata
     b. Add local memory structures to the worker context

*/

const char *Worker::
emitImplOCL() {
  const char *err;
  FILE *f;
  if ((err = openOutput(m_fileName.c_str(), m_outDir, "", OCLIMPL, HEADER, m_implName, f)))
    return err;
  fprintf(f, "/*\n");
  printgen(f, " *", m_file.c_str());
  fprintf(f, " */\n");
  const char *upper = upperdup(m_implName);
  fprintf(f,
          "\n"
          "/* This file contains the implementation declarations for OCL worker %s */\n\n"
          "#ifndef OCL_WORKER_%s_H__\n"
          "#define OCL_WORKER_%s_H__\n\n"
          "#include <OCL_Worker.h>\n\n",
          m_implName, upper, upper);

  const char *last;
  if (m_ports.size()) {
    fprintf(f,
	    "/*\n"
	    " * Enumeration of port ordinals for worker %s (for port masks)\n"
	    " */\n"
	    "typedef enum {\n",
	    m_implName);
    last = "";
    for (unsigned n = 0; n < m_ports.size(); n++) {
      Port *port = m_ports[n];
      fprintf(f, "%s  %s_%s", last, upper, upperdup(port->cname()));
      // FIXME TWO WAY
      last = ",\n";
    }
    fprintf(f, "\n} %c%sPort;\n", toupper(m_implName[0]), m_implName+1);
  }
  if (m_ctl.nRunProperties < m_ctl.properties.size()) {
    fprintf(f,
	    "/*\n"
	    " * Definitions for default values of parameter properties.\n");
    fprintf(f,
	    " * Parameters are defined as macros with no arguments to catch spelling errors.\n");
    fprintf(f,
	    " */\n");
    for (PropertiesIter pi = m_ctl.properties.begin(); pi != m_ctl.properties.end(); pi++) {
      OU::Property &p = **pi;
      if (p.m_isParameter) {
	std::string value;
	fprintf(f,
		"#ifndef PARAM_%s\n"
		"#define PARAM_%s() (%s)\n"
		"#endif\n",
		p.m_name.c_str(), p.m_name.c_str(), rccPropValue(p, value));
      }
    }
  }
  if (m_ctl.nRunProperties) {
    fprintf(f,
            "/*\n"
            " * Property structure for worker %s\n"
            " */\n"
            "typedef struct {\n",
            m_implName);
    unsigned pad = 0;
    size_t offset = 0;
    bool isLastDummy = false;
    for (PropertiesIter pi = m_ctl.properties.begin(); pi != m_ctl.properties.end(); pi++)
      if (!(*pi)->m_isParameter) {
	std::string type;
	rccMember(type, **pi, 2, offset, pad, m_implName, true, isLastDummy, false, false);
	fputs(type.c_str(), f);
      }
    fprintf(f, "} %c%sProperties;\n\n", toupper(m_implName[0]), m_implName + 1);
  }
  fprintf(f,
          "/*\n"
          " * Worker kernel arg structure for worker %s\n"
          " */\n"
          "typedef struct {\n"
	  "  OCL_SELF\n",
	  m_implName);
  for (unsigned n = 0; n < m_ports.size(); n++)
    fprintf(f, "  OCLPort %s;\n", m_ports[n]->cname() );
  fprintf(f,
          "} %c%sWorker;\n",
          toupper(m_implName[0]), m_implName + 1);
  fprintf(f,
          "/*\n"
          " * Worker persistent structure for worker %s\n"
          " */\n"
	  "typedef struct {\n"
	  "  OCLReturned      returned;\n"
	  "  %c%sWorker        self;\n",
	  m_implName, toupper(m_implName[0]), m_implName + 1);
  assert(!(sizeof(OCPI::OCL::OCLReturned) & 7));
  if (m_ctl.nRunProperties) {
    fprintf(f,"  %c%sProperties properties;\n",
            toupper(m_implName[0]), m_implName + 1);
    if (m_ctl.sizeOfConfigSpace & 7)
      fprintf(f, "  uint8_t pad0[(sizeof(%c%sProperties)+7)&~7];\n",
            toupper(m_implName[0]), m_implName + 1);
  }
  if (m_localMemories.size())
    for (unsigned n = 0; n < m_localMemories.size(); n++) {
      LocalMemory* mem = m_localMemories[n];
      fprintf(f, "  uint8_t %s[%zu];\n", mem->name, (mem->sizeOfLocalMemory+7)&~7);
    }
  fprintf(f,
          "} %c%sPersist;\n\n",
          toupper(m_implName[0]), m_implName + 1);

  fprintf(f,
	  "\n"
	  "OCLResult %s_run(%c%sWorker *self",
	  m_implName, toupper(m_implName[0]), m_implName + 1);
  if (m_ctl.nRunProperties)
    fprintf(f, ", __global %c%sProperties *properties", toupper(m_implName[0]), m_implName + 1);
  fprintf(f, ");\n");
  
  for (unsigned n = 0; n < m_ports.size(); n++)
    m_ports[n]->emitRccCImpl(f);
  for (unsigned n = 0; n < m_ports.size(); n++)
    m_ports[n]->emitRccCImpl1(f);
  fprintf(f,
          "\n"
          "#endif /* ifndef OCL_WORKER_%s_H__ */\n",
	  upper);
  if (fclose(f))
    return "File close for writing failed.  No space?";
  return 0;
}

const char* Worker::
emitSkelOCL() {
  const char *err;
  FILE *f;
  if ((err = openOutput(m_fileName.c_str(), m_outDir, "", "_skel", ".cl", NULL, f)))
    return err;
  fprintf(f, "/*\n");
  printgen(f, " *", m_file.c_str(), true);
  fprintf(f, " *\n");
  fprintf(f,
	  " * This file contains the OCL implementation skeleton for worker: %s\n"
	  " */\n\n"
	  "#include \"%s-worker.h\"\n\n"
	  "/*\n"
	  " * Required work group size for worker %s run() function.\n"
	  " */\n"
	  "#define OCL_WG_X 1\n"
	  "#define OCL_WG_Y 1\n"
	  "#define OCL_WG_Z 1\n\n"
	  "/*\n"
	  " * Methods to implement for worker %s, based on metadata.\n"
	  " */\n",
	  m_implName, m_implName, m_implName, m_implName);
  unsigned op = 0;
  const char **cp;
  const char *mName;
  for (cp = OU::Worker::s_controlOpNames; *cp; cp++, op++)
    if (m_ctl.controlOps & (1 << op)) {
      if ((err = rccMethodName(*cp, mName)))
	return err;
      fprintf(f,
	      "\n"
	      "OCLResult %s_%s(%c%sWorker *self)\n{\n"
	      "\n  (void)self;\n"
	      "  return OCL_OK;\n"
	      "}\n",
	      m_implName,
	      mName,
        toupper(m_implName[0]),
        m_implName + 1 );
    }

  const size_t pad_len = 14 + strlen ( m_implName ) + 3;
  char pad [ pad_len + 1 ];
  memset ( pad, ' ', pad_len );
  pad [ pad_len ] = '\0';

  if ((err = rccMethodName("run", mName)))
    return err;
  fprintf(f,
	  "\n"
	  "OCLResult %s_run(%c%sWorker *self",
	  m_implName, toupper(m_implName[0]), m_implName + 1);
  if (m_ctl.nRunProperties)
    fprintf(f, ", __global %c%sProperties *properties", toupper(m_implName[0]), m_implName + 1);
  fprintf(f,
	  ") {\n"
	  "  (void)self;\n"
	  "  return OCL_ADVANCE;\n"
	  "}\n");
  if (fclose(f))
    return "Error closing file for writing.  No space?";
  return emitEntryPointOCL();
}

const char *Worker::
emitEntryPointOCL() {
  const char* err;
  FILE* f;
  if ((err = openOutput(m_fileName.c_str(), m_outDir, "", OCLENTRYPOINT, SOURCE, m_implName, f)))
    return err;
  unsigned pad = OCPI_UTRUNCATE(unsigned, 18 + strlen(m_implName));
  fprintf(f,
	  "#ifndef OCL_START_PORT\n"
	  "#define OCL_START_PORT\n"
	  "static void startPort(OCLPort *p, __global uint8_t *base) {\n"
	  "  __global OCLHeader *h = (__global OCLHeader *)(base + p->readyOffset+8);\n"
          "  p->current.data = h->m_data ? (__global uint8_t*)h + h->m_data : 0;\n"
	  "  p->current.length = h->m_length;\n"
	  "  p->current.opCode = h->m_opCode;\n" // not needed for output
	  "  p->current.end = h->m_eof;\n"       // not needed for output
	  "  p->current.header = h;\n"
	  "}\n"
	  "#endif\n"
	  "/*\n"
	  " * This generated kernel dispatches to the worker's methods\n"
	  " * This single kernel/function dispatches the run method and control operations.\n"
	  " */\n"
	  "__kernel __attribute__((reqd_work_group_size(OCL_WG_X, OCL_WG_Y, OCL_WG_Z)))\n"
	  "void %s_kernel(__global %c%sPersist *persist",
	  m_implName, toupper(m_implName[0]), m_implName+1);
  for (unsigned n = 0; n < m_ports.size(); n++)
    fprintf(f, ",\n%*s__global uint8_t *buffers%u", pad, "", n);
  fprintf(f,
	  ") {\n");
  if (m_ports.size()) {
    fprintf(f,
	    "  __global uint8_t *bases[%zu] = {", m_ports.size());
    for (unsigned n = 0; n < m_ports.size(); n++)
      fprintf(f, "%sbuffers%u", n ? ", " : "", n);
    fprintf(f,
	    "};\n");
  }
  fprintf(f, "%c%sWorker self = persist->self;\n", toupper(m_implName[0]), m_implName+1);
  fprintf(f,
	  "  if (self.logLevel >= 10)\n"
	  "    printf(\"OpenCL Kernel running op: %%d count: %%d nd: %%d %%p %%p %%p\\n\",\n"
	  "           self.controlOp, self.runCount, get_work_dim(), persist, buffers0, buffers1);\n"
	  "  switch (self.controlOp) {\n"
          "  case OCPI_OCL_RUN:\n");
  fprintf(f,
	  "    while (self.runCount--) {\n");
  if (m_ports.size())
    fprintf(f,
	    "      OCLPort *p = &self.%s;\n"
	    "      for (unsigned n = 0; n < self.nPorts; n++, p++)\n"
	    "        startPort(p, bases[n]);\n",
	    m_ports[0]->m_name.c_str());
      fprintf(f,
	      "      persist->returned.result = %s_run(&self%s);\n",
	      m_implName, m_ctl.nRunProperties ? ", &persist->properties" : "");
  if (m_ports.size())
    fprintf(f,
	    "      p = &self.%s;\n"
	    "      for (unsigned n = 0; n < self.nPorts; n++, p++)\n"
	    "        if (p->isOutput) {\n"
	    "          __global OCLHeader *h = p->current.header;\n"
	    "          if (p->current.data == 0)\n"
	    "             h->m_data = 0;\n"
	    "          h->m_length = p->current.length;\n"
	    "          h->m_opCode = p->current.opCode;\n"
	    "          h->m_eof = p->current.end;\n"
	    "        }\n"
	    "      if (self.runCount) {\n"
	    "          p = &self.%s;\n"
	    "          for (unsigned n = 0; n < self.nPorts; n++, p++)\n"
	    "            if ((p->readyOffset += p->bufferStride) >= p->endOffset)\n"
	    "              p->readyOffset = 0;\n"
	    "      }\n",
	    m_ports[0]->m_name.c_str(), m_ports[0]->m_name.c_str());
  fprintf(f,
	  "    }\n"
	  "    break;\n");
  unsigned op = 0;
  const char* mName;
  for (const char** cp = OU::Worker::s_controlOpNames; *cp; cp++, op++)
    if (m_ctl.controlOps & (1 << op ))
      if ((err = rccMethodName (*cp, mName)))
        return err;
      else {
	const char* mUname = upperdup(mName);
	fprintf(f,
		"  case OCPI_OCL_%s:\n"
		"    persist->returned.result = %s_%s(&self);\n"
		"    break;\n",
		mUname, m_implName, mName);
      }
  fprintf(f,
	  "  default:;\n"
	  "  }\n"
	  "}\n\n");
  if (fclose(f))
    return "Could not close output file. No space?";
  return 0;
}

/*
 * What implementation-specific attributes does an OCL worker have?
 * And which are not available at runtime?
 * And if they are indeed available at runtime, do we really retreive them from the
 * container or just let the container use what it knows?
 */
const char * Worker::
parseOcl() {
  const char *err;
  if ((err = OE::checkAttrs(m_xml,  IMPL_ATTRS, "language", (void*)0)) ||
      (err = OE::checkElements(m_xml, IMPL_ELEMS, "port", (void*)0)))
    return err;
  const char *lang = ezxml_cattr(m_xml, "Language");
  if (lang && strcasecmp(lang, "cl"))
    return OU::esprintf("For an OCL worker, language \"%s\" is invalid", lang);
  ezxml_t xctl;
  if ((err = parseSpec()) ||
      (err = parseImplControl(xctl, NULL)) ||
      (xctl && (err = OE::checkAttrs(xctl, GENERIC_IMPL_CONTROL_ATTRS, (void *)0))) ||
      (err = parseImplLocalMemory()))
    return err;
  // Parse data port implementation metadata: maxlength, minbuffers.
  DataPort *sp;
  for (ezxml_t x = ezxml_cchild(m_xml, "Port"); x; x = ezxml_next(x))
    if ((err = checkDataPort(x, sp)) || !createDataPort<OclPort>(*this, x, sp, -1, err))
      return err;
  for (unsigned i = 0; i < m_ports.size(); i++)
    m_ports[i]->finalizeOclDataPort();
  m_model = OclModel;
  m_modelString = "ocl";
  m_reusable = true;
  m_language = C;
  m_baseTypes = oclTypes;
  return NULL;
}

// This is a parsed for the assembly of what does into a single worker binary
const char *Worker::
parseOclAssy() {
  const char *err;
  ::Assembly *a = m_assembly = new ::Assembly(*this);
  m_model = OclModel;
  m_modelString = "ocl";

  static const char
    *topAttrs[] = {IMPL_ATTRS, NULL},
    *instAttrs[] = {INST_ATTRS, NULL};
  // Do the generic assembly parsing, then to more specific to OCL
  if ((err = a->parseAssy(m_xml, topAttrs, instAttrs, true, m_outDir)))
    return err;
  return NULL;
}

OclPort::
OclPort(Worker &w, ezxml_t x, DataPort *sp, int ordinal, const char *&err)
  : RccPort(w, x, sp, ordinal, err) {
}

const char *DataPort::
finalizeOclDataPort() {
  const char *err = NULL;
  if (type == WDIPort)
    createDataPort<OclPort>(worker(), NULL, this, -1, err);
  return err;
}
