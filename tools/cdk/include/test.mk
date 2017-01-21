# This file is the generic Makefile for .test directories in component libraries.
# The makefile for all unit test directories (ending in .test in component libraries)

$(if $(wildcard $(OCPI_CDK_DIR)),,$(error OCPI_CDK_DIR environment variable not set properly.))

include $(OCPI_CDK_DIR)/include/util.mk

ifneq ($(Model),test)
  $(error This file, in $(Cwd), must only be used in .test directories)
endif

# We need the project dir because remote system mount dirs point to the project
$(call OcpiIncludeProject,error)

export Cases
export KeepSimulations
# This is to allow the spec to be found and any protocols it depends on
# FIXME: These lines should be shared better - they are a copy
ifeq ($(filter prepare run runtests runonly verify verifyonly view,$(MAKECMDGOALS)),)
override XmlIncludeDirsInternal:=\
  $(call Unique,\
    . $(GeneratedDir) \
    $(IncludeDirs) $(XmlIncludeDirs) \
    $(XmlIncludeDirsInternal) \
    ../lib/$(Model)\
    ../specs \
    $(OcpiXmlComponentLibraries) \
    $(foreach d,$(subst :, ,$(OCPI_XML_INCLUDE_PATH)),$(wildcard $d)) \
    $(foreach d,$(OcpiGetProjectPath),$(wildcard $d/specs)) \
    $(OCPI_CDK_DIR)/lib/components/hdl\
    $(OCPI_CDK_DIR)/lib/components/$(Model)\
    $(OCPI_CDK_DIR)/lib/components \
    $(OCPI_CDK_DIR)/specs \
   )
endif
# Primary goals for this Makefile, with "build" being the default (all)
.PHONY: generate build prepare run runonly verify verifyonly

# Compatility
.PHONY: test tests runtests generated
# "make test" is a synonym for "making the tests", which is "build", which is the default
test: build
tests: build
# runtests is the high level UI goal.  Inside here it is "run"
all: build
generated: generate


# This is the representative result file that says things are properly generated.
CASERPT:=gen/cases.txt

# This is the input file describing this test suite
TESTXML:=$(CwdName)-test.xml

$(CASERPT): $(TESTXML)
	$(AT)echo ========= Generating test assemblies, inputs and applications.
	$(AT)$(OcpiGen) -v -T $< || ($(RM) -r -f gen; exit 1)
	$(AT)chmod a+x gen/applications/*.sh
generate: $(CASERPT)

.PHONY: testxml
$(TESTXML):
	$(AT)[ -f $@ ] || echo '<tests/>' > $@

# This is the target to build test-related artifacts using tools
build: generate
	$(AT)[ ! -d gen/assemblies ] || $(MAKE) -C gen/assemblies

# Prepare to run by looking for available containers and generating run scripts for the
# current platform environment - this is context/platform sensitiive
prepare:
	$(AT)echo Preparing for execution on available platforms with available built workers and assemblies.
	$(AT)$(OCPI_CDK_DIR)/scripts/testrunprep.sh $(call FindRelative,$(OCPI_PROJECT_DIR),$(CURDIR))

runonly:
	$(AT)echo Executing tests on available or specified platforms:
	$(AT)if [ ! -e run/runtests.sh ]; then \
	       echo Execution has not been prepared.  Use make prepare. ;\
	       exit 1; \
	     fi
	$(AT)./run/runtests.sh run

run: prepare runonly

runtests: prepare
	$(AT)echo Running and verifying test outputs on available platforms: 
	$(AT)./run/runtests.sh run verify $(and $(View),view)

verifyonly:
	$(AT)echo Verifying test outputs on available platforms: 
	$(AT)./run/runtests.sh verify $(and $(View),view)

view:
	$(AT)echo View test outputs on available platforms: 
	$(AT)./run/runtests.sh view

verify: run verifyonly

clean:
	$(AT)rm -r -f gen run