# SSG Benchmarking

The programs in this project require [ssg](https://github.com/mochi-hpc/mochi-ssg)
 version 0.5.0 or newer (There was a major SSG API chage in 0.5.0 December 2021)

## Launch/Observe
- start up one or more SSG providers (ssg-launch-group)
- benchmark times how long it takes to observe that group with ever-more
  observers.  Will also report fastest, slowest, and provide a crude histogram
  of times.
