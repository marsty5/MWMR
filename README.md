MWMR
====

This project was pursued as my dissertation, during my bachelors in Computer Science department at University of Cyprus.

Description:<br>
Implementation and evaluation of a service that emulates multi-writers, multi-readers (MWMR) survivable atomic registers in a distributed message-passing system.
I implemeted two algorithms; <br>
(a) The SWMR (single-writer multi-readers) which already existed at that time. <br>
(b) The MWMR (multi-writer multi-readers) which was my one of my main contributions. <br>
I compared the two implementations by running a variety of experiments on PlanetLab.

Files<br>
For the SWMR; use the sreader.c, fwriter.c and fserver.c <br>
For the MWMR; use the freader.c, fwriter.c, fserver.c <br>
(Difference in 2 algorithms is the reader file)

Supporting file<br>
uploadScr.sh was written and used for parallel execution of the code in different machines on PlanetLab.
