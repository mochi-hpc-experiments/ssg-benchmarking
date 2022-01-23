# SSG Benchmarking

The programs in this project require [ssg](https://github.com/mochi-hpc/mochi-ssg)
 version 0.5.0 or newer (There was a major SSG API chage in 0.5.0 December 2021)

## Launch/Observe
- start up one or more SSG providers (ssg-launch-group)
- benchmark times how long it takes to observe that group with ever-more
  observers.  Will also report fastest, slowest, and provide a crude histogram
  of times.

### Metrics

Each of the following reports average, minimum, and maximum across all
client ranks in the ssg-observe-group program:

- `init`: elapsed time to initialize client; includes `MPI\_init()`,
  `margo\_init()`, and `ssg\_init()`
- `load`: time to execute `ssg\_group\_id\_load()` to read group information
  from file
- `observe`: time to execute `ssg\_group\_refresh()` to retrieve state of
  servers

There is also a histogram that shows 5 bins (automatically scaled) with
count of how many ranks fell into each time range for the `observe` step.
