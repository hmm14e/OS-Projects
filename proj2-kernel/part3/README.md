## Part 3 - Elevator Scheduler Spec

Your task is to implement a scheduling algorithm for a hotel elevator. Your elevator must track the number of passengers and the total weight. Elevator load consists of four types of people: adults, children, room service, and bellhops:
* An adult counts as 1 passenger unit and 1 weight unit
* A child counts as 1 passenger unit and ½ weight unit
* Room service counts as 2 passenger units and 2 weight units
* A bellhop counts as 2 passengers unit and 3 weight unit

Passengers will appear on a floor of their choosing and always know where they wish to go. You can assume most of the time when a passenger is on a floor other than the first, they will choose to go to the first floor (for optimization purposes). Passengers board the elevator in FIFO order. If a passenger can fit, the elevator must accept them unless the elevator is moving in the opposite direction from where they wish to go. Once they board the elevator, they may only get off when the elevator arrives at the destination. Passengers will wait on floors to be serviced indefinitely.

### Step 1: Kernel Module with an Elevator
Develop a representation of an elevator. In this project, you will be required to support having a maximum load of 15 weight units and 10 passenger units (neither can be exceeded at any point). The elevator must wait for 2.0 seconds when moving between floors, and it must wait for 1.0 seconds whileloading/unloading passengers. The building has floor 1 being the minimum floor number and floor 10 being the maximum floor number. New passengers can arrive at any time and each floor needs to support an arbitrary number of them.

### Step 2: Add System Calls
Once you have a kernel module, you must modify the kernel by adding three system calls. These calls will be used by a user-space application to control your elevator and create passengers. You need to assign the system calls the following numbers:
* 323 for start_elevator()
* 324 for issue_request()
* 325 for stop_elevator()

`int start_elevator(void)`
Description: Activates the elevator for service. From that point onward, the elevator exists and will begin to service requests. This system call will return 1 if the elevator is already active, 0 for a successful elevator start, and -ERRORNUM if it could not initialize (e.g. -ENOMEM if it couldn’t allocate memory). Initialize an elevator as follows:
* State: IDLE
* Current floor: 1
* Current load: 0 passenger units, 0 weight units


`int issue_request(int passenger_type, int start_floor, int destination_floor)`
Description: Creates a passenger of type passenger_type at start_floor that wishes to go to destination_floor. This function returns 1 if the request is not valid (one of the variables is out of range), and 0 otherwise. A passenger type can be translated to an int as follows:
* Adult = 1
* Child = 2
* Room service = 3
* Bellhop = 4

`int stop_elevator(void)`
Description: Deactivates the elevator. At this point, this elevator will process no more new requests (that is, passengers waiting on floors). However, before an elevator completely stops, it must offload allof its current passengers. Only after the elevator is empty may it be deactivated (state = OFFLINE). This function returns 1 if the elevator is already in the process of deactivating, and 0 otherwise.

### Step 3: /Proc
The module must provide a proc entry named /proc/elevator. Here, you will need to print the following (each labeled appropriately):
* The elevator's movement state:
    * OFFLINE: when the module is installed but the elevator isn’t running (initial state)
    * IDLE: elevator is stopped on a floor because there are no more passengers to service
    * LOADING: elevator is stopped on a floor to load and unload passengers
    * UP: elevator is moving from a lower floor to a higher floor
    * DOWN: elevator is moving from a higher floor to a lower floor
* The current floor the elevator is on
* The next floor the elevator intends to service
* The elevator's current load (in terms of both passengers units and weight units)

You will also need to print the following for each floor of the building:
* The load of the waiting passengers
* The total number of passengers that have been serviced


### Step 4: Test
Once
Once  you've implemented your system calls, you must interact with two provided user-space applications that will allow communication with your kernel module.
**producer.c**
This program will issue N random requests, specified by input.

**consumer.c** \<--start | --stop>
This program expects one flag and on argument:
    * If the flag is --start, then the program must start the elevator
    * If the flag is --stop, then the program must stop the elevator

**producer.c** and **consumer.c** will be provided to you.

---------------------------------------------------------

## Implementation Details

### Main parts
* `Passenger` implementation
    * Implements a simple struct to hold the *passenger_type* and *destination_floor*.
    * Load information (units and weight) are located in a global lookup table.
* `Elevator` implementation
    * Implements the logic to move between floors, load and unload passengers from/to a floor.
    * Implements `elevator_run` -- threaded function in charge of the scheduling (currently the [SCAN algortihm](https://en.wikipedia.org/wiki/Elevator_algorithm))
* `Floor` implementation
    * Implements the logical representation of a single floor, which is in essence a FIFO queue, its corresponding lock, and some metric variables.
* ProcFS functions
    * Implements the handlers to writing status to the /proc/elevator file.
    * When /proc/elevator is read, the elevator and floor status are written to the file.
* Module functions
    * Implements the initialization and teardown logic of the module.
    * On initialization, the `Elevator` and `Floor` array variables are initialized.
### Scheduling
* When the elevator doesn't have anyone in it, its status is set to *IDLE*
* In the `elevator_run` loop, there are two cases:
   * The elevator is IDLE, which in this case the elevator will
      * Do a search through the floors looking for someone to service, if someone is found we move to that a floor, pick them up, and start a scan in their destination direction
      * If no one is found, just call `schedule` (voluntarily give control to scheduler) and wait
   * The elevator is UP or DOWN, which in this case the elevator will
      * Unload all passengers who are at their destination floor.
         * If we have unloaded everybody, then we are done with the current scan, and can start a new one
         * Else we continue with the current scan
 * This scheduling is very naive, for a few reasons
   * It doesn't take advantage of the fact that most on people on floor 2..N are going to floor 1
   * It doesn't take into account clusters of requests
   * When starting a new scan, it starts looking for people starting at floor 1 .. N, rather than close to where it already is
### Notes
* There is no floating point arithmetic allowed in kernel-mode, so handling fractional weight units (child 0.5) is tricky. Our implementation uses an internal variable on `Floor` and `Elevator`, `load_in_weight_half`, to keep track of whether there is an even or odd number of children.
* It wasn't very clear to us when exactly to acquire mutexes in our implementation.
   * For example, when a function ends up calling other functions, sometimes the lock will be acquired in the caller and the lock is implicit in the callees, and other times it will be acquired in the callees.
* When `stop_elevator` is called for the first time, the `elevator_run` thread will be stopped and a new thread will be spawned for running `elevator_unload_all`.
   * Since unloading is a potentially long operation (~20 seconds), we do not want to block successive `stop_elevator` system calls, so we just put it in a thread and let it do its thing.
* When a request is issued with the same *start_floor* and *end_floor*, the passenger isn't entered into the queue and the passengers served for the floor is incremented.

### TODO
* ~~Handle fractional load units~~
* Handle `mutex_lock_interruptible` signals (i.e. when it doesn't return 0)
* Handle `floor_create` allocation failure
* Test thoroughly

