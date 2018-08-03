# lcgeo (Linear Collider Geometry)
[![Build Status](https://travis-ci.org/iLCSoft/lcgeo.svg?branch=master)](https://travis-ci.org/iLCSoft/lcgeo)
[![Coverity Scan Build Status](https://scan.coverity.com/projects/12359/badge.svg)](https://scan.coverity.com/projects/ilcsoft-lcgeo)

Implementation of Linear Collider detector models in DD4hep.

lcgeo is distributed under the [GPLv3 License](http://www.gnu.org/licenses/gpl-3.0.en.html)

[![License](https://www.gnu.org/graphics/gplv3-127x51.png)](https://www.gnu.org/licenses/gpl-3.0.en.html)

## Requirements
DD4hep built with Geant4 and LCIO
## Download and Installation
### Download
  * `git clone https://github.com/iLCSoft/lcgeo.git`
### Remove Unneeded ILD Drivers
  * `rm lcgeo/detector/calorimeter/SHcal*`
  * `rm lcgeo/detector/calorimeter/SEcal*`
  * `rm lcgeo/detector/CaloTB/CaloPrototype*`
  * `mkdir lcgeo/build`
### Setup Environment
  * `source /cvmfs/sft.cern.ch/lcg/releases/gcc/4.9.3/x86_64-slc6/setup.sh`
  * `source /cvmfs/ilc.desy.de/sw/x86_64_gcc49_sl6/v02-00-01/init_ilcsoft.sh`
  * `cd lcgeo/build`
### Prepare Build Environment
  * `cmake -DCMAKE_CXX_COMPILER=$(which g++) -DCMAKE_C_COMPILER=$(which gcc\) -C /cvmfs/ilc.desy.de/sw/x86_64_gcc49_sl6/v02-00-01/ILCSoft.cmake ..`
  * `cmake -DCMAKE_CXX_COMPILER=$(which g++) -DCMAKE_C_COMPILER=$(which gcc\) -DILCUTIL_DIR=/cvmfs/ilc.desy.de/sw/x86_64_gcc49_sl6/v02-00-01/ILCSoft.cmake -C $ILCSOFT/ILCSoft.cmake ..`
### Build
  * `make -j4`
  * `make install`
### (Don't) Initialize
Running this command will cause ddsim to not work.
  * `source ../bin/thislcgeo.sh`

## Session Initialisation
The following commands must be run at the beginning of every session. From the lcgeo directory:
  * `source /cvmfs/sft.cern.ch/lcg/releases/gcc/4.9.3/x86_64-slc6/setup.sh`
  * `source /cvmfs/ilc.desy.de/sw/x86_64_gcc49_sl6/v02-00-01/init_ilcsoft.sh`
  
## Running some examples

### Create single particle input file
Modify the python script `./example/lcio_particle_gun.py` in order to create
an LCIO input file with single particles:
  * `python lcio_particle_gun.py`
  * `python example/lcio_particle_gun.py`

There is also example input file with 10 singe muons: ./mcparticles.slcio

### Run the simulation:
  * `ddsim --compactFile=SiD/compact/SiD_o1_v01/SiD_o1_v03.xml --runType=batch --inputFile mcparticles.slcio -N=1 --outputFile=testSiD_o1_v03.slcio`

This creates an lcio file testSiD_o1_v03.slci with sim hits and MCParticles.

You can look at it with:

  * `anajob simple_lcio.slcio`
or 
  * `dumpevent simple_lcio.slcio 1`

Change the ddsim command line parameters as needed to read other input files.

## Run digitisation and reconstruction with Marlin
Using gear files taken from the Glasgow site (https://twiki.ppe.gla.ac.uk/bin/view/LinearCollider/GlaSiDGettingStarted):

  * `wget https://twiki.ppe.gla.ac.uk/pub/LinearCollider/GlaSiDGettingStarted/SiDReconstruction_v03_160711.xml`
  * `wget https://twiki.ppe.gla.ac.uk/pub/LinearCollider/GlaSiDGettingStarted/gear_sid.xml`
  * `Marlin SiDReconstruction_v03_160711.xml`  

  
(Instructions below haven't been tested)  
## Event displays:
There are several ways for visualizing the detector geometry and the simulated events:

1) CED event display (requires freeglut to be installed)
   `ced2go -d gear_ILD_o1_v05_ORG.xml -t ced2go-template.xml simple_lcio.slcio`

2) teveDisplay
   `ln -s simple_lcio.slcio teve_infile.slcio`
   `teveDisplay ../ILD/compact/ILD_o1_v05/ILD_o1_v05.xml`

3) DDEve:
   `root`
   `.x $DD4hepINSTALL/examples/DDEve/DDEve.C()`
and then load  
   `../ILD/compact/ILD_o1_v05/DDEve.xml`

## Running the reconstruction with Marlin (EXPERIMENTAL)
 1) create a gear file from the DD4hep detector model, e.g.
    `convertToGear default ../trunk/ILD/compact/ILD_o1_v05.xml gear_ILD_o1_v05_dd4hep.xml`

    [ Friendly reminder: in order to work you need to explicitly build DD4hep with GEAR enabled. Use the flag `-DDD4HEP_USE_GEAR="ON"` for the cmake command. ]

    Note: currently this might still be incomplete and you can copy missing information for example from the original Mokka gear files ( or use `$DDSIM/example/gear_ILD_o1_v05_dd4hep.xml` )

 2) run simulation (as described above), e.g.:
    `python ddsim.py ../ILD/compact/ILD_o1_v05/ILD_o1_v05.xml`

 3) run the standard reconstruction ( tracking only so far ):
    `Marlin ../example/ild_dd4hep_stdreco.xml`  
    `--global.LCIOInputFiles=./simple_lcio.slcio`
    `--MyLCIOOutputProcessor.LCIOOutputFile=./simple_lcio_REC.slcio`

 4) look at the result w/ the CED event display, e.g.
    `ced2go -d ../example/gear_ILD_o1_v05_ORG.xml -t ../example/ced2go-template.xml simple_lcio_REC.slcio`

## License and Copyright
Copyright (C), lcgeo Authors

lcgeo is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License long with this program.  If not, see <http://www.gnu.org/licenses/>.
