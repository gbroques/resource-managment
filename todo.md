# TODO
* ~~Increment clock~~
* ~~Terminate after 2 real life seconds~~
* ~~Assign number of instances [1, 10] to each resource~~
* ~~Assign 3 to 5 as shareable~~
* Get 1 process requesting, releasing, and allocating resources
* Fork multiple children at random times
  * Between 1 and 500 ms of logical clock

# USER
* ~~Add parameter specifying bound for when a process should request / let go of a resource~~
* ~~Roll a random number from 0 to that bound when the process should request or release a resource it already has~~
* Make the request through shared memory
* Enter a loop and keep checking to see if request is granted
* ~~Every 0 to 250 ms check if the the process should terminate~~
* If so, deallocate all resources by communicating to master that it's releasing all of those resources
* Make sure the probability is low enough for churn to happen
