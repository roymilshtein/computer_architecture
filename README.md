# CPU Simulator with Tomasulo Algorithm

This project simulates a CPU using the Tomasulo Algorithm, allowing configurable functional units, reservation stations, and unit delays.

## Project Goals
The main objective is to implement a CPU simulator where:
- The number of functional units and reservation stations are configurable.
- Units' delays can be set via configuration files.

## Testing
Three test folders (`Test1`, `Test2`, `Test3`) containing:
- Input files: `cfg.txt`, `memin.txt`
- Output files: `regout.txt`, `traceinst.txt`, `tracecdb.txt`

Each test focuses on specific scenarios to validate the simulator's correctness.

## Simulator Workflow
### Initialization
1. Extract variables from `cfg.txt`.
2. Read instructions from `memin.txt`.
3. Initialize free functional units and build reservation stations.
4. Initialize instruction queue and registers.

### While Loop (Each Iteration = 1 Clock Cycle)
1. **Fetch Stage**: Fetch up to 2 instructions if slots are available and `halt` is not fetched.
2. **Issue Stage**: Assign instructions to reservation stations.
3. **Execute Stage**: Execute ready instructions in available functional units.
4. **Write Result Stage**: Write results to CDB, update registers, and clear stations.

### Closing and Stopping
1. Print output files (`regout.txt`, `traceinst.txt`, `tracecdb.txt`).
2. Free allocated memory and exit.

## Key Definitions
- **Registers**: 16
- **Memory Lines**: 4096
- **Instruction Queue Size**: 16
- **Tag Size**: 6 (3 letters + 2 digits + null terminator)

## Structures
### Instruction
Contains fields such as `opcode`, `dst`, `src0`, `src1`, `pc`, `result`, `tag`, `raw_inst`, and clock cycle markers (`clk_fetched`, `clk_issued`, `clk_start`, `clk_end`, `clk_write`).

### Reservation Station
Fields include `name`, `pc`, `busy`, `opcode`, `Vj`, `Vk`, `Qj`, `Qk`, `num_func_unit`, and `index`.

### Functional Unit
Holds `delay` and `busy` status.

### Instruction Queue
Contains an array of instructions and `front` and `rear` counters.

### Register
Holds `value`, `tag`, and `valid` status.

### CDB (Common Data Bus)
Fields include `busy`, `data`, `tag`, `dest`, and `cdb_type`.

### Trace CDB
Stores data for each CDB write including `cycle`, `pc`, `cdb_type`, `data`, and `tag`.

## Global Variables
- `regs[NUM_OF_REGS]`: Register status array.
- `inst_arr[MAX_MEMIN_LINES]`: Instruction array from `memin.txt`.
- `instruction_queue`: Instruction queue.
- `trace_cdb[MAX_MEMIN_LINES]`: Trace CDB items array.
- Functional units and reservation stations arrays for ADD, MUL, and DIV operations.
- Three CDBs: `add_cdb`, `mul_cdb`, and `div_cdb`.

## Helper Functions
- `safe_file_open`: Opens a file and checks for errors.
- `find_res_station`: Finds an available reservation station.
- Queue management functions: `queue_is_full`, `queue_is_empty`, `dequeue`, `enqueue_inst_q`.
- `is_ready_for_execution`: Checks if a station has the values needed for execution.
- `compare_pc`: Compares PCs to determine execution order.
- `find_free_unit`: Finds a free functional unit by type.
- additional helper functions are shown in the documentation file

---

This README provides a concise overview of the CPU Simulator project. For detailed implementation and further information, refer to the code and documentation within the project repository.
