#!/usr/bin/python
# This file is protected by Copyright. Please refer to the COPYRIGHT file
# distributed with this source distribution.
#
# This file is part of OpenCPI <http://www.opencpi.org>
#
# OpenCPI is free software: you can redistribute it and/or modify it under the
# terms of the GNU Lesser General Public License as published by the Free
# Software Foundation, either version 3 of the License, or (at your option) any
# later version.
#
# OpenCPI is distributed in the hope that it will be useful, but WITHOUT ANY
# WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
# A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more
# details.
#
# You should have received a copy of the GNU Lesser General Public License along
# with this program. If not, see <http://www.gnu.org/licenses/>.

# TODO: integegrate more inline with ocpirun -A o get information instead of metadata file

from lxml import etree as ET
import os
import sys
import subprocess
import hdltargets

def addLibs(curRoot, libs):
    allWorkersLocal = False
    libraryDirectories = []
    for a in libs:
        if a in ["lib","gen","doc"]:
            continue
        if a == "specs":
            allWorkersLocal = True
            continue

        if a.endswith(".hdl") or a.endswith(".rcc") or a.endswith(".test"):
            allWorkersLocal = True
        else:
            libraryDirectories.append(a)

    # Opencpi Rules - either the components directory is
    # the library directory or if has library sub directories.
    if allWorkersLocal:
        ET.SubElement(curRoot, "library", {"name" : "components"})
    else:
        for libDir in libraryDirectories:
            ET.SubElement(curRoot, "library", {"name" : libDir})
def addSpecs(curRoot, curDir):
    for dirName, subdirList, fileList in os.walk(curDir):
        if (dirName.endswith("/specs")):
            for a in fileList:
                worker = ET.SubElement(curRoot, "spec")
                worker.set('name', a)

def getWorkerPlatforms(dirName, name):
    buildFile = dirName + "/" + name.split('.', 1)[0] + "-build.xml"
    #print buildFile

def checkBuiltApp (appName, appDir):
    fileList = os.listdir(appDir)
    targets = []
    appList = set([appName + ".cxx", appName + ".cpp", appName + ".cc"])
    if (1 == len(appList.intersection(fileList))):
        for fileName in fileList:
            if fileName.startswith("target"):
                fileName = fileName.split('-')
                if len(fileName) == 5 :
                    targets.append(fileName[3])
                elif len(fileName) == 2 :
                    targets.append(fileName[1])
                elif len(fileName) == 3 :
                    targets.append(fileName[2])
                else:
                    targets.append(fileName[2])
        retVal = targets
    else:
        retVal = ["N/A"]
    return retVal

def checkBuilt (primDir):
    fileList = os.listdir(primDir)
    targets = []
    for fileName in fileList:
        if fileName.startswith("target"):
            fileName = fileName.split('-')
            if len(fileName) == 2 :
                targets.append(fileName[1])
    retVal = targets
    return retVal

def checkBuiltAssy (assyName, workerDir):
    fileList = os.listdir(workerDir)
    platforms = []
    containers = []
    fileList_noxml = [x.strip('.xml') for x in fileList]
    fileList_noxml = [x for x in fileList_noxml if not x.startswith("container-")]
    platformList = [plat_obj.name for plat_obj in hdltargets.HdlPlatform.all()]
    for fileName in fileList:
        if fileName.startswith("container-"):
            foundContainer = False;
            for x in fileList_noxml:
                if fileName.endswith(x):
                    #print "adding to containers : " + x + " for " + fileName
                    containers.append(x)
                    foundContainer = True
                    break
            if (not foundContainer):
                #print "adding to containers : base for " + fileName
                containers.append("base")
            for x in platformList:
                if x in fileName:
                    platforms.append(x)
                    break
            #print "filename is " + fileName
    #print "platfroms" + str(platforms)
    #print "containers" + str(containers)
    retVal = zip(platforms, containers)
    return retVal

def checkBuiltWorker (workerDir):
    fileList = os.listdir(workerDir)
    targets = []
    configs = []
    for fileName in fileList:
        if fileName.startswith("target"):
            fileName = fileName.split('-')
            if len(fileName) == 5 :
                targets.append(fileName[3])
                configs.append(fileName[1])
            elif len(fileName) == 2 :
                targets.append(fileName[1])
                configs.append("0")
            elif len(fileName) == 3 :
                targets.append(fileName[2])
                configs.append(fileName[1])
            else:
                targets.append(fileName[2])
                configs.append("0")
    retVal = zip(targets, configs)
    return retVal

def addWorkers(curRoot, workers, dirName):
    workersET = ET.SubElement(curRoot, "workers")
    testsET = ET.SubElement(curRoot, "tests")
    specsET = ET.SubElement(curRoot, "specs")
    addSpecs(specsET, dirName)
    for a in workers:
        if(not a.endswith(".test") and (a not in ["lib","specs","gen","doc","include"])):
            built = checkBuiltWorker(dirName + '/' + a)
            worker = ET.SubElement(workersET, "worker")
            worker.set('name', a)
            for targetStr, configStr in built:
                target = ET.SubElement(worker, "built")
                target.set('target', targetStr)
		target.set('configID', configStr)
            # plat = getWorkerPlatforms(dirName + "/" + a, a)
        elif(a.endswith(".test") and (a not in ["lib","specs","gen","doc","include"])):
            test = ET.SubElement(testsET, "test")
            test.set('name', a)

def dirIsLib(dirName, comps):
    for name in comps.findall("library"):
        if (dirName.endswith(name.get("name"))):
            return (True, name)
    return (False, [])

def addApplications (root, apps):
    for a in apps:
        built = checkBuiltApp(a, dirName + '/' + a)
        app = ET.SubElement(root, "application")
        app.set('name', a)
        for targetStr in built:
                target = ET.SubElement(app, "built")
                target.set('target', targetStr)

def addPlatforms (root, plats, dirName):
    for a in plats:
        if(a not in ["lib"]):
            built = checkBuilt(dirName + '/' + a)
            plat = ET.SubElement(root, "platform")
            plat.set('name', a)
            for targetStr in built:
                target = ET.SubElement(plat, "built")
                target.set('target', targetStr)

def addAssemblies (root, assys, dirName):
    for a in assys:
        built = checkBuiltAssy(a, dirName + '/' + a)
        #print "assemblies dir is: " + str(built)
        app = ET.SubElement(root, "assembly")
        app.set('name', a)
        for platformStr, containerStr in built:
                target = ET.SubElement(app, "built")
                target.set('platform', platformStr)
                target.set('container', containerStr)

def addPrimitives (root, primitives, dirName):
    for a in primitives:
        if a == "lib":
            continue
        built = checkBuilt(dirName + '/' + a)
        prim = ET.SubElement(root, "primitive")
        prim.set('name', a)
        for targetStr in built:
                target = ET.SubElement(prim, "built")
                target.set('target', targetStr)

def isStale (myDir, force):
    retVal = True
    find_output = ""
    if (force == False):
        find_output = subprocess.Popen(['find', myDir, "-name", "*.xml", '-newer', myDir + "/project.xml", "-quit"], stdout=subprocess.PIPE, stderr=subprocess.PIPE).communicate()[0]

    if (find_output != ""):
        retVal = False

    return retVal

def getProjDir(myDir, force ):
    currentDir = myDir
    if (force == True):
        return currentDir
    loopNum = 0;
    while (loopNum < 10):
        if (os.path.isfile(currentDir + "/Makefile")):
            with open(currentDir + "/Makefile", 'r') as myfile:
                data=myfile.read().replace('\n', '')
            if (data.find("include $(OCPI_CDK_DIR)/include/project.mk") > -1) :
                return currentDir
        currentDir = currentDir + "/../"
        loopNum = loopNum + 1
    print "Not in a Project Directory, Check the directory passed to the script"
    sys.exit(0)

# main
if len(sys.argv) < 2 :
    print("ERROR: need to specify the path to the project")
    sys.exit(1)

if ((len(sys.argv) == 3) and (sys.argv[2] == "force")):
    force = True
else:
    force = False

mydir = sys.argv[1]
mydir = getProjDir(mydir, force)

if (isStale(mydir, force)):
    # Get the project name, add it as an attribute in the project element.
    strings = mydir.split("/")
    splitLen = strings.__len__()
    if splitLen == 1:
        projectName = strings[0]
    else:
        projectName = strings[splitLen -1]

    root = ET.Element("project", {"name" : projectName})

    comps = ET.SubElement(root, "components")
    hdl = ET.SubElement(root, "hdl")
    assys = ET.SubElement(hdl, "assemblies")
    prims = ET.SubElement(hdl, "primitives")

    for dirName, subdirList, fileList in os.walk(mydir):
        if "exports" in dirName:
            continue
        elif dirName.endswith("/components"):
            addLibs(comps, subdirList)
        elif dirName.endswith("/applications"):
            apps = ET.SubElement(root, "applications")
            addApplications(apps, subdirList)
        elif dirName.endswith("/platforms"):
            platforms = ET.SubElement(hdl, "platforms")
            addPlatforms(platforms, subdirList, dirName)

        elif dirName.endswith("/cards"):
            hdlLibs = hdl.findall("libraries")
            if hdlLibs.__len__() == 0:
                hdlLibs = [ET.SubElement(hdl, "libraries")]
            cards = ET.SubElement(hdlLibs[0], "library")
            cards.set('name', "cards")
            addWorkers(cards, subdirList, dirName)

        elif dirName.endswith("/devices"):
            if "/platforms/" not in dirName:
                # This is the hdl/devices directory
                hdlLibs = hdl.findall("libraries")
                if hdlLibs.__len__() == 0:
                    hdlLibs = [ET.SubElement(hdl, "libraries")]
                devs = ET.SubElement(hdlLibs[0], "library")
                devs.set('name', "devices")
                addWorkers(devs, subdirList, dirName)
            else:
                # this is a devices directory under a platform
                dirSplit = dirName.split('/')
                platName = [];
                index = range(0, len(dirSplit)-1)
                for i, sub in zip(index, dirSplit):
                    if (sub == "platforms"):
                        platName = dirSplit[i+1]
                platformsEl =   hdl.findall("platforms")
                if  platformsEl.__len__() >0:
                    plats = platformsEl[0]
                    platformTag = "platform[@name='"+platName+"']"
                    plat = plats.findall(platformTag)
                    if plat.__len__() > 0:
                        devs = ET.SubElement(plat[0], "library")
                        devs.set('name',"devices")
                        addWorkers(devs, subdirList, dirName)

        elif dirName.endswith("hdl/assemblies"):
            addAssemblies(assys ,subdirList, dirName)
        elif dirName.endswith("hdl/primitives"):
            addPrimitives(prims, subdirList, dirName)

        retVal = dirIsLib(dirName, comps)
        if (retVal[0]):
            addWorkers(retVal[1], subdirList, dirName)

    print "Updating project metadata..."
    myFile = open(mydir+"/project.xml", 'w')
    myFile.write(ET.tostring(root,pretty_print=True))
else:
    print("metadata is not stale, not regenerating")
sys.exit(0)
