# Resource Management
A resource management module for an operating system simulator.


## How to Build and Run
1. Clone or download the project

Within the root of the project:

2. Run `make`
3. Run `oss`

## Arbritration Rule
When the system deadlocks, we use a **LIFO** policy to determine which process to kill first.

## Arguments
```
 -h  Show help.
 -v  Specify verbose log output
 -l  Specify the log file. Defaults to 'oss.out'.
 -b  Specify the upper bound for when processes should request or release a resource.
 ```

Read `cs4760Assignment4Fall2017Hauschild.pdf` for more details.
