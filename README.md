Papas README and examples
=======

UNDER Developement and Testing

PAPAS (PAramatrized PArticle Simulation) is a simulation of the particle flow. It propagates stable generated particles through a simple detector model.

In the tracker, charged particles may be detected as tracks, taking into account the acceptance, efficiency, and momentum resolution of this detector.

In the calorimeters, particles are detected as energy deposits. The energy deposits are modelled by taking into account the following detector properties: energy resolution, acceptance, energy thresholds, and characteristic size. The latter is defined as the distance between two clusters below which the two clusters cannot be resolved and are considered as a single cluster.

A particle flow algorithm then runs over the simulated tracks and clusters to identify and reconstruct charged hadrons, photons, and neutral hadrons. These particles can then be used as an input to higher-level algorithms like jet clustering, or directly in the analysis.

 Electrons and muons are passed through PAPAS without any modification, and the user is responsible for applying is own efficiency and resolution models. The hadronic decay products of tau leptons are simulated just like other hadrons and photons.

A CMS-like detector model is provided as an example, and used in example_simple.cpp.

Installing required software
--
If you're working on lxplus, all the necessary software has been preinstalled for you and you can just proceed with installing the fcc-physics package.

The other supported operating systems are ubuntu 14, macos X, and slc6 (other than lxplus nodes).

You need to install:

- The FCC packages needed to produce and read events in the FCC event data model (EDM):

    - podio: a package allowing to define event data models and to manipulate EDM events.
    - fcc-edm: definition of the FCC EDM, based on podio

- You may also find it useful to have

    - pythia8


### Installing the Papas package

--

Before installing and everytime you want to use this software on lxplus, set up your environment:

    source /afs/cern.ch/exp/fcc/sw/0.7/init_fcc_stack.sh
    source init.sh
    
On other systems make sure you have PODIO, FFCEDM and ROOT environment variables set up.


Compilation
--

    mkdir build
    cd build
    cmake -DCMAKE_INSTALL_PREFIX=../install ../
    make -j 4
    make install
    cd ..

DOxygen Documentation
--
  
   make doc

Test
--
Rudimentary tests that run a simple example and run unit tests can be run with

    cd build
    make test


Running
Examples can be found in the examples directory

To run example_simple:

(1) create a Pythia example.root file eg by following:-

https://twiki.cern.ch/twiki/bin/view/FCC/FccSoftwareHeppy

(2) Then run Papas on this file

    example_simple example.root

This should produce text outputs and also jgp display files.


You should get a printout like:

    Generated Stable Particles
    Particle :p206     :17179869390: pdgid =    22, status =   1, q =  0, pt =   0.9, e =   0.9, eta = -0.13, theta = -0.13, phi =  0.08, mass =  0.00
    Particle :p202     :17179869386: pdgid =   211, status =   1, q =  1, pt =   1.0, e =   1.0, eta = -0.21, theta = -0.21, phi = -2.72, mass =  0.14
    Particle :p201     :17179869385: pdgid =    22, status =   1, q =  0, pt =   0.3, e =   0.3, eta = -0.44, theta = -0.43, phi =  0.69, mass =  0.00
    ...
    Particle :p88      :17179869272: pdgid =  -211, status =   1, q = -1, pt =   0.8, e =   0.8, eta =  0.10, theta =  0.10, phi = -2.56, mass =  0.14

    Reconstructed Particles
    Particle :r678     :21474837158: pdgid =   211, status =   1, q =  1, pt =   0.6, e =   0.6, eta = -0.24, theta = -0.24, phi = -2.94, mass =  0.14
    Particle :r677     :21474837157: pdgid =  -211, status =   1, q = -1, pt =   0.8, e =   0.9, eta =  0.30, theta =  0.29, phi = -2.44, mass =  0.14
    Particle :r676     :21474837156: pdgid =   211, status =   1, q =  1, pt =   1.0, e =   1.0, eta = -0.21, theta = -0.21, phi = -2.72, mass =  0.14
    ...
    Particle :r631     :21474837111: pdgid =    22, status =   1, q =  0, pt =  16.1, e =  16.5, eta = -0.23, theta = -0.23, phi = -0.01, mass =  0.00

jpg files like:

TODO work out how to add in an image
![Alt text](/Users/alice/fcc/papas/papas_cc/doc/file0.pdf?raw=true "Optional Title")


The example will also produce an output root file containing the reconstructed particles

          simpleeg.root

For Mac Xcode project use 
--

mkdir xbuild
cd xbuild
cmake -G Xcode ../
make

